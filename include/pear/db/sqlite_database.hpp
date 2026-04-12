#ifndef PEAR_DB_SQLITE_DATABASE_HPP
#define PEAR_DB_SQLITE_DATABASE_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <pear/net/db_contract.hpp>

namespace pear::db {

class Connection;

class SqliteDatabase : public pear::net::DatabaseFacade {
public:
    explicit SqliteDatabase(const std::filesystem::path& db_path);
    ~SqliteDatabase() override;

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&&) noexcept = default;
    SqliteDatabase& operator=(SqliteDatabase&&) noexcept = default;

    std::vector<pear::net::WalEntryInfo> getWalEntriesSince(uint64_t last_seq_id) override;
    void applyWalEntries(const std::vector<pear::net::WalEntryInfo>& entries) override;

    std::optional<pear::net::FileUpdateInfo> getFileInfo(
        const std::string& file_id,
        uint64_t version
    ) override;

    uint64_t addWalEntry(const pear::net::WalEntryInfo& entry) override;
    uint64_t getLastSeqId() override;
    std::vector<pear::net::FileUpdateInfo> getAllFiles() override;

    void clearStaging() override;

    uint64_t registerDevice(const std::string& address) override;
    std::string getDeviceAddress(uint64_t device_id) override;

    void setMasterAddress(const std::string& address) override;
    std::string getMasterAddress() override;

    void setDeviceId(uint64_t id) override;
    uint64_t getDeviceId() override;

    std::vector<std::string> getAllFileStatus();

private:
    void applyWalEntryToState(const pear::net::WalEntryInfo& entry);

    std::unique_ptr<Connection> conn_;
};

} // namespace pear::db

#endif // PEAR_DB_SQLITE_DATABASE_HPP