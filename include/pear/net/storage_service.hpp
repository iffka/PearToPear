#ifndef PEAR_NET_STORAGE_SERVICE_HPP_
#define PEAR_NET_STORAGE_SERVICE_HPP_

#include <memory>

#include <grpcpp/grpcpp.h>

#include "p2p.grpc.pb.h"
#include "fs_contract.hpp"

namespace pear::net {

class StorageServiceImpl final : public pear::net::Storage::Service {
public:
    explicit StorageServiceImpl(std::shared_ptr<FilesystemFacade> fs);

    grpc::Status DownloadFile(grpc::ServerContext* ctx,
                              const DownloadRequest* req,
                              grpc::ServerWriter<FileChunk>* writer) override;

private:
    std::shared_ptr<FilesystemFacade> fs_;
};

} // namespace pear::net

#endif // PEAR_NET_STORAGE_SERVICE_HPP_