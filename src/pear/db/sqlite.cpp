#include "sqlite.hpp"

namespace pear::db {

namespace {

// Формирует и кидает DbError с текущим sqlite errmsg.
[[noreturn]] void throw_sqlite(sqlite3* db, const char* prefix) {
    std::string msg = prefix ? (std::string(prefix) + ": ") : "";
    msg += (db ? sqlite3_errmsg(db) : "sqlite error");
    throw DbError(msg);
}

}  // namespace

// ---- Connection ----

void Connection::open(const std::filesystem::path& file) {
    close();
    if (sqlite3_open(file.string().c_str(), &db_) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite3_open");
    }
    // FK-поддержка включена по умолчанию (на будущее).
    exec("PRAGMA foreign_keys = ON;");
}

void Connection::close() noexcept {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void Connection::exec(std::string_view sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "sqlite3_exec";
        sqlite3_free(err);
        throw DbError(msg);
    }
}

Statement Connection::prepare(std::string_view sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, std::string(sql).c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite3_prepare_v2");
    }
    return Statement(db_, stmt);
}

void Connection::begin() { exec("BEGIN;"); }
void Connection::commit() { exec("COMMIT;"); }

void Connection::rollback() noexcept {
    if (db_) {
        (void)sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
}

// ---- Statement ----

void Statement::finalize() noexcept {
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

void Statement::reset() { (void)sqlite3_reset(stmt_); }
void Statement::clear_bindings() { (void)sqlite3_clear_bindings(stmt_); }

void Statement::bind(int idx, std::int64_t v) {
    if (sqlite3_bind_int64(stmt_, idx, static_cast<sqlite3_int64>(v)) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite3_bind_int64");
    }
}

void Statement::bind(int idx, std::uint64_t v) {
    bind(idx, static_cast<std::int64_t>(v));
}

void Statement::bind(int idx, int v) {
    if (sqlite3_bind_int(stmt_, idx, v) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite3_bind_int");
    }
}

void Statement::bind(int idx, std::string_view v) {
    if (sqlite3_bind_text(stmt_, idx, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT) !=
        SQLITE_OK) {
        throw_sqlite(db_, "sqlite3_bind_text");
    }
}

void Statement::bind_null(int idx) {
    if (sqlite3_bind_null(stmt_, idx) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite3_bind_null");
    }
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw_sqlite(db_, "sqlite3_step");
}

void Statement::run() {
    while (step()) {
    }
}

bool Statement::is_null(int col) const {
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
}

std::int64_t Statement::col_i64(int col) const {
    return sqlite3_column_int64(stmt_, col);
}

std::string Statement::col_text(int col) const {
    const unsigned char* p = sqlite3_column_text(stmt_, col);
    return p ? reinterpret_cast<const char*>(p) : std::string{};
}

}  // namespace pear::db
