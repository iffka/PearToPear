#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>

#include "chat.grpc.pb.h"

using grpc::ClientContext;
using grpc::Status;

int main(int argc, char** argv) {
  std::string client_id = (argc >= 2 ? argv[1] : "client");

  auto channel = grpc::CreateChannel("127.0.0.1:50051", grpc::InsecureChannelCredentials());

  auto stub = demo::Broadcast::NewStub(channel);

  demo::SubscribeRequest req;
  req.set_client_id(client_id);

  ClientContext ctx;

  auto reader = stub->Subscribe(&ctx, req);

  demo::ServerMessage msg;
  while (reader->Read(&msg)) {
    std::cout << "#" << msg.seq() << " " << msg.text() << "\n";
  }

  Status s = reader->Finish();
  if (!s.ok()) {
    std::cerr << "Subscribe ended with error: " << s.error_message() << "\n";
    return 1;
  }
  return 0;
}
