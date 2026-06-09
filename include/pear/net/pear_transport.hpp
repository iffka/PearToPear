#ifndef PEAR_NET_PEAR_TRANSPORT_HPP_
#define PEAR_NET_PEAR_TRANSPORT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <pear/net/db_types.hpp>

namespace pear::net {

class PearTransport {
public:
    virtual ~PearTransport() = default;

    virtual uint64_t registerDevice(const std::string& master_ref, const std::string& self_ref) = 0;
    virtual std::vector<WalEntryInfo> updateDB(const std::string& master_ref, uint64_t last_seq_id, uint64_t device_id) = 0;
    virtual bool pushWAL(const std::string& master_ref, uint64_t device_id, const std::vector<WalEntryInfo>& entries, std::vector<uint64_t>& out_assigned_seq_ids) = 0;
    virtual void downloadFile(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id, const std::string& destination_path) = 0;
    virtual uint64_t getObjectSize(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id) = 0;
    virtual void downloadFileRange(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id, uint64_t offset, uint64_t size, const std::string& destination_path) = 0;
};

} // namespace pear::net

#endif // PEAR_NET_PEAR_TRANSPORT_HPP_
