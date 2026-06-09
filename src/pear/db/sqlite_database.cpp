#include <pear/db/sqlite_database.hpp>
#include <string_view>

#include "schema.hpp"
#include "sqlite.hpp"

namespace pear::db {

using pear::net::DeviceUpdateInfo;
using pear::net::FileDeleteInfo;
using pear::net::FileUpdateInfo;
using pear::net::ObjectOwnerUpdateInfo;
using pear::net::WalEntryInfo;
using pear::net::WalOpTypeInfo;

namespace {

constexpr std::string_view kCfgMasterAddress = "master_address";
constexpr std::string_view kCfgDeviceId = "device_id";

void bindWalEntryState(Statement& st, const WalEntryInfo& entry) {
    st.bind(1, entry.seq_id);
    st.bind(2, entry.timestamp);
    st.bind(3, static_cast<int>(entry.op_type));

    if (entry.op_type == WalOpTypeInfo::kFileUpdate) {
        st.bind(4, entry.file.path);
        st.bind(5, entry.file.object_hash);
        st.bind(6, entry.file.version);
        st.bind(7, entry.file.owner_device_id);
        st.bind(8, entry.file.read_only ? 1 : 0);
        st.bind_null(9);
        st.bind_null(10);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kFileDelete) {
        st.bind(4, entry.file_delete.path);
        st.bind_null(5);
        st.bind(6, entry.file_delete.version);
        st.bind(7, entry.file_delete.owner_device_id);
        st.bind(8, 0);
        st.bind_null(9);
        st.bind_null(10);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kObjectOwnerUpdate || entry.op_type == WalOpTypeInfo::kObjectOwnerDelete) {
        st.bind_null(4);
        st.bind(5, entry.object_owner.object_hash);
        st.bind_null(6);
        st.bind(7, entry.object_owner.owner_device_id);
        st.bind(8, 0);
        st.bind_null(9);
        st.bind_null(10);
        return;
    }

    st.bind_null(4);
    st.bind_null(5);
    st.bind_null(6);
    st.bind_null(7);
    st.bind(8, 0);
    st.bind(9, entry.device.device_id);
    st.bind(10, entry.device.address);
}

}  // namespace

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
            file_path,
            file_object_hash,
            file_version,
            file_owner_device_id,
            file_read_only,
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
            entry.file.path = st.col_text(3);
            entry.file.object_hash = st.col_text(4);
            entry.file.version = static_cast<uint64_t>(st.col_i64(5));
            entry.file.owner_device_id = static_cast<uint64_t>(st.col_i64(6));
            entry.file.read_only = st.col_i64(7) != 0;
        } else if (entry.op_type == WalOpTypeInfo::kFileDelete) {
            entry.file_delete.path = st.col_text(3);
            entry.file_delete.version = static_cast<uint64_t>(st.col_i64(5));
            entry.file_delete.owner_device_id = static_cast<uint64_t>(st.col_i64(6));
        } else if (entry.op_type == WalOpTypeInfo::kDeviceUpdate) {
            entry.device.device_id = static_cast<uint64_t>(st.col_i64(8));
            entry.device.address = st.col_text(9);
        } else if (entry.op_type == WalOpTypeInfo::kObjectOwnerUpdate || entry.op_type == WalOpTypeInfo::kObjectOwnerDelete) {
            entry.object_owner.object_hash = st.col_text(4);
            entry.object_owner.owner_device_id = static_cast<uint64_t>(st.col_i64(6));
        }

        out.push_back(std::move(entry));
    }
    return out;
}

void SqliteDatabase::applyWalEntryToState(const WalEntryInfo& entry) {
    if (entry.op_type == WalOpTypeInfo::kFileUpdate) {
        auto st = conn_->prepare(R"sql(
            INSERT OR REPLACE INTO files(
                path,
                version,
                object_hash,
                owner_device_id,
                is_deleted,
                read_only
            )
            VALUES(?1, ?2, ?3, ?4, 0, ?5);
        )sql");

        st.bind(1, entry.file.path);
        st.bind(2, entry.file.version);
        st.bind(3, entry.file.object_hash);
        st.bind(4, entry.file.owner_device_id);
        st.bind(5, entry.file.read_only ? 1 : 0);
        st.run();

        auto owner_st = conn_->prepare(R"sql(
            INSERT OR IGNORE INTO object_owners(object_hash, owner_device_id)
            VALUES(?1, ?2);
        )sql");
        owner_st.bind(1, entry.file.object_hash);
        owner_st.bind(2, entry.file.owner_device_id);
        owner_st.run();
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kFileDelete) {
        auto st = conn_->prepare(R"sql(
            INSERT OR REPLACE INTO files(
                path,
                version,
                object_hash,
                owner_device_id,
                is_deleted,
                read_only
            )
            VALUES(?1, ?2, NULL, ?3, 1, 0);
        )sql");

        st.bind(1, entry.file_delete.path);
        st.bind(2, entry.file_delete.version);
        st.bind(3, entry.file_delete.owner_device_id);
        st.run();
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kObjectOwnerUpdate) {
        auto st = conn_->prepare(R"sql(
            INSERT OR IGNORE INTO object_owners(object_hash, owner_device_id)
            VALUES(?1, ?2);
        )sql");

        st.bind(1, entry.object_owner.object_hash);
        st.bind(2, entry.object_owner.owner_device_id);
        st.run();
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kObjectOwnerDelete) {
        auto st = conn_->prepare(R"sql(
            DELETE FROM object_owners
            WHERE object_hash = ?1 AND owner_device_id = ?2;
        )sql");

        st.bind(1, entry.object_owner.object_hash);
        st.bind(2, entry.object_owner.owner_device_id);
        st.run();
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kDeviceUpdate) {
        auto st = conn_->prepare(R"sql(
            INSERT INTO devices(device_id, address)
            VALUES(?1, ?2)
            ON CONFLICT(device_id) DO UPDATE SET
                address = excluded.address;
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
                    file_path,
                    file_object_hash,
                    file_version,
                    file_owner_device_id,
                    file_read_only,
                    device_id,
                    device_address
                )
                VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);
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

std::optional<FileUpdateInfo> SqliteDatabase::getFileInfoByPath(const std::string& path, uint64_t version) {
    if (version == 0) {
        auto st = conn_->prepare(R"sql(
            SELECT path, object_hash, version, owner_device_id, read_only, is_deleted
            FROM files
            WHERE path = ?1
            ORDER BY version DESC
            LIMIT 1;
        )sql");

        st.bind(1, path);

        if (!st.step()) {
            return std::nullopt;
        }

        bool is_deleted = st.col_i64(5) != 0;
        if (is_deleted) {
            return std::nullopt;
        }

        FileUpdateInfo info;
        info.path = st.col_text(0);
        info.object_hash = st.col_text(1);
        info.version = static_cast<uint64_t>(st.col_i64(2));
        info.owner_device_id = static_cast<uint64_t>(st.col_i64(3));
        info.read_only = st.col_i64(4) != 0;
        return info;
    }

    auto st = conn_->prepare(R"sql(
        SELECT path, object_hash, version, owner_device_id, read_only
        FROM files
        WHERE path = ?1 AND version = ?2 AND is_deleted = 0;
    )sql");

    st.bind(1, path);
    st.bind(2, version);

    if (!st.step()) {
        return std::nullopt;
    }
    FileUpdateInfo info;
    info.path = st.col_text(0);
    info.object_hash = st.col_text(1);
    info.version = static_cast<uint64_t>(st.col_i64(2));
    info.owner_device_id = static_cast<uint64_t>(st.col_i64(3));
    info.read_only = st.col_i64(4) != 0;
    return info;
}

std::optional<std::string> SqliteDatabase::getObjectHashByPath(const std::string& path) {
    auto info = getFileInfoByPath(path, 0);
    if (!info) {
        return std::nullopt;
    }
    return info->object_hash;
}

uint64_t SqliteDatabase::addWalEntry(const WalEntryInfo& entry) {
    uint64_t new_seq_id = 0;

    conn_->begin();
    try {
        new_seq_id = getLastSeqId() + 1;

        WalEntryInfo stored_entry = entry;
        stored_entry.seq_id = new_seq_id;

        if (stored_entry.op_type == WalOpTypeInfo::kFileUpdate) {
            stored_entry.file.version = getNextVersion(stored_entry.file.path);
        } else if (stored_entry.op_type == WalOpTypeInfo::kFileDelete) {
            stored_entry.file_delete.version = getNextVersion(stored_entry.file_delete.path);
        }

        auto st = conn_->prepare(R"sql(
            INSERT INTO wal(
                seq_id,
                timestamp,
                op_type,
                file_path,
                file_object_hash,
                file_version,
                file_owner_device_id,
                file_read_only,
                device_id,
                device_address
            )
            VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);
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

uint64_t SqliteDatabase::getNextVersion(const std::string& path) {
    auto st = conn_->prepare(R"sql(
        SELECT COALESCE(MAX(version), 0)
        FROM files
        WHERE path = ?1;
    )sql");

    st.bind(1, path);

    if (!st.step()) {
        return 1;
    }

    return static_cast<uint64_t>(st.col_i64(0)) + 1;
}

std::vector<FileUpdateInfo> SqliteDatabase::getAllFiles() {
    std::vector<FileUpdateInfo> out;
    auto st = conn_->prepare(R"sql(
        SELECT f.path, f.object_hash, f.version, f.owner_device_id, f.read_only
        FROM files f
        JOIN (
            SELECT path, MAX(version) AS max_version
            FROM files
            GROUP BY path
        ) latest
            ON latest.path = f.path
           AND latest.max_version = f.version
        WHERE f.is_deleted = 0
        ORDER BY f.path ASC;
    )sql");
    while (st.step()) {
        FileUpdateInfo info;
        info.path = st.col_text(0);
        info.object_hash = st.col_text(1);
        info.version = static_cast<uint64_t>(st.col_i64(2));
        info.owner_device_id = static_cast<uint64_t>(st.col_i64(3));
        info.read_only = st.col_i64(4) != 0;
        out.push_back(std::move(info));
    }
    return out;
}

std::vector<std::string> SqliteDatabase::getAllKnownFilePaths() {
    std::vector<std::string> out;

    auto st = conn_->prepare(R"sql(
        SELECT DISTINCT path
        FROM files
        ORDER BY path ASC;
    )sql");

    while (st.step()) {
        out.push_back(st.col_text(0));
    }

    return out;
}

uint64_t SqliteDatabase::cleanupOldFileVersionsForPath(const std::string& path, uint64_t keep_versions) {
    std::vector<uint64_t> versions;

    auto select_st = conn_->prepare(R"sql(
        SELECT version
        FROM files
        WHERE path = ?1
        ORDER BY version DESC;
    )sql");

    select_st.bind(1, path);

    while (select_st.step()) {
        versions.push_back(static_cast<uint64_t>(select_st.col_i64(0)));
    }

    if (versions.size() <= keep_versions) {
        return 0;
    }

    uint64_t removed_count = 0;

    for (std::size_t index = keep_versions; index < versions.size(); ++index) {
        auto delete_st = conn_->prepare(R"sql(
            DELETE FROM files
            WHERE path = ?1 AND version = ?2;
        )sql");

        delete_st.bind(1, path);
        delete_st.bind(2, versions[index]);
        delete_st.run();

        ++removed_count;
    }

    return removed_count;
}

uint64_t SqliteDatabase::cleanupOldFileVersions(const std::vector<std::string>& paths, uint64_t keep_versions) {
    uint64_t removed_count = 0;

    conn_->begin();

    try {
        for (const auto& path : paths) {
            removed_count += cleanupOldFileVersionsForPath(path, keep_versions);
        }

        conn_->commit();
    } catch (...) {
        conn_->rollback();
        throw;
    }

    return removed_count;
}

void SqliteDatabase::stageFile(const std::string& path, const std::string& object_hash, const std::string& local_path, const std::string& operation, bool read_only) {
    auto st = conn_->prepare(R"sql(
        INSERT INTO staging_files(path, object_hash, local_path, operation, read_only)
        VALUES(?1, ?2, ?3, ?4, ?5)
        ON CONFLICT(path) DO UPDATE
        SET object_hash = excluded.object_hash,
            local_path = excluded.local_path,
            operation = excluded.operation,
            read_only = excluded.read_only;
    )sql");

    st.bind(1, path);
    st.bind(2, object_hash);
    st.bind(3, local_path);
    st.bind(4, operation);
    st.bind(5, read_only ? 1 : 0);
    st.run();
}

void SqliteDatabase::unstageFile(const std::string& path) {
    auto st = conn_->prepare(R"sql(
        DELETE FROM staging_files
        WHERE path = ?1;
    )sql");

    st.bind(1, path);
    st.run();
}

std::vector<StagedFileInfo> SqliteDatabase::getStagedFiles() {
    std::vector<StagedFileInfo> out;

    auto st = conn_->prepare(R"sql(
        SELECT path, object_hash, local_path, operation, read_only
        FROM staging_files
        ORDER BY path ASC;
    )sql");

    while (st.step()) {
        StagedFileInfo info;
        info.path = st.col_text(0);
        info.object_hash = st.col_text(1);
        info.local_path = st.col_text(2);
        info.operation = st.col_text(3);
        info.read_only = st.col_i64(4) != 0;
        out.push_back(std::move(info));
    }

    return out;
}

void SqliteDatabase::clearStaging() {
    auto st = conn_->prepare("DELETE FROM staging_files;");
    st.run();
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

    uint64_t new_id = static_cast<uint64_t>(sqlite3_last_insert_rowid(conn_->native()));

    WalEntryInfo entry{};
    entry.op_type = WalOpTypeInfo::kDeviceUpdate;
    entry.device.device_id = new_id;
    entry.device.address = address;

    addWalEntry(entry);

    return new_id;
}

std::string SqliteDatabase::getDeviceAddress(uint64_t device_id) {
    auto st = conn_->prepare("SELECT address FROM devices WHERE device_id = ?1;");
    st.bind(1, device_id);

    if (!st.step()) {
        return {};
    }

    return st.col_text(0);
}


std::vector<uint64_t> SqliteDatabase::getObjectOwnerDeviceIds(const std::string& object_hash) {
    std::vector<uint64_t> out;
    auto st = conn_->prepare(R"sql(
        SELECT owner_device_id
        FROM object_owners
        WHERE object_hash = ?1
        ORDER BY owner_device_id ASC;
    )sql");

    st.bind(1, object_hash);

    while (st.step()) {
        out.push_back(static_cast<uint64_t>(st.col_i64(0)));
    }

    return out;
}

std::vector<std::string> SqliteDatabase::getObjectOwnerAddresses(const std::string& object_hash) {
    std::vector<std::string> out;
    auto st = conn_->prepare(R"sql(
        SELECT d.address
        FROM object_owners o
        JOIN devices d ON d.device_id = o.owner_device_id
        WHERE o.object_hash = ?1
        ORDER BY o.owner_device_id ASC;
    )sql");

    st.bind(1, object_hash);

    while (st.step()) {
        out.push_back(st.col_text(0));
    }

    return out;
}

bool SqliteDatabase::hasObjectOwner(const std::string& object_hash, uint64_t device_id) {
    auto st = conn_->prepare(R"sql(
        SELECT 1
        FROM object_owners
        WHERE object_hash = ?1 AND owner_device_id = ?2
        LIMIT 1;
    )sql");

    st.bind(1, object_hash);
    st.bind(2, device_id);

    return st.step();
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
        out.push_back(file.path + "@" + std::to_string(file.version));
    }
    return out;
}

bool SqliteDatabase::isObjectReferenced(const std::string& object_hash) {
    auto st = conn_->prepare(R"sql(
        SELECT EXISTS(
            SELECT 1
            FROM files
            WHERE object_hash = ?1
            UNION ALL
            SELECT 1
            FROM staging_files
            WHERE object_hash = ?1
        );
    )sql");

    st.bind(1, object_hash);

    if (!st.step()) {
        return false;
    }

    return st.col_i64(0) != 0;
}

}  // namespace pear::db
