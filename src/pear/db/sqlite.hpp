fndef P2P_DB_SQLITE_HPP
#define P2P_DB_SQLITE_HPP

#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace p2p::db {

struct DbError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class Statement;

class Connection {
public:
  Connection() = default;
  explicit Connection(const std::filesystem::path& file) { open(file); }

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  Connection(Connection&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}
  Connection& operator=(Connection&& other) noexcept {
        if (this != &other) {
              close();
              db_ = std::exchange(other.db_, nullptr);
            }
        return *this;
      }

  ~Connection() { close(); }

  void open(const std::filesystem::path& file);
  void close() noexcept;

  void exec(std::string_view sql);
  Statement prepare(std::string_view sql);

  void begin();
  void commit();
  void rollback() noexcept;

  sqlite3* native() const noexcept { return db_; }

private:
  sqlite3* db_ = nullptr;
};

class Statement {
public:
  Statement() = default;
  Statement(sqlite3* db, sqlite3_stmt* stmt) : db_(db), stmt_(stmt) {}

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  Statement(Statement&& other) noexcept
      : db_(other.db_), stmt_(std::exchange(other.stmt_, nullptr)) {}

  Statement& operator=(Statement&& other) noexcept {
        if (this != &other) {
              finalize();
              db_ = other.db_;
              stmt_ = std::exchange(other.stmt_, nullptr);
            }
        return *this;
      }

  ~Statement() { finalize(); }

  void reset();
  void clear_bindings();

  void bind(int idx, std::int64_t v);
  void bind(int idx, std::uint64_t v);
  void bind(int idx, int v);
  void bind(int idx, std::string_view v);
  void bind_null(int idx);

  bool step();
  void run();

  bool is_null(int col) const;
  std::int64_t col_i64(int col) const;
  std::string col_text(int col) const;

  sqlite3_stmt* native() const noexcept { return stmt_; }

private:
  void finalize() noexcept;

  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace p2p::db

#endif  // P2P_DB_SQLITE_HPP
