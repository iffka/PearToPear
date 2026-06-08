#include "pear/net/transport_registry.hpp"

#include <stdexcept>

#include "pear/net/grpc_direct_transport.hpp"

namespace pear::net {
namespace {

std::shared_ptr<PearTransport>& currentTransport(){
    static std::shared_ptr<PearTransport> transport = std::make_shared<GrpcDirectTransport>();
    return transport;
}

} // namespace

void setTransport(std::shared_ptr<PearTransport> transport){
    if (!transport) {
        throw std::invalid_argument("transport must not be null");
    }

    currentTransport() = std::move(transport);
}

PearTransport& transport(){
    return *currentTransport();
}

} // namespace pear::net