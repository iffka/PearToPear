#ifndef PEAR_NET_REPLICA_SERVICE_HPP_
#define PEAR_NET_REPLICA_SERVICE_HPP_

#include "p2p.grpc.pb.h"

#include <memory>
#include <mutex>

namespace pear::db {
class SqliteDatabase;
}

namespace pear::net {

class ReplicaServiceImpl final : public Replica::Service {
public:
    explicit ReplicaServiceImpl(std::shared_ptr<pear::db::SqliteDatabase> db);

    grpc::Status Prepare(grpc::ServerContext* ctx, const PrepareRequest* req, PrepareResponse* resp) override;
    grpc::Status Commit(grpc::ServerContext* ctx, const CommitRequest* req, CommitResponse* resp) override;
    grpc::Status GetReplicaState(grpc::ServerContext* ctx, const ReplicaStateRequest* req, ReplicaStateResponse* resp) override;

private:
    std::shared_ptr<pear::db::SqliteDatabase> db_;
    std::mutex db_mutex_;
};

} // namespace pear::net

#endif // PEAR_NET_REPLICA_SERVICE_HPP_