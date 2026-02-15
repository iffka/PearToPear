#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

#include "chat.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

class ChatService final : public demo::Chat::Service {
public:
  Status Subscribe(ServerContext* ctx,
                   const demo::SubscribeRequest* req,
                   ServerWriter<demo::ChatMessage>* writer) override {
    std::cerr << "Client subscribed: " << req->client_id() << "\n";
    std::cerr << "Type lines in server terminal, they will be sent to client.\n";

    std::string line;
    while (!ctx->IsCancelled() && std::getline(std::cin, line)) {
      demo::ChatMessage msg;
      msg.set_text(line);
      if (!writer->Write(msg)) {
        // клиент отключился
        break;
      }
    }
    return Status::OK;
  }
};

int main() {
  std::string addr = "0.0.0.0:50051";
  ChatService service;

  ServerBuilder builder;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << addr << "\n";
  server->Wait();
}
