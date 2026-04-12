#ifndef PEAR_NET_DB_TYPES_HPP_
#define PEAR_NET_DB_TYPES_HPP_

#include <cstdint>
#include <string>

namespace pear::net {

struct FileUpdateInfo {
    std::string file_id;
    std::string name;
    uint64_t version;
    uint64_t owner_device_id;
};

struct DeviceUpdateInfo {
    uint64_t device_id;
    std::string address;
};

struct WalEntryInfo {
    uint64_t seq_id;
    uint64_t timestamp;
    int op_type;
    FileUpdateInfo file;
    DeviceUpdateInfo device;
};

} // namespace pear::net

#endif // PEAR_NET_DB_TYPES_HPP_