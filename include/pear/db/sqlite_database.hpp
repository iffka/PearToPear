#ifndef PEAR_DB_SQLITE_DATABASE_HPP
#define PEAR_DB_SQLITE_DATABASE_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <pear/net/db_types.hpp>

namespace pear::db {

class Connection;

struct StagedFileInfo {
    std::string file_id;
    std::string name;
    std::string local_path;
};

class SqliteDatabase {
public:
    explicit SqliteDatabase(const std::filesystem::path& db_path);
    ~SqliteDatabase();

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&&) noexcept = default;
    SqliteDatabase& operator=(SqliteDatabase&&) noexcept = default;

    std::vector<pear::net::WalEntryInfo> getWalEntriesSince(uint64_t last_seq_id);
    void applyWalEntries(const std::vector<pear::net::WalEntryInfo>& entries);
    std::optional<pear::net::FileUpdateInfo> getFileInfo(const std::string& file_id, uint64_t version);
    uint64_t addWalEntry(const pear::net::WalEntryInfo& entry);
    uint64_t getLastSeqId();
    uint64_t getNextVersion(const std::string& file_id);
    std::vector<pear::net::FileUpdateInfo> getAllFiles();
    void stageFile(const std::string& file_id, const std::string& name, const std::string& local_path);
    void unstageFile(const std::string& file_id);
    std::vector<StagedFileInfo> getStagedFiles();
    void clearStaging();

    uint64_t registerDevice(const std::string& address);
    std::string getDeviceAddress(uint64_t device_id);

    void setMasterAddress(const std::string& address);
    std::string getMasterAddress();

    void setDeviceId(uint64_t id);
    uint64_t getDeviceId();

    std::vector<std::string> getAllFileStatus();

private:
    void applyWalEntryToState(const pear::net::WalEntryInfo& entry);

    std::unique_ptr<Connection> conn_;
};

} // namespace pear::db

#endif // PEAR_DB_SQLITE_DATABASE_HPP