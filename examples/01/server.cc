#include <grpcpp/grpcpp.h>

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "chat.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

struct Bus {
  std::mutex m;
  std::condition_variable cv;
  std::vector<std::string> msgs;
  bool closed = false;

  void publish(std::string s) {
    {
      std::lock_guard<std::mutex> lk(m);
      msgs.push_back(std::move(s));
    }
    cv.notify_all();
  }
};

class ChatService final : public demo::Chat::Service {
public:
  explicit ChatService(Bus& bus) : bus_(bus) {}

  Status Subscribe(ServerContext* ctx,
                   const demo::SubscribeRequest* req,
                   ServerWriter<demo::ChatMessage>* writer) override {
    std::cerr << "Client subscribed: " << req->client_id() << "\n";
    size_t idx;
    {
      std::lock_guard<std::mutex> lk(bus_.m);
      idx = bus_.msgs.size();
    }

    demo::ChatMessage out;

    while (!ctx->IsCancelled()) {
      std::unique_lock<std::mutex> lk(bus_.m);

      bus_.cv.wait(lk, [&] {
        return ctx->IsCancelled() || bus_.closed || idx < bus_.msgs.size();
      });

      if (ctx->IsCancelled()) break;

      while (idx < bus_.msgs.size()) {
        std::string s = bus_.msgs[idx++];
        lk.unlock();

        out.set_text(s);
        if (!writer->Write(out)) {
          return Status::OK;
        }

        lk.lock();
      }

      if (bus_.closed) break;
    }

    return Status::OK;
  }

private:
  Bus& bus_;
};

int main() {
  Bus bus;
  ChatService service(bus);

  std::string addr = "0.0.0.0:50051";
  ServerBuilder builder;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << addr << "\n";
  std::cout << "Type lines here; they will be broadcast to ALL clients.\n";

  std::thread input([&] {
    std::string line;
    while (std::getline(std::cin, line)) {
      bus.publish(line);
    }
    {
      std::lock_guard<std::mutex> lk(bus.m);
      bus.closed = true;
    }
    bus.cv.notify_all();
    server->Shutdown();
  });

  server->Wait();
  input.join();
}
