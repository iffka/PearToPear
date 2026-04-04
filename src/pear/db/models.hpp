#ifndef P2P_DB_MODELS_HPP
#define P2P_DB_MODELS_HPP

#include <cstdint>
#include <string>

namespace p2p::db {

struct Device {
    std::string id;
    std::string address;
};

struct FileMeta {
    std::string file_id;
    std::string name;
    std::uint64_t size = 0;
    std::string sha256;
    std::uint64_t version = 0;
};

struct BlockMeta {
    std::string block_id;
    std::string file_id;
    std::uint64_t idx = 0;
    std::uint64_t size = 0;
    std::string sha256;
};

}  // namespace p2p::db

#endif  // P2P_DB_MODELS_HPP
