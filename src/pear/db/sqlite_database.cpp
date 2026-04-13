#include "sqlite_database.hpp"

#include <string_view>

#include "schema.hpp"
#include "sqlite.hpp"

namespace pear::db {

using pear::net::DeviceUpdateInfo;
using pear::net::FileDeleteInfo;
using pear::net::FileUpdateInfo;
using pear::net::WalEntryInfo;
using pear::net::WalOpTypeInfo;

namespace {

constexpr std::string_view kCfgMasterAddress = "master_address";
constexpr std::string_view kCfgDeviceId = "device_id";

std::string getLatestKnownFileName(Connection& conn, const std::string& file_id) {
    auto st = conn.prepare(R"sql(
        SELECT name
        FROM files
        WHERE file_id = ?1
        ORDER BY version DESC
        LIMIT 1;
    )sql");

    st.bind(1, file_id);

    if (!st.step()) {
        return file_id;
    }

    return st.col_text(0);
}

void bindWalEntryState(sqlite::Statement& st, const WalEntryInfo& entry) {
    st.bind(1, entry.seq_id);
    st.bind(2, entry.timestamp);
    st.bind(3, static_cast<int>(entry.op_type));

    if (entry.op_type == WalOpTypeInfo::kFileUpdate) {
        st.bind(4, entry.file.file_id);
        st.bind(5, entry.file.name);
        st.bind(6, entry.file.version);
        st.bind(7, entry.file.owner_device_id);
        st.bind_null(8);
        st.bind_null(9);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kFileDelete) {
        st.bind(4, entry.file_delete.file_id);
        st.bind_null(5);
        st.bind(6, entry.file_delete.version);
        st.bind(7, entry.file_delete.owner_device_id);
        st.bind_null(8);
        st.bind_null(9);
        return;
    }

    st.bind_null(4);
    st.bind_null(5);
    st.bind_null(6);
    st.bind_null(7);
    st.bind(8, entry.device.device_id);
    st.bind(9, entry.device.address);
}

} // namespace

SqliteDatabase::SqliteDatabase(const std::filesystem::path& db_path)
    : conn_(std::make_unique<Connection>(db_path)) {
    ensure_schema(*conn_);
}

SqliteDatabase::~SqliteDatabase() = default;

std::vector<WalEntryInfo> SqliteDatabase::getWalEntriesSince(uint64_t last_seq_id) {
    std::vector<WalEntryInfo> out;
    auto st = conn_->prepare(R"sql(
        SELECT
            seq_id,
            timestamp,
            op_type,
            file_id,
            file_name,
            file_version,
            file_owner_device_id,
            device_id,
            device_address
        FROM wal
        WHERE seq_id > ?1
        ORDER BY seq_id ASC;
    )sql");
    st.bind(1, last_seq_id);
    while (st.step()) {
        WalEntryInfo entry;

        entry.seq_id = static_cast<uint64_t>(st.col_i64(0));
        entry.timestamp = static_cast<uint64_t>(st.col_i64(1));
        entry.op_type = static_cast<WalOpTypeInfo>(st.col_i64(2));

        if (entry.op_type == WalOpTypeInfo::kFileUpdate) {
            entry.file.file_id = st.col_text(3);
            entry.file.name = st.col_text(4);
            entry.file.version = static_cast<uint64_t>(st.col_i64(5));
            entry.file.owner_device_id = static_cast<uint64_t>(st.col_i64(6));
        } else if (entry.op_type == WalOpTypeInfo::kFileDelete) {
            entry.file_delete.file_id = st.col_text(3);
            entry.file_delete.version = static_cast<uint64_t>(st.col_i64(5));
            entry.file_delete.owner_device_id = static_cast<uint64_t>(st.col_i64(6));
        } else if (entry.op_type == WalOpTypeInfo::kDeviceUpdate) {
            entry.device.device_id = static_cast<uint64_t>(st.col_i64(7));
            entry.device.address = st.col_text(8);
        }

        out.push_back(std::move(entry));
    }
    return out;
}

void SqliteDatabase::applyWalEntryToState(const WalEntryInfo& entry) {
    if (entry.op_type == WalOpTypeInfo::kFileUpdate) {
        auto st = conn_->prepare(R"sql(
            INSERT OR REPLACE INTO files(
                file_id,
                version,
                name,
                owner_device_id,
                is_deleted
            )
            VALUES(?1, ?2, ?3, ?4, 0);
        )sql");

        st.bind(1, entry.file.file_id);
        st.bind(2, entry.file.version);
        st.bind(3, entry.file.name);
        st.bind(4, entry.file.owner_device_id);
        st.run();
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kFileDelete) {
        std::string latest_name = getLatestKnownFileName(*conn_, entry.file_delete.file_id);

        auto st = conn_->prepare(R"sql(
            INSERT OR REPLACE INTO files(
                file_id,
                version,
                name,
                owner_device_id,
                is_deleted
            )
            VALUES(?1, ?2, ?3, ?4, 1);
        )sql");

        st.bind(1, entry.file_delete.file_id);
        st.bind(2, entry.file_delete.version);
        st.bind(3, latest_name);
        st.bind(4, entry.file_delete.owner_device_id);
        st.run();
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kDeviceUpdate) {
        auto st = conn_->prepare(R"sql(
            INSERT INTO devices(device_id, address)
            VALUES(?1, ?2)
            ON CONFLICT(device_id) DO UPDATE
            SET address = excluded.address;
        )sql");

        st.bind(1, entry.device.device_id);
        st.bind(2, entry.device.address);
        st.run();
    }
}

void SqliteDatabase::applyWalEntries(const std::vector<WalEntryInfo>& entries) {
    conn_->begin();

    try {
        for (const auto& entry : entries) {
            auto st = conn_->prepare(R"sql(
                INSERT OR IGNORE INTO wal(
                    seq_id,
                    timestamp,
                    op_type,
                    file_id,
                    file_name,
                    file_version,
                    file_owner_device_id,
                    device_id,
                    device_address
                )
                VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);
            )sql");

            bindWalEntryState(st, entry);
            st.run();

            applyWalEntryToState(entry);
        }
        conn_->commit();
    } catch (...) {
        conn_->rollback();
        throw;
    }
}

std::optional<FileUpdateInfo> SqliteDatabase::getFileInfo(const std::string& file_id, uint64_t version) {
    if (version == 0) {
        auto st = conn_->prepare(R"sql(
            SELECT file_id, name, version, owner_device_id, is_deleted
            FROM files
            WHERE file_id = ?1
            ORDER BY version DESC
            LIMIT 1;
        )sql");

        st.bind(1, file_id);

        if (!st.step()) {
            return std::nullopt;
        }

        bool is_deleted = st.col_i64(4) != 0;

        if (is_deleted) {
            return std::nullopt;
        }

        FileUpdateInfo info;
        info.file_id = st.col_text(0);
        info.name = st.col_text(1);
        info.version = static_cast<uint64_t>(st.col_i64(2));
        info.owner_device_id = static_cast<uint64_t>(st.col_i64(3));
        return info;
    }

    auto st = conn_->prepare(R"sql(
        SELECT file_id, name, version, owner_device_id
        FROM files
        WHERE file_id = ?1 AND version = ?2 AND is_deleted = 0;
    )sql");

    st.bind(1, file_id);
    st.bind(2, version);

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
    uint64_t new_seq_id = 0;

    conn_->begin();
    try {
        new_seq_id = getLastSeqId() + 1;

        WalEntryInfo stored_entry = entry;
        stored_entry.seq_id = new_seq_id;

        auto st = conn_->prepare(R"sql(
            INSERT INTO wal(
                seq_id,
                timestamp,
                op_type,
                file_id,
                file_name,
                file_version,
                file_owner_device_id,
                device_id,
                device_address
            )
            VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);
        )sql");

        bindWalEntryState(st, stored_entry);
        st.run();

        applyWalEntryToState(stored_entry);

        conn_->commit();
    } catch (...) {
        conn_->rollback();
        throw;
    }

    return new_seq_id;
}

uint64_t SqliteDatabase::getLastSeqId() {
    auto st = conn_->prepare("SELECT COALESCE(MAX(seq_id), 0) FROM wal;");

    if (!st.step()) {
        return 0;
    }

    return static_cast<uint64_t>(st.col_i64(0));
}

uint64_t SqliteDatabase::getNextVersion(const std::string& file_id) {
    auto st = conn_->prepare(R"sql(
        SELECT COALESCE(MAX(version), 0)
        FROM files
        WHERE file_id = ?1;
    )sql");

    st.bind(1, file_id);

    if (!st.step()) {
        return 1;
    }

    return static_cast<uint64_t>(st.col_i64(0)) + 1;
}

std::vector<FileUpdateInfo> SqliteDatabase::getAllFiles() {
    std::vector<FileUpdateInfo> out;
    auto st = conn_->prepare(R"sql(
        SELECT f.file_id, f.name, f.version, f.owner_device_id
        FROM files f
        JOIN (
            SELECT file_id, MAX(version) AS max_version
            FROM files
            GROUP BY file_id
        ) latest
            ON latest.file_id = f.file_id
           AND latest.max_version = f.version
        WHERE f.is_deleted = 0
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
    // Пока staging в БД не хранится.
}

uint64_t SqliteDatabase::registerDevice(const std::string& address) {
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

    if (!st.step()) {
        return {};
    }

    return st.col_text(0);
}

void SqliteDatabase::setMasterAddress(const std::string& address) {
    auto st = conn_->prepare(R"sql(
        INSERT INTO local_config(key, value)
        VALUES(?1, ?2)
        ON CONFLICT(key) DO UPDATE
        SET value = excluded.value;
    )sql");
    st.bind(1, kCfgMasterAddress);
    st.bind(2, address);
    st.run();
}

std::string SqliteDatabase::getMasterAddress() {
    auto st = conn_->prepare("SELECT value FROM local_config WHERE key = ?1;");
    st.bind(1, kCfgMasterAddress);

    if (!st.step()) {
        return {};
    }

    return st.col_text(0);
}

void SqliteDatabase::setDeviceId(uint64_t id) {
    auto st = conn_->prepare(R"sql(
        INSERT INTO local_config(key, value)
        VALUES(?1, ?2)
        ON CONFLICT(key) DO UPDATE
        SET value = excluded.value;
    )sql");
    st.bind(1, kCfgDeviceId);
    st.bind(2, std::to_string(id));
    st.run();
}

uint64_t SqliteDatabase::getDeviceId() {
    auto st = conn_->prepare("SELECT value FROM local_config WHERE key = ?1;");
    st.bind(1, kCfgDeviceId);

    if (!st.step()) {
        return 0;
    }

    try {
        return std::stoull(st.col_text(0));
    } catch (...) {
        return 0;
    }
}

std::vector<std::string> SqliteDatabase::getAllFileStatus() {
    std::vector<std::string> out;

    for (const auto& file : getAllFiles()) {
        out.push_back(file.name + "@" + std::to_string(file.version));
    }
    return out;
}

} // namespace pear::db