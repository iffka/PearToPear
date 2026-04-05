#ifndef PEAR_NET_DB_CONTRACT_HPP_
#define PEAR_NET_DB_CONTRACT_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
    int op_type; // 0 = FILE_UPDATE, 1 = DEVICE_UPDATE
    union Data {
        FileUpdateInfo file;
        DeviceUpdateInfo device;
    } data;
};

class DatabaseFacade {
public:
    virtual ~DatabaseFacade() = default;

    virtual std::vector<WalEntryInfo> getWalEntriesSince(uint64_t last_seq_id) = 0;
    virtual void applyWalEntries(const std::vector<WalEntryInfo>& entries) = 0;
    virtual std::optional<FileUpdateInfo> getFileInfo(const std::string& file_id, uint64_t version) = 0;
    virtual uint64_t registerDevice(const std::string& address) = 0;
    virtual std::string getDeviceAddress(uint64_t device_id) = 0;
    virtual uint64_t addWalEntry(const WalEntryInfo& entry) = 0;
};

} // namespace pear::net

#endif // PEAR_NET_DB_CONTRACT_HPP_