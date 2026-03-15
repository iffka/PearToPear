#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include "request.grpc.pb.h"

namespace fs = std::filesystem;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using peartopeer::P2PService;
using peartopeer::ClientRequest;
using peartopeer::ServerFeedback;
using peartopeer::FileRequest;
using peartopeer::FileResponse;

class P2PClient {
public:
    P2PClient(std::shared_ptr<Channel> channel, const std::string& storage_path)
        : stub_(P2PService::NewStub(channel)), storage_path_(storage_path) {
        fs::path files_dir = fs::path(storage_path_) / "files";
        fs::create_directories(files_dir);
    }

    void SendTextRequest(const std::string& text) {
        ClientRequest request;
        request.set_text(text);

        ServerFeedback feedback;
        ClientContext context;

        Status status = stub_->SendRequest(&context, request, &feedback);

        if (status.ok()) {
            std::cout << "Server responded: " << feedback.message()
                      << " (success: " << feedback.success() << ")" << std::endl;
        } else {
            std::cerr << "RPC failed: " << status.error_code() << ": " << status.error_message() << std::endl;
        }
    }

    void DownloadFile(const std::string& filename) {
        FileRequest request;
        request.set_filename(filename);

        FileResponse response;
        ClientContext context;

        Status status = stub_->DownloadFile(&context, request, &response);

        if (status.ok()) {
            if (response.success()) {
                fs::path file_path = fs::path(storage_path_) / "files" / filename;
                std::ofstream out_file(file_path, std::ios::binary);
                if (out_file) {
                    out_file.write(response.content().data(), response.content().size());
                    out_file.close();
                    std::cout << "File " << filename << " downloaded successfully." << std::endl;

                    fs::path db_path = fs::path(storage_path_) / "database.txt";
                    std::ofstream db_file(db_path, std::ios_base::app);
                    if (db_file) {
                        db_file << filename << std::endl;
                        db_file.close();
                    } else {
                        std::cerr << "Failed to update database.txt" << std::endl;
                    }
                } else {
                    std::cerr << "Failed to save file: " << filename << std::endl;
                }
            } else {
                std::cout << "Download failed: " << response.error_message() << std::endl;
            }
        } else {
            std::cerr << "RPC failed: " << status.error_code() << ": " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<P2PService::Stub> stub_;
    std::string storage_path_;
};

void PrintUsage() {
    std::cout << "Commands:\n"
              << "  log <text>          - send a text request\n"
              << "  download <filename> - download a file\n"
              << "  exit                - quit\n";
}

int main(int argc, char** argv) {
    std::string storage_path = ".";
    if (argc > 1) {
        storage_path = argv[1];
    }

    std::string target_str = "localhost:9090";
    auto channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    P2PClient client(channel, storage_path);

    std::cout << "Client started. Storage path: " << storage_path << std::endl;
    PrintUsage();

    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        if (line == "exit") break;

        if (line.substr(0, 4) == "log ") {
            std::string text = line.substr(4);
            client.SendTextRequest(text);
        } else if (line.substr(0, 9) == "download ") {
            std::string filename = line.substr(9);
            client.DownloadFile(filename);
        } else {
            std::cout << "Unknown command. ";
            PrintUsage();
        }
    }

    return 0;
}