#include <pear/net/storage_service.hpp>

#include <fstream>
#include <string>

namespace pear::net {

namespace {

std::string make_object_name(const std::string& file_id, uint64_t version) {
    return file_id + "." + std::to_string(version);
}

} // namespace

StorageServiceImpl::StorageServiceImpl(std::shared_ptr<pear::storage::Workspace> workspace) : workspace_(std::move(workspace)) {}

grpc::Status StorageServiceImpl::DownloadFile(grpc::ServerContext* /*ctx*/, const DownloadRequest* req, grpc::ServerWriter<FileChunk>* writer) {
    try {
        const std::string file_id = req->file_id();
        const uint64_t version = req->version();
        const std::string object_name = make_object_name(file_id, version);

        if (!workspace_->has_objectfile(object_name)) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Object not found");
        }

        const std::filesystem::path object_path =
            workspace_->get_objectfile_path(object_name);

        std::ifstream file(object_path, std::ios::binary);
        if (!file) {
            return grpc::Status(
                grpc::StatusCode::INTERNAL,
                "Cannot open file"
            );
        }

        constexpr std::size_t chunk_size = 64 * 1024;
        std::uint64_t offset = 0;
        char buffer[chunk_size];

        while (file.read(buffer, chunk_size) || file.gcount() > 0) {
            FileChunk chunk;
            chunk.set_data(buffer, static_cast<int>(file.gcount()));
            chunk.set_offset(offset);
            chunk.set_last_chunk(file.peek() == EOF);

            if (!writer->Write(chunk)) {
                break;
            }

            offset += static_cast<std::uint64_t>(file.gcount());
        }

        return grpc::Status::OK;
    } catch (const std::exception& exception) {
        return grpc::Status(grpc::StatusCode::INTERNAL, exception.what());
    }
}

} // namespace pear::net