#ifndef PEAR_NET_MASTER_SERVICE_HPP_
#define PEAR_NET_MASTER_SERVICE_HPP_

#include <memory>

#include <grpcpp/grpcpp.h>

#include <pear/db/sqlite_database.hpp>

#include "p2p.grpc.pb.h"

namespace pear::net {

class MasterServiceImpl final : public pear::net::Master::Service {
public:
    explicit MasterServiceImpl(std::shared_ptr<pear::db::SqliteDatabase> db);

    grpc::Status RegisterDevice(grpc::ServerContext* ctx, const RegisterRequest* req, RegisterResponse* resp) override;
    grpc::Status UpdateDB(grpc::ServerContext* ctx, const UpdateDBRequest* req, UpdateDBResponse* resp) override;
    grpc::Status PushWAL(grpc::ServerContext* ctx, const PushWALRequest* req, PushWALResponse* resp) override;

private:
    std::shared_ptr<pear::db::SqliteDatabase> db_;
};

} // namespace pear::net

#endif // PEAR_NET_MASTER_SERVICE_HPP_