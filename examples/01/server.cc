#include <grpcpp/grpcpp.h>

#include <condition_variable>
#include <cstdint>
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

class MessageBus {
public:
  void publish(const std::string& text) {
    demo::ServerMessage msg;
    msg.set_text(text);
    msg.set_seq(++seq_);

    {
      std::lock_guard<std::mutex> g(mu_);
      msgs_.push_back(std::move(msg));
    }
    cv_.notify_all();
  }

  size_t start_index_for_new_subscriber() const {
    std::lock_guard<std::mutex> g(mu_);
    return msgs_.size();
  }

  std::mutex& mu() { return mu_; }
  std::condition_variable& cv() { return cv_; }

  size_t size_unsafe() const { return msgs_.size(); }
  demo::ServerMessage get_unsafe(size_t i) const { return msgs_[i]; }

private:
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::vector<demo::ServerMessage> msgs_;
  uint64_t seq_ = 0;
};

class BroadcastService final : public demo::Broadcast::Service {
public:
  explicit BroadcastService(MessageBus& bus) : bus_(bus) {}

  Status Subscribe(ServerContext* ctx,
                   const demo::SubscribeRequest* req,
                   ServerWriter<demo::ServerMessage>* writer) override {
    std::cerr << "Client subscribed: " << req->client_id() << "\n";

    size_t idx = bus_.start_index_for_new_subscriber();

    while (!ctx->IsCancelled()) {
      std::unique_lock<std::mutex> lk(bus_.mu());

      bus_.cv().wait(lk, [&] {
        return ctx->IsCancelled() || idx < bus_.size_unsafe();
      });

      if (ctx->IsCancelled()) break;

      while (idx < bus_.size_unsafe()) {
        demo::ServerMessage msg = bus_.get_unsafe(idx++);
        lk.unlock();

        if (!writer->Write(msg)) {
          return Status::OK;
        }

        lk.lock();
      }
    }

    return Status::OK;
  }

private:
  MessageBus& bus_;
};

static bool parse_send(const std::string& line, std::string& out_text) {
  const std::string prefix = "send:";
  if (line.rfind(prefix, 0) != 0) return false;
  out_text = line.substr(prefix.size());
  if (!out_text.empty() && out_text[0] == ' ') out_text.erase(0, 1);
  return true;
}

int main() {
  MessageBus bus;
  BroadcastService service(bus);

  ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on 0.0.0.0:50051\n";
  std::cout << "Type: send: <text>   or   quit\n";

  std::thread console([&] {
    std::string line, text;
    while (std::getline(std::cin, line)) {
      if (line == "quit") break;

      if (parse_send(line, text)) {
        bus.publish(text);
      } else {
        std::cout << "Unknown. Use: send: <text> or quit\n";
      }
    }
    server->Shutdown();
  });

  server->Wait();
  console.join();
  return 0;
}
