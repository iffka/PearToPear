#include <pear/net/node.hpp>

#include <pear/net/master_service.hpp>
#include <pear/net/storage_service.hpp>

#include <utility>

namespace pear::net {

Node::Node(std::shared_ptr<pear::db::SqliteDatabase> db, std::shared_ptr<pear::storage::Workspace> workspace, bool is_master)
    : db_(std::move(db)), workspace_(std::move(workspace)), is_master_(is_master) {}

Node::~Node() {
    stop();
}

void Node::start(const std::string& listen_address, bool storage_only) {
    if (running_) {
        return;
    }

    server_thread_ = std::thread(&Node::runServerThread, this, listen_address, storage_only);
}

void Node::stop() {
    if (server_) {
        server_->Shutdown();
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    running_ = false;
}

bool Node::is_running() const {
    return running_;
}

void Node::runServerThread(const std::string& address, bool storage_only) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());

    if (!storage_only && is_master_) {
        master_service_ = std::make_unique<MasterServiceImpl>(db_);
        builder.RegisterService(master_service_.get());
    }

    storage_service_ = std::make_unique<StorageServiceImpl>(workspace_);
    builder.RegisterService(storage_service_.get());

    server_ = builder.BuildAndStart();
    running_ = true;
    server_->Wait();
    running_ = false;
}

} // namespace pear::net