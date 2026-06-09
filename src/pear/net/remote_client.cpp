#include <pear/net/remote_client.hpp>

#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <fstream>
#include <stdexcept>
#include <utility>
#include <chrono>
#include <filesystem>

#include "p2p.grpc.pb.h"

namespace pear::net {

namespace {

void fillProtoWalEntry(WalEntry* proto_entry, const WalEntryInfo& entry) {
    proto_entry->set_seq_id(entry.seq_id);
    proto_entry->set_timestamp(entry.timestamp);
    proto_entry->set_op_type(static_cast<WalOpType>(entry.op_type));

    if (entry.op_type == WalOpTypeInfo::kFileUpdate) {
        auto* file_update = proto_entry->mutable_file_update();
        file_update->set_path(entry.file.path);
        file_update->set_object_hash(entry.file.object_hash);
        file_update->set_version(entry.file.version);
        file_update->set_owner_device_id(entry.file.owner_device_id);
        file_update->set_read_only(entry.file.read_only);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kFileDelete) {
        auto* file_delete = proto_entry->mutable_file_delete();
        file_delete->set_path(entry.file_delete.path);
        file_delete->set_version(entry.file_delete.version);
        file_delete->set_owner_device_id(entry.file_delete.owner_device_id);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kDeviceUpdate) {
        auto* device_update = proto_entry->mutable_device_update();
        device_update->set_device_id(entry.device.device_id);
        device_update->set_address(entry.device.address);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kObjectOwnerUpdate) {
        auto* object_owner_update = proto_entry->mutable_object_owner_update();
        object_owner_update->set_object_hash(entry.object_owner.object_hash);
        object_owner_update->set_owner_device_id(entry.object_owner.owner_device_id);
        return;
    }

    if (entry.op_type == WalOpTypeInfo::kObjectOwnerDelete) {
        auto* object_owner_delete = proto_entry->mutable_object_owner_delete();
        object_owner_delete->set_object_hash(entry.object_owner.object_hash);
        object_owner_delete->set_owner_device_id(entry.object_owner.owner_device_id);
    }
}

WalEntryInfo parseProtoWalEntry(const WalEntry& proto_entry) {
    WalEntryInfo entry;

    entry.seq_id = proto_entry.seq_id();
    entry.timestamp = proto_entry.timestamp();
    entry.op_type = static_cast<WalOpTypeInfo>(proto_entry.op_type());

    if (proto_entry.has_file_update()) {
        entry.file.path = proto_entry.file_update().path();
        entry.file.object_hash = proto_entry.file_update().object_hash();
        entry.file.version = proto_entry.file_update().version();
        entry.file.owner_device_id = proto_entry.file_update().owner_device_id();
        entry.file.read_only = proto_entry.file_update().read_only();
        return entry;
    }

    if (proto_entry.has_file_delete()) {
        entry.file_delete.path = proto_entry.file_delete().path();
        entry.file_delete.version = proto_entry.file_delete().version();
        entry.file_delete.owner_device_id = proto_entry.file_delete().owner_device_id();
        return entry;
    }

    if (proto_entry.has_device_update()) {
        entry.device.device_id = proto_entry.device_update().device_id();
        entry.device.address = proto_entry.device_update().address();
        return entry;
    }

    if (proto_entry.has_object_owner_update()) {
        entry.object_owner.object_hash = proto_entry.object_owner_update().object_hash();
        entry.object_owner.owner_device_id = proto_entry.object_owner_update().owner_device_id();
        return entry;
    }

    if (proto_entry.has_object_owner_delete()) {
        entry.object_owner.object_hash = proto_entry.object_owner_delete().object_hash();
        entry.object_owner.owner_device_id = proto_entry.object_owner_delete().owner_device_id();
    }

    return entry;
}

} // namespace

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

std::vector<WalEntryInfo> RemoteClient::UpdateDB(
    const std::string& gu_address,
    uint64_t last_seq_id,
    uint64_t device_id
) {
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

    for (const auto& proto_entry : resp.entries()) {
        entries.push_back(parseProtoWalEntry(proto_entry));
    }
    return entries;
}

bool RemoteClient::PushWAL(
    const std::string& gu_address,
    uint64_t device_id,
    const std::vector<WalEntryInfo>& entries,
    std::vector<uint64_t>& out_assigned_seq_ids
) {
    auto channel = grpc::CreateChannel(gu_address, grpc::InsecureChannelCredentials());
    auto stub = Master::NewStub(channel);
    PushWALRequest req;
    req.set_device_id(device_id);

    for (const auto& entry : entries) {
        auto* proto_entry = req.add_entries();
        fillProtoWalEntry(proto_entry, entry);
        proto_entry->set_seq_id(0);
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

void RemoteClient::DownloadFile(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id, const std::string& destination_path) {
    namespace fs = std::filesystem;

    const fs::path destination(destination_path);
    const fs::path temp_path = destination.parent_path() / (destination.filename().string() + ".download." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    auto remove_temp_file = [&]() {
        std::error_code error;
        fs::remove(temp_path, error);
    };

    auto channel = grpc::CreateChannel(vu_address, grpc::InsecureChannelCredentials());
    auto stub = Storage::NewStub(channel);

    DownloadRequest req;
    req.set_object_hash(object_hash);
    req.set_requester_device_id(requester_device_id);

    grpc::ClientContext ctx;
    auto reader = stub->DownloadFile(&ctx, req);

    FileChunk chunk;
    std::ofstream out(temp_path, std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("DownloadFile failed: failed to open temp file");
    }

    while (reader->Read(&chunk)) {
        out.write(chunk.data().data(), static_cast<std::streamsize>(chunk.data().size()));
        if (!out.good()) {
            remove_temp_file();
            throw std::runtime_error("DownloadFile failed: failed to write temp file");
        }
    }

    out.close();
    if (!out.good()) {
        remove_temp_file();
        throw std::runtime_error("DownloadFile failed: failed to close temp file");
    }

    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        remove_temp_file();
        throw std::runtime_error("DownloadFile failed: " + status.error_message());
    }

    std::error_code remove_error;
    fs::remove(destination, remove_error);

    std::error_code rename_error;
    fs::rename(temp_path, destination, rename_error);
    if (rename_error) {
        remove_temp_file();
        throw std::runtime_error("DownloadFile failed: failed to move temp file");
    }
}

uint64_t RemoteClient::GetObjectSize(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id) {
    auto channel = grpc::CreateChannel(vu_address, grpc::InsecureChannelCredentials());
    auto stub = Storage::NewStub(channel);

    ObjectInfoRequest req;
    req.set_object_hash(object_hash);
    req.set_requester_device_id(requester_device_id);

    ObjectInfoResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub->GetObjectInfo(&ctx, req, &resp);

    if (!status.ok() || !resp.success()) {
        throw std::runtime_error("GetObjectSize failed: " + resp.error_message());
    }

    return resp.size();
}

void RemoteClient::DownloadFileRange(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id, uint64_t offset, uint64_t size, const std::string& destination_path) {
    namespace fs = std::filesystem;

    const fs::path destination(destination_path);
    const fs::path temp_path = destination.parent_path() / (destination.filename().string() + ".download." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    auto remove_temp_file = [&]() {
        std::error_code error;
        fs::remove(temp_path, error);
    };

    auto channel = grpc::CreateChannel(vu_address, grpc::InsecureChannelCredentials());
    auto stub = Storage::NewStub(channel);

    DownloadRangeRequest req;
    req.set_object_hash(object_hash);
    req.set_requester_device_id(requester_device_id);
    req.set_offset(offset);
    req.set_size(size);

    grpc::ClientContext ctx;
    auto reader = stub->DownloadFileRange(&ctx, req);

    FileChunk chunk;
    std::ofstream out(temp_path, std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("DownloadFileRange failed: failed to open temp file");
    }

    while (reader->Read(&chunk)) {
        out.write(chunk.data().data(), static_cast<std::streamsize>(chunk.data().size()));
        if (!out.good()) {
            remove_temp_file();
            throw std::runtime_error("DownloadFileRange failed: failed to write temp file");
        }
    }

    out.close();
    if (!out.good()) {
        remove_temp_file();
        throw std::runtime_error("DownloadFileRange failed: failed to close temp file");
    }

    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        remove_temp_file();
        throw std::runtime_error("DownloadFileRange failed: " + status.error_message());
    }

    std::error_code remove_error;
    fs::remove(destination, remove_error);

    std::error_code rename_error;
    fs::rename(temp_path, destination, rename_error);
    if (rename_error) {
        remove_temp_file();
        throw std::runtime_error("DownloadFileRange failed: failed to move temp file");
    }
}

bool RemoteClient::DeleteObject(const std::string& vu_address, const std::string& object_hash, uint64_t requester_device_id) {
    auto channel = grpc::CreateChannel(vu_address, grpc::InsecureChannelCredentials());
    auto stub = Storage::NewStub(channel);

    DeleteObjectRequest req;
    req.set_object_hash(object_hash);
    req.set_requester_device_id(requester_device_id);

    DeleteObjectResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub->DeleteObject(&ctx, req, &resp);

    if (!status.ok()) {
        throw std::runtime_error("DeleteObject failed: " + status.error_message());
    }

    if (resp.busy()) {
        return false;
    }

    if (!resp.success()) {
        throw std::runtime_error("DeleteObject failed: " + resp.error_message());
    }

    return true;
}

} // namespace pear::net