#ifndef PEAR_NET_NODE_HPP_
#define PEAR_NET_NODE_HPP_

#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "db_contract.hpp"
#include "fs_contract.hpp"

namespace pear::net {

class MasterServiceImpl;
class StorageServiceImpl;

class Node {
public:
    Node(std::shared_ptr<DatabaseFacade> db,
         std::shared_ptr<FilesystemFacade> fs,
         bool is_master = false);

    ~Node();

    void start(const std::string& listen_address, bool storage_only = false);
    void stop();
    bool is_running() const;

private:
    void runServerThread(const std::string& address, bool storage_only);

    std::shared_ptr<DatabaseFacade> db_;
    std::shared_ptr<FilesystemFacade> fs_;
    bool is_master_;

    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<MasterServiceImpl> master_service_;
    std::unique_ptr<StorageServiceImpl> storage_service_;
    std::thread server_thread_;
    bool running_ = false;
};

} // namespace pear::net

#endif // PEAR_NET_NODE_HPP_