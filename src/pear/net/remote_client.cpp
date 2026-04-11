#include <pear/net/remote_client.hpp>

#include <grpcpp/grpcpp.h>
#include <fstream>

#include "p2p.grpc.pb.h"

namespace pear::net {

uint64_t RemoteClient::RegisterDevice(const std::string& gu_address, const std::string& my_address) {
    auto channel = grpc::CreateChannel(gu_address, grpc::InsecureChannelCredentials());
    auto stub = Master::NewStub(channel);
    RegisterRequest req;
    req.set_address(my_address);
    RegisterResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub->RegisterDevice(&ctx, req, &resp);
    if (!status.ok() || !resp.success()) {
        throw std::runtime_error("RegisterDevice failed: " + resp.error_message());
    }
    return resp.assigned_device_id();
}

std::vector<WalEntryInfo> RemoteClient::UpdateDB(const std::string& gu_address,
                                                  uint64_t last_seq_id,
                                                  uint64_t device_id) {
    auto channel = grpc::CreateChannel(gu_address, grpc::InsecureChannelCredentials());
    auto stub = Master::NewStub(channel);
    UpdateDBRequest req;
    req.set_last_seq_id(last_seq_id);
    req.set_device_id(device_id);
    UpdateDBResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub->UpdateDB(&ctx, req, &resp);
    if (!status.ok() || !resp.success()) {
        throw std::runtime_error("UpdateDB failed: " + resp.error_message());
    }

    std::vector<WalEntryInfo> entries;
    for (const auto& e : resp.entries()) {
        WalEntryInfo info;
        info.seq_id = e.seq_id();
        info.timestamp = e.timestamp();
        info.op_type = static_cast<int>(e.op_type());
        if (e.has_file_update()) {
            info.file.file_id = e.file_update().file_id();
            info.file.name = e.file_update().name();
            info.file.version = e.file_update().version();
            info.file.owner_device_id = e.file_update().owner_device_id();
        } else if (e.has_device_update()) {
            info.device.device_id = e.device_update().device_id();
            info.device.address = e.device_update().address();
        }
        entries.push_back(info);
    }
    return entries;
}

bool RemoteClient::PushWAL(const std::string& gu_address,
                           uint64_t device_id,
                           const std::vector<WalEntryInfo>& entries,
                           std::vector<uint64_t>& out_assigned_seq_ids) {
    auto channel = grpc::CreateChannel(gu_address, grpc::InsecureChannelCredentials());
    auto stub = Master::NewStub(channel);
    PushWALRequest req;
    req.set_device_id(device_id);
    for (const auto& e : entries) {
        auto* pe = req.add_entries();
        pe->set_seq_id(0);
        pe->set_timestamp(e.timestamp);
        pe->set_op_type(static_cast<WalOpType>(e.op_type));
        if (e.op_type == 0) {
            auto* fu = pe->mutable_file_update();
            fu->set_file_id(e.file.file_id);
            fu->set_name(e.file.name);
            fu->set_version(e.file.version);
            fu->set_owner_device_id(e.file.owner_device_id);
        } else if (e.op_type == 1) {
            auto* du = pe->mutable_device_update();
            du->set_device_id(e.device.device_id);
            du->set_address(e.device.address);
        }
    }
    PushWALResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub->PushWAL(&ctx, req, &resp);
    if (status.ok() && resp.success()) {
        out_assigned_seq_ids.clear();
        for (auto id : resp.assigned_seq_ids()) {
            out_assigned_seq_ids.push_back(id);
        }
        return true;
    }
    return false;
}

void RemoteClient::DownloadFile(const std::string& vu_address,
                                const std::string& file_id,
                                uint64_t version,
                                uint64_t requester_device_id,
                                const std::string& destination_path) {
    auto channel = grpc::CreateChannel(vu_address, grpc::InsecureChannelCredentials());
    auto stub = Storage::NewStub(channel);
    DownloadRequest req;
    req.set_file_id(file_id);
    req.set_version(version);
    req.set_requester_device_id(requester_device_id);
    grpc::ClientContext ctx;
    auto reader = stub->DownloadFile(&ctx, req);
    FileChunk chunk;
    std::ofstream out(destination_path, std::ios::binary);
    while (reader->Read(&chunk)) {
        out.write(chunk.data().data(), chunk.data().size());
    }
    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        throw std::runtime_error("DownloadFile failed: " + status.error_message());
    }
}

} // namespace pear::net