#ifndef PEAR_NET_STORAGE_SERVICE_HPP_
#define PEAR_NET_STORAGE_SERVICE_HPP_

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <grpcpp/grpcpp.h>

#include <pear/fs/workspace.hpp>

#include "p2p.grpc.pb.h"

namespace pear::net {

class StorageServiceImpl final : public pear::net::Storage::Service {
public:
    explicit StorageServiceImpl(std::shared_ptr<pear::storage::Workspace> workspace);

    grpc::Status DownloadFile( grpc::ServerContext* ctx, const DownloadRequest* req, grpc::ServerWriter<FileChunk>* writer) override;
    grpc::Status GetObjectInfo(grpc::ServerContext* ctx, const ObjectInfoRequest* req, ObjectInfoResponse* resp) override;
    grpc::Status DownloadFileRange(grpc::ServerContext* ctx, const DownloadRangeRequest* req, grpc::ServerWriter<FileChunk>* writer) override;
    grpc::Status DeleteObject(grpc::ServerContext* ctx, const DeleteObjectRequest* req, DeleteObjectResponse* resp) override;

private:
    std::shared_ptr<pear::storage::Workspace> workspace_;
    std::shared_ptr<std::shared_mutex> getObjectLock(const std::string& object_hash);
    std::mutex object_locks_mutex_;
    std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> object_locks_;

};

} // namespace pear::net

#endif // PEAR_NET_STORAGE_SERVICE_HPP_