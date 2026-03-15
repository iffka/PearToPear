#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include "request.grpc.pb.h"

namespace fs = std::filesystem;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using peartopeer::P2PService;
using peartopeer::ClientRequest;
using peartopeer::ServerFeedback;
using peartopeer::FileRequest;
using peartopeer::FileResponse;

class P2PServiceImpl final : public P2PService::Service {
public:
    P2PServiceImpl(const std::string& storage_path) : storage_path_(storage_path) {
        fs::path files_dir = fs::path(storage_path_) / "files";
        fs::create_directories(files_dir);
    }

    Status SendRequest(ServerContext* context,
                       const ClientRequest* request,
                       ServerFeedback* feedback) override {
        std::string log_entry = request->text();
        std::cout << "Received: " << log_entry << std::endl;

        fs::path log_path = fs::path(storage_path_) / "log.txt";
        std::ofstream log_file(log_path, std::ios_base::app);
        if (log_file.is_open()) {
            log_file << log_entry << std::endl;
            log_file.close();
        } else {
            std::cerr << "Failed to open log file" << std::endl;
        }

        feedback->set_message("Your request has been logged.");
        feedback->set_success(true);

        return Status::OK;
    }

    Status DownloadFile(ServerContext* context,
                        const FileRequest* request,
                        FileResponse* response) override {
        std::string filename = request->filename();
        std::cout << "Download request for: " << filename << std::endl;

        fs::path db_path = fs::path(storage_path_) / "database.txt";
        bool found_in_db = false;
        std::ifstream db_file(db_path);
        if (db_file.is_open()) {
            std::string line;
            while (std::getline(db_file, line)) {
                if (line == filename) {
                    found_in_db = true;
                    break;
                }
            }
            db_file.close();
        }

        fs::path file_path = fs::path(storage_path_) / "files" / filename;
        bool file_exists = fs::exists(file_path) && fs::is_regular_file(file_path);

        if (file_exists && found_in_db) {
            std::ifstream file(file_path, std::ios::binary);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                response->set_content(content);
                response->set_success(true);
                std::cout << "File " << filename << " sent successfully." << std::endl;
            } else {
                response->set_success(false);
                response->set_error_message("Failed to open file");
                std::cerr << "Failed to open file: " << file_path << std::endl;
            }
        } else {
            response->set_success(false);
            response->set_error_message("File not found");
            std::cout << "File " << filename << " not found." << std::endl;
        }
        return Status::OK;
    }

private:
    std::string storage_path_;
};

void RunServer(const std::string& storage_path) {
    std::string server_address("0.0.0.0:9090");
    P2PServiceImpl service(storage_path);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    std::cout << "Storage path: " << storage_path << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    std::string storage_path = ".";
    if (argc > 1) {
        storage_path = argv[1];
    }
    RunServer(storage_path);
    return 0;
}