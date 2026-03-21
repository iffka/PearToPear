#ifndef PEAR_DB_DB_HPP
#define PEAR_DB_DB_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace pear::db {

struct FileRecord {
    std::string file_id;
    std::string name;
    uint64_t version;
    std::string owner_device_id;
};

struct WalEntry {
    uint64_t seq_id;
    std::string file_id;
    std::string name;
    uint64_t version;
    std::string owner_device_id;
};

class Database {
public:
    explicit Database(const std::filesystem::path& db_path);

    void init();

    void apply_wal(const std::vector<WalEntry>& entries);

    std::vector<FileRecord> get_all_files();

    uint64_t get_last_seq_id();

private:
    void exec(const std::string& sql);

    void apply_wal_entry(const WalEntry& e);

private:
    void* m_db = nullptr;
};

} // namespace pear::db

#endif
