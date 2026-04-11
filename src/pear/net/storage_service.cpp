#include <pear/net/storage_service.hpp>

#include <fstream>

namespace pear::net {

StorageServiceImpl::StorageServiceImpl(std::shared_ptr<FilesystemFacade> fs)
    : fs_(std::move(fs)) {}

grpc::Status StorageServiceImpl::DownloadFile(grpc::ServerContext* /*ctx*/,
                                              const DownloadRequest* req,
                                              grpc::ServerWriter<FileChunk>* writer) {
    try {
        std::string file_id = req->file_id();
        uint64_t version = req->version();
        uint64_t requester = req->requester_device_id();

        auto obj_path = fs_->getObjectPath(file_id, version);
        if (obj_path.empty() || !std::filesystem::exists(obj_path)) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Object not found");
        }

        fs_->incrementDownloadCounter(file_id, version);

        const size_t chunk_size = 64 * 1024; // 64KB
        std::ifstream file(obj_path, std::ios::binary);
        if (!file) {
            fs_->decrementDownloadCounter(file_id, version);
            return grpc::Status(grpc::StatusCode::INTERNAL, "Cannot open file");
        }

        uint64_t offset = 0;
        char buffer[chunk_size];
        while (file.read(buffer, chunk_size) || file.gcount() > 0) {
            FileChunk chunk;
            chunk.set_data(buffer, file.gcount());
            chunk.set_offset(offset);
            chunk.set_last_chunk(file.peek() == EOF);
            if (!writer->Write(chunk)) {
                break;
            }
            offset += file.gcount();
        }

        fs_->decrementDownloadCounter(file_id, version);
        fs_->tryDeleteOldVersion(file_id, version);

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

} // namespace pear::net