#include <pear/db/db.hpp>

#include <sqlite3.h>
#include <stdexcept>

namespace pear::db {

static void check(int rc, sqlite3* db) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

Database::Database(const std::filesystem::path& db_path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.string().c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("cannot open db");
    }
    m_db = db;
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec((sqlite3*)m_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "sql error";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}

void Database::init() {
    exec(R"(
        CREATE TABLE IF NOT EXISTS files(
            file_id TEXT PRIMARY KEY,
            name TEXT,
            version INTEGER,
            owner_device_id TEXT
        );
    )");

    exec(R"(
        CREATE TABLE IF NOT EXISTS wal(
            seq_id INTEGER PRIMARY KEY,
            file_id TEXT,
            name TEXT,
            version INTEGER,
            owner_device_id TEXT
        );
    )");
}

uint64_t Database::get_last_seq_id() {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2((sqlite3*)m_db,
        "SELECT MAX(seq_id) FROM wal;", -1, &stmt, nullptr);

    uint64_t res = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        res = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return res;
}

void Database::apply_wal(const std::vector<WalEntry>& entries) {
    exec("BEGIN;");
    for (const auto& e : entries) {
        apply_wal_entry(e);
    }
    exec("COMMIT;");
}

void Database::apply_wal_entry(const WalEntry& e) {
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2((sqlite3*)m_db,
        "INSERT OR IGNORE INTO wal(seq_id, file_id, name, version, owner_device_id) VALUES(?,?,?,?,?);",
        -1, &stmt, nullptr);

    sqlite3_bind_int64(stmt, 1, e.seq_id);
    sqlite3_bind_text(stmt, 2, e.file_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, e.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, e.version);
    sqlite3_bind_text(stmt, 5, e.owner_device_id.c_str(), -1, SQLITE_TRANSIENT);

    check(sqlite3_step(stmt), (sqlite3*)m_db);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2((sqlite3*)m_db,
        "INSERT INTO files(file_id, name, version, owner_device_id) VALUES(?,?,?,?) "
        "ON CONFLICT(file_id) DO UPDATE SET "
        "name=excluded.name, version=excluded.version, owner_device_id=excluded.owner_device_id;",
        -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, e.file_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, e.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, e.version);
    sqlite3_bind_text(stmt, 4, e.owner_device_id.c_str(), -1, SQLITE_TRANSIENT);

    check(sqlite3_step(stmt), (sqlite3*)m_db);
    sqlite3_finalize(stmt);
}

std::vector<FileRecord> Database::get_all_files() {
    std::vector<FileRecord> res;

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2((sqlite3*)m_db,
        "SELECT file_id, name, version, owner_device_id FROM files;",
        -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileRecord f;
        f.file_id = (const char*)sqlite3_column_text(stmt, 0);
        f.name = (const char*)sqlite3_column_text(stmt, 1);
        f.version = sqlite3_column_int64(stmt, 2);
        f.owner_device_id = (const char*)sqlite3_column_text(stmt, 3);
        res.push_back(f);
    }

    sqlite3_finalize(stmt);
    return res;
}

} // namespace pear::db
