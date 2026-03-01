#ifndef P2P_DB_REPOSITORIES_HPP
#define P2P_DB_REPOSITORIES_HPP

#include <optional>
#include <string>
#include <vector>

#include "models.hpp"
#include "sqlite.hpp"

namespace p2p::db {

class DeviceRepository {
   public:
    explicit DeviceRepository(Connection& c) : c_(c) {}

    void upsert(const Device& d);
    void remove_by_id(const std::string& id);
    std::vector<Device> list_all();

   private:
    Connection& c_;
};

class FileRepository {
   public:
    explicit FileRepository(Connection& c) : c_(c) {}

    void upsert(const FileMeta& f);
    std::optional<FileMeta> get_by_id(const std::string& file_id);
    std::vector<FileMeta> list_all();
    void remove_by_id(const std::string& file_id);

   private:
    Connection& c_;
};

class BlockRepository {
   public:
    explicit BlockRepository(Connection& c) : c_(c) {}

    void upsert(const BlockMeta& b);
    std::vector<BlockMeta> list_by_file(const std::string& file_id);

   private:
    Connection& c_;
};

}  // namespace p2p::db

#endif  // P2P_DB_REPOSITORIES_HPP
