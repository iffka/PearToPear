#include <pear/net/storage_service.hpp>

#include <cstdint>
#include <fstream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <memory>
#include <mutex>
#include <shared_mutex>


namespace pear::net {

namespace {

grpc::Status stream_object_range(const std::filesystem::path& object_path, uint64_t offset, uint64_t size, grpc::ServerWriter<FileChunk>* writer) {
    std::ifstream file(object_path, std::ios::binary);
    if (!file) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Cannot open file");
    }

    file.seekg(0, std::ios::end);
    const uint64_t object_size = static_cast<uint64_t>(file.tellg());
    if (offset > object_size) {
        return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "Offset is out of range");
    }

    const uint64_t available_size = object_size - offset;
    uint64_t remaining_size = size == 0 ? available_size : std::min(size, available_size);

    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

    constexpr std::size_t chunk_size = 64 * 1024;
    char buffer[chunk_size];
    uint64_t current_offset = offset;

    while (remaining_size > 0) {
        const std::size_t bytes_to_read = static_cast<std::size_t>(std::min<uint64_t>(chunk_size, remaining_size));
        file.read(buffer, static_cast<std::streamsize>(bytes_to_read));
        const std::streamsize bytes_read = file.gcount();

        if (bytes_read <= 0) {
            break;
        }

        FileChunk chunk;
        chunk.set_data(buffer, static_cast<std::size_t>(bytes_read));
        chunk.set_offset(current_offset);
        chunk.set_last_chunk(static_cast<uint64_t>(bytes_read) == remaining_size);

        if (!writer->Write(chunk)) {
            break;
        }

        current_offset += static_cast<uint64_t>(bytes_read);
        remaining_size -= static_cast<uint64_t>(bytes_read);
    }

    return grpc::Status::OK;
}

} // namespace

StorageServiceImpl::StorageServiceImpl(std::shared_ptr<pear::storage::Workspace> workspace) : workspace_(std::move(workspace)) {}

std::shared_ptr<std::shared_mutex> StorageServiceImpl::getObjectLock(const std::string& object_hash) {
    std::lock_guard<std::mutex> lock(object_locks_mutex_);

    auto& object_lock = object_locks_[object_hash];
    if (!object_lock) {
        object_lock = std::make_shared<std::shared_mutex>();
    }

    return object_lock;
}

grpc::Status StorageServiceImpl::DownloadFile(grpc::ServerContext* /*ctx*/, const DownloadRequest* req, grpc::ServerWriter<FileChunk>* writer) {
    try {
        const std::string object_hash = req->object_hash();
        const auto object_lock = getObjectLock(object_hash);
        std::shared_lock<std::shared_mutex> lock(*object_lock);

        if (!workspace_->has_objectfile(object_hash)) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Object not found");
        }

        const std::filesystem::path object_path = workspace_->get_objectfile_path(object_hash);
        return stream_object_range(object_path, 0, 0, writer);
    } catch (const std::exception& exception) {
        return grpc::Status(grpc::StatusCode::INTERNAL, exception.what());
    }
}

grpc::Status StorageServiceImpl::GetObjectInfo(grpc::ServerContext* /*ctx*/, const ObjectInfoRequest* req, ObjectInfoResponse* resp) {
    try {
        const std::string object_hash = req->object_hash();
        const auto object_lock = getObjectLock(object_hash);
        std::shared_lock<std::shared_mutex> lock(*object_lock);

        if (!workspace_->has_objectfile(object_hash)) {
            resp->set_success(false);
            resp->set_error_message("Object not found");
            return grpc::Status::OK;
        }

        const std::filesystem::path object_path = workspace_->get_objectfile_path(object_hash);

        resp->set_success(true);
        resp->set_size(static_cast<uint64_t>(std::filesystem::file_size(object_path)));
        return grpc::Status::OK;
    } catch (const std::exception& exception) {
        resp->set_success(false);
        resp->set_error_message(exception.what());
        return grpc::Status::OK;
    }
}

grpc::Status StorageServiceImpl::DownloadFileRange(grpc::ServerContext* /*ctx*/, const DownloadRangeRequest* req, grpc::ServerWriter<FileChunk>* writer) {
    try {
        const std::string object_hash = req->object_hash();
        const auto object_lock = getObjectLock(object_hash);
        std::shared_lock<std::shared_mutex> lock(*object_lock);

        if (!workspace_->has_objectfile(object_hash)) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Object not found");
        }

        const std::filesystem::path object_path = workspace_->get_objectfile_path(object_hash);
        return stream_object_range(object_path, req->offset(), req->size(), writer);
    } catch (const std::exception& exception) {
        return grpc::Status(grpc::StatusCode::INTERNAL, exception.what());
    }
}

grpc::Status StorageServiceImpl::DeleteObject(grpc::ServerContext* /*ctx*/, const DeleteObjectRequest* req, DeleteObjectResponse* resp) {
    try {
        const std::string object_hash = req->object_hash();
        const auto object_lock = getObjectLock(object_hash);
        std::unique_lock<std::shared_mutex> lock(*object_lock, std::try_to_lock);

        if (!lock.owns_lock()) {
            resp->set_success(false);
            resp->set_busy(true);
            resp->set_error_message("Object is busy");
            return grpc::Status::OK;
        }

        if (!workspace_->has_objectfile(object_hash)) {
            resp->set_success(true);
            resp->set_busy(false);
            return grpc::Status::OK;
        }

        workspace_->delete_objectfile(object_hash);

        resp->set_success(true);
        resp->set_busy(false);
        return grpc::Status::OK;
    } catch (const std::exception& exception) {
        resp->set_success(false);
        resp->set_busy(false);
        resp->set_error_message(exception.what());
        return grpc::Status::OK;
    }
}

}  // namespace pear::net