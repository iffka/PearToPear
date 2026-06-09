#ifndef PEAR_NET_GRPC_DIRECT_TRANSPORT_HPP_
#define PEAR_NET_GRPC_DIRECT_TRANSPORT_HPP_

#include <pear/net/pear_transport.hpp>

namespace pear::net {

class GrpcDirectTransport final : public PearTransport {
public:
    uint64_t registerDevice(const std::string& master_ref, const std::string& self_ref) override;
    std::vector<WalEntryInfo> updateDB(const std::string& master_ref, uint64_t last_seq_id, uint64_t device_id) override;
    bool pushWAL(const std::string& master_ref, uint64_t device_id, const std::vector<WalEntryInfo>& entries, std::vector<uint64_t>& out_assigned_seq_ids) override;
    bool prepareReplica( const std::string& replica_ref, uint64_t view_number, uint64_t leader_device_id, const std::vector<WalEntryInfo>& entries, uint64_t commit_number, uint64_t& out_last_seq_id) override;
    bool commitReplica( const std::string& replica_ref, uint64_t view_number, uint64_t leader_device_id, uint64_t commit_number) override;
    ReplicaStateInfo getReplicaState(const std::string& replica_ref, uint64_t requester_device_id) override;
    void downloadFile(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id, const std::string& destination_path) override;
    uint64_t getObjectSize(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id) override;
    void downloadFileRange(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id, uint64_t offset, uint64_t size, const std::string& destination_path) override;
    bool deleteObject(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id) override;
};

} // namespace pear::net

#endif // PEAR_NET_GRPC_DIRECT_TRANSPORT_HPP_
