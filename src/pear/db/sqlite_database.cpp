#include <pear/db/sqlite_database.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

#include "schema.hpp"
#include "sqlite.hpp"

namespace pear::db {

using pear::net::DeviceUpdateInfo;
using pear::net::FileUpdateInfo;
using pear::net::WalEntryInfo;

namespace {

// op_type в контракте — обычный int. Фиксируем константы тут,
// чтобы не таскать магические числа по коду.
constexpr int kOpFileUpdate = 0;
constexpr int kOpDeviceUpdate = 1;

// Ключи для таблицы local_config.
constexpr std::string_view kCfgMasterAddress = "master_address";
constexpr std::string_view kCfgDeviceId = "device_id";

}  // namespace

// ---- ctor / dtor ----

SqliteDatabase::SqliteDatabase(const std::filesystem::path& db_path)
    : conn_(std::make_unique<Connection>(db_path)) {
    ensure_schema(*conn_);
}

SqliteDatabase::~SqliteDatabase() = default;

// ---- WAL ----

std::vector<WalEntryInfo> SqliteDatabase::getWalEntriesSince(uint64_t last_seq_id) {
    std::vector<WalEntryInfo> out;
    auto st = conn_->prepare(R"sql(
        SELECT seq_id, timestamp, op_type,
               file_id, file_name, file_version, file_owner_device_id,
               device_id, device_address
        FROM wal
        WHERE seq_id > ?1
        ORDER BY seq_id ASC;
    )sql");
    st.bind(1, last_seq_id);
    while (st.step()) {
        WalEntryInfo e;
        e.seq_id = static_cast<uint64_t>(st.col_i64(0));
        e.timestamp = static_cast<uint64_t>(st.col_i64(1));
        e.op_type = static_cast<int>(st.col_i64(2));
        if (e.op_type == kOpFileUpdate) {
            e.file.file_id = st.col_text(3);
            e.file.name = st.col_text(4);
            e.file.version = static_cast<uint64_t>(st.col_i64(5));
            e.file.owner_device_id = static_cast<uint64_t>(st.col_i64(6));
        } else if (e.op_type == kOpDeviceUpdate) {
            e.device.device_id = static_cast<uint64_t>(st.col_i64(7));
            e.device.address = st.col_text(8);
        }
        out.push_back(std::move(e));
    }
    return out;
}

void SqliteDatabase::applyWalEntryToState(const WalEntryInfo& e) {
    if (e.op_type == kOpFileUpdate) {
        // История версий — уникальный ключ (file_id, version).
        // REPLACE выбран намеренно: если та же версия придёт повторно
        // (идемпотентность applyWalEntries), строка просто перезапишется
        // одинаковыми значениями.
        auto st = conn_->prepare(R"sql(
            INSERT OR REPLACE INTO files(file_id, version, name, owner_device_id)
            VALUES(?1, ?2, ?3, ?4);
        )sql");
        st.bind(1, e.file.file_id);
        st.bind(2, e.file.version);
        st.bind(3, e.file.name);
        st.bind(4, e.file.owner_device_id);
        st.run();
    } else if (e.op_type == kOpDeviceUpdate) {
        // На стороне ВУ device_id известен — просто UPSERT адрес.
        auto st = conn_->prepare(R"sql(
            INSERT INTO devices(device_id, address) VALUES(?1, ?2)
            ON CONFLICT(device_id) DO UPDATE SET address = excluded.address;
        )sql");
        st.bind(1, e.device.device_id);
        st.bind(2, e.device.address);
        st.run();
    }
}

void SqliteDatabase::applyWalEntries(const std::vector<WalEntryInfo>& entries) {
    // Весь батч накатывается в одной транзакции —
    // либо целиком, либо ничего (атомарный pull).
    conn_->begin();
    try {
        for (const auto& e : entries) {
            // 1) Журнал (идемпотентно: seq_id — PK).
            auto st = conn_->prepare(R"sql(
                INSERT OR IGNORE INTO wal(
                    seq_id, timestamp, op_type,
                    file_id, file_name, file_version, file_owner_device_id,
                    device_id, device_address
                ) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);
            )sql");
            st.bind(1, e.seq_id);
            st.bind(2, e.timestamp);
            st.bind(3, e.op_type);
            if (e.op_type == kOpFileUpdate) {
                st.bind(4, e.file.file_id);
                st.bind(5, e.file.name);
                st.bind(6, e.file.version);
                st.bind(7, e.file.owner_device_id);
                st.bind_null(8);
                st.bind_null(9);
            } else {
                st.bind_null(4);
                st.bind_null(5);
                st.bind_null(6);
                st.bind_null(7);
                st.bind(8, e.device.device_id);
                st.bind(9, e.device.address);
            }
            st.run();

            // 2) Актуальное состояние.
            applyWalEntryToState(e);
        }
        conn_->commit();
    } catch (...) {
        conn_->rollback();
        throw;
    }
}

std::optional<FileUpdateInfo> SqliteDatabase::getFileInfo(const std::string& file_id,
                                                          uint64_t version) {
    // Контракт: version == 0 ⇒ последняя известная версия.
    const char* sql =
        (version == 0)
            ? R"sql(SELECT file_id, name, version, owner_device_id
                    FROM files
                    WHERE file_id = ?1
                    ORDER BY version DESC
                    LIMIT 1;)sql"
            : R"sql(SELECT file_id, name, version, owner_device_id
                    FROM files
                    WHERE file_id = ?1 AND version = ?2;)sql";

    auto st = conn_->prepare(sql);
    st.bind(1, file_id);
    if (version != 0) {
        st.bind(2, version);
    }
    if (!st.step()) {
        return std::nullopt;
    }
    FileUpdateInfo info;
    info.file_id = st.col_text(0);
    info.name = st.col_text(1);
    info.version = static_cast<uint64_t>(st.col_i64(2));
    info.owner_device_id = static_cast<uint64_t>(st.col_i64(3));
    return info;
}

uint64_t SqliteDatabase::addWalEntry(const WalEntryInfo& entry) {
    // Сторона ГУ: сами назначаем seq_id = last+1 и пишем запись.
    // Всё — в одной транзакции, чтобы два параллельных PushWAL не
    // дали одинаковый seq_id (SQLite сериализует writes автоматически).
    uint64_t new_seq = 0;
    conn_->begin();
    try {
        new_seq = getLastSeqId() + 1;

        WalEntryInfo e = entry;
        e.seq_id = new_seq;

        auto st = conn_->prepare(R"sql(
            INSERT INTO wal(
                seq_id, timestamp, op_type,
                file_id, file_name, file_version, file_owner_device_id,
                device_id, device_address
            ) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);
        )sql");
        st.bind(1, e.seq_id);
        st.bind(2, e.timestamp);
        st.bind(3, e.op_type);
        if (e.op_type == kOpFileUpdate) {
            st.bind(4, e.file.file_id);
            st.bind(5, e.file.name);
            st.bind(6, e.file.version);
            st.bind(7, e.file.owner_device_id);
            st.bind_null(8);
            st.bind_null(9);
        } else {
            st.bind_null(4);
            st.bind_null(5);
            st.bind_null(6);
            st.bind_null(7);
            st.bind(8, e.device.device_id);
            st.bind(9, e.device.address);
        }
        st.run();

        applyWalEntryToState(e);
        conn_->commit();
    } catch (...) {
        conn_->rollback();
        throw;
    }
    return new_seq;
}

uint64_t SqliteDatabase::getLastSeqId() {
    auto st = conn_->prepare("SELECT COALESCE(MAX(seq_id), 0) FROM wal;");
    if (!st.step()) return 0;
    return static_cast<uint64_t>(st.col_i64(0));
}

std::vector<FileUpdateInfo> SqliteDatabase::getAllFiles() {
    // По одной строке на file_id — только последняя версия.
    std::vector<FileUpdateInfo> out;
    auto st = conn_->prepare(R"sql(
        SELECT f.file_id, f.name, f.version, f.owner_device_id
        FROM files f
        JOIN (
            SELECT file_id, MAX(version) AS max_v
            FROM files
            GROUP BY file_id
        ) last ON last.file_id = f.file_id AND last.max_v = f.version
        ORDER BY f.name ASC;
    )sql");
    while (st.step()) {
        FileUpdateInfo info;
        info.file_id = st.col_text(0);
        info.name = st.col_text(1);
        info.version = static_cast<uint64_t>(st.col_i64(2));
        info.owner_device_id = static_cast<uint64_t>(st.col_i64(3));
        out.push_back(std::move(info));
    }
    return out;
}

void SqliteDatabase::clearStaging() {
    // В MVP staging в БД не хранится — реализация пустая (см. комментарий
    // в db_contract.hpp). Если staging переедет в БД, добавить таблицу
    // pear_staging и очищать её здесь.
}

// ---- Devices ----

uint64_t SqliteDatabase::registerDevice(const std::string& address) {
    // Идемпотентно: повторный вызов с тем же address возвращает
    // тот же device_id, а не UNIQUE-ошибку.
    {
        auto st = conn_->prepare("SELECT device_id FROM devices WHERE address = ?1;");
        st.bind(1, address);
        if (st.step()) {
            return static_cast<uint64_t>(st.col_i64(0));
        }
    }
    auto st = conn_->prepare("INSERT INTO devices(address) VALUES(?1);");
    st.bind(1, address);
    st.run();
    return static_cast<uint64_t>(sqlite3_last_insert_rowid(conn_->native()));
}

std::string SqliteDatabase::getDeviceAddress(uint64_t device_id) {
    auto st = conn_->prepare("SELECT address FROM devices WHERE device_id = ?1;");
    st.bind(1, device_id);
    if (!st.step()) return {};
    return st.col_text(0);
}

// ---- Local config ----

void SqliteDatabase::setMasterAddress(const std::string& address) {
    auto st = conn_->prepare(R"sql(
        INSERT INTO local_config(key, value) VALUES(?1, ?2)
        ON CONFLICT(key) DO UPDATE SET value = excluded.value;
    )sql");
    st.bind(1, kCfgMasterAddress);
    st.bind(2, address);
    st.run();
}

std::string SqliteDatabase::getMasterAddress() {
    auto st = conn_->prepare("SELECT value FROM local_config WHERE key = ?1;");
    st.bind(1, kCfgMasterAddress);
    if (!st.step()) return {};
    return st.col_text(0);
}

void SqliteDatabase::setDeviceId(uint64_t id) {
    auto st = conn_->prepare(R"sql(
        INSERT INTO local_config(key, value) VALUES(?1, ?2)
        ON CONFLICT(key) DO UPDATE SET value = excluded.value;
    )sql");
    st.bind(1, kCfgDeviceId);
    st.bind(2, std::to_string(id));
    st.run();
}

uint64_t SqliteDatabase::getDeviceId() {
    auto st = conn_->prepare("SELECT value FROM local_config WHERE key = ?1;");
    st.bind(1, kCfgDeviceId);
    if (!st.step()) return 0;
    try {
        return std::stoull(st.col_text(0));
    } catch (...) {
        return 0;
    }
}

// ---- Status helper ----

std::vector<std::string> SqliteDatabase::getAllFileStatus() {
    // Формат записи: "<name>@<version>". CLI может печатать as-is.
    std::vector<std::string> out;
    for (const auto& f : getAllFiles()) {
        out.push_back(f.name + "@" + std::to_string(f.version));
    }
    return out;
}

}  // namespace pear::db
