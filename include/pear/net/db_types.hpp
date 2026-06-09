#ifndef PEAR_NET_DB_TYPES_HPP_
#define PEAR_NET_DB_TYPES_HPP_

#include <cstdint>
#include <string>

namespace pear::net {

enum WalOpTypeInfo { kFileUpdate = 0, kDeviceUpdate = 1, kFileDelete = 2, kObjectOwnerUpdate = 3, kObjectOwnerDelete = 4};

// path - логический файл в общем репозитории (relative path от корня workspace)
// object_hash - конкретная версия содержимого, лежит в .peer/obj/<hash>
// version - монотонный номер версии для данного path
struct FileUpdateInfo {
    std::string path;
    std::string object_hash;
    uint64_t version;
    uint64_t owner_device_id;
    bool read_only = false;
};

struct FileDeleteInfo {
    std::string path;
    uint64_t version;
    uint64_t owner_device_id;
};

struct DeviceUpdateInfo {
    uint64_t device_id;
    std::string address;
};

struct ObjectOwnerUpdateInfo {
    std::string object_hash;
    uint64_t owner_device_id;
};

struct WalEntryInfo {
    uint64_t seq_id;
    uint64_t timestamp;
    uint64_t entry_view_number = 0;
    WalOpTypeInfo op_type;

    FileUpdateInfo file;
    FileDeleteInfo file_delete;
    DeviceUpdateInfo device;
    ObjectOwnerUpdateInfo object_owner;
};

enum ReplicaStatusInfo {
    kReplicaNormal = 0,
    kReplicaViewChange = 1,
    kReplicaRecovering = 2
};

struct VrStateInfo {
    uint64_t view_number = 0;
    uint64_t commit_number = 0;
    uint64_t last_normal_view = 0;
    ReplicaStatusInfo status = kReplicaNormal;
};

struct ReplicaStateInfo {
    uint64_t device_id = 0;
    std::string address;
    VrStateInfo vr_state;
    uint64_t last_seq_id = 0;
};

}  // namespace pear::net

#endif  // PEAR_NET_DB_TYPES_HPP_
