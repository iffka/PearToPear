#include <pear/net/grpc_direct_transport.hpp>

#include <pear/net/remote_client.hpp>

namespace pear::net {

uint64_t GrpcDirectTransport::registerDevice(const std::string& master_ref, const std::string& self_ref) {
    return RemoteClient::RegisterDevice(master_ref, self_ref);
}

std::vector<WalEntryInfo> GrpcDirectTransport::updateDB(const std::string& master_ref, uint64_t last_seq_id, uint64_t device_id) {
    return RemoteClient::UpdateDB(master_ref, last_seq_id, device_id);
}

bool GrpcDirectTransport::pushWAL(const std::string& master_ref, uint64_t device_id, const std::vector<WalEntryInfo>& entries, std::vector<uint64_t>& out_assigned_seq_ids) {
    return RemoteClient::PushWAL(master_ref, device_id, entries, out_assigned_seq_ids);
}

void GrpcDirectTransport::downloadFile(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id, const std::string& destination_path) {
    RemoteClient::DownloadFile(owner_ref, object_hash, requester_device_id, destination_path);
}

uint64_t GrpcDirectTransport::getObjectSize(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id) {
    return RemoteClient::GetObjectSize(owner_ref, object_hash, requester_device_id);
}

void GrpcDirectTransport::downloadFileRange(const std::string& owner_ref, const std::string& object_hash, uint64_t requester_device_id, uint64_t offset, uint64_t size, const std::string& destination_path) {
    RemoteClient::DownloadFileRange(owner_ref, object_hash, requester_device_id, offset, size, destination_path);
}

} // namespace pear::net
