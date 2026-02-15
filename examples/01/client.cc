#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

#include "chat.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

int main() {
  auto channel = grpc::CreateChannel("127.0.0.1:50051",
                                     grpc::InsecureChannelCredentials());
  auto stub = demo::Chat::NewStub(channel);

  demo::SubscribeRequest req;
  req.set_client_id("client-1");

  ClientContext ctx;
  std::unique_ptr<grpc::ClientReader<demo::ChatMessage>> reader =
      stub->Subscribe(&ctx, req);

  demo::ChatMessage msg;
  while (reader->Read(&msg)) {
    std::cout << "From server: " << msg.text() << "\n";
  }

  Status s = reader->Finish();
  if (!s.ok()) {
    std::cerr << "Stream ended with error: " << s.error_message() << "\n";
    return 1;
  }
  return 0;
}
