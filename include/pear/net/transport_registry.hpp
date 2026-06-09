#ifndef PEAR_NET_TRANSPORT_REGISTRY_HPP
#define PEAR_NET_TRANSPORT_REGISTRY_HPP

#include <memory>

#include "pear/net/pear_transport.hpp"

namespace pear::net {

void setTransport(std::shared_ptr<PearTransport> transport);

PearTransport& transport();

} // namespace pear::net

#endif // PEAR_NET_TRANSPORT_REGISTRY_HPP