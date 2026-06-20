#ifndef PEAR_NET_REMOTE_CLIENT_HPP_
#define PEAR_NET_REMOTE_CLIENT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <pear/net/db_types.hpp>

namespace pear::net {

class RemoteClient {
public:
    static uint64_t RegisterDevice(const std::string& gu_address, const std::string& my_address);
    static std::vector<WalEntryInfo> UpdateDB(const std::string& gu_address, uint64_t last_seq_id, uint64_t device_id);
    static bool PushWAL(const std::string& gu_address, uint64_t device_id, const std::vector<WalEntryInfo>& entries, std::vector<uint64_t>& out_assigned_seq_ids);
    static bool Prepare( const std::string& replica_address, uint64_t view_number, uint64_t leader_device_id, const std::vector<WalEntryInfo>& entries, uint64_t commit_number, uint64_t& out_last_seq_id);
    static bool Commit( const std::string& replica_address, uint64_t view_number, uint64_t leader_device_id, uint64_t commit_number);
    static ReplicaStateInfo GetReplicaState(const std::string& replica_address, uint64_t requester_device_id);
    static void DownloadFile(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id, const std::string& destination_path);
    static uint64_t GetObjectSize(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id);
    static void DownloadFileRange(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id, uint64_t offset, uint64_t size, const std::string& destination_path);
    static bool DeleteObject(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id);
};

} // namespace pear::net

#endif // PEAR_NET_REMOTE_CLIENT_HPP_