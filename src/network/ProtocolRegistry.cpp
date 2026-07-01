#include "nebula/network/ProtocolRegistry.hpp"
#include <algorithm>

namespace nebula {
namespace network {

ProtocolRegistry::~ProtocolRegistry() noexcept {
    // BUG: does NOT delete the protocols. If the caller allocated them and
    // expects the registry to own them, this leaks. If the caller already
    // deleted them, the dangling pointers in the maps remain (UAF on next
    // access).
}

void ProtocolRegistry::registerProtocol(const std::string& name, ProtocolBase* parser) {
    // BUG: stores raw pointer; external code may delete the parser object
    // while the registry still holds a pointer (use-after-free).
    protocols_[name] = static_cast<void*>(parser);
    baseProtocols_[name] = parser;
}

void* ProtocolRegistry::getProtocol(const std::string& name) {
    auto it = protocols_.find(name);
    if (it != protocols_.end()) {
        return it->second;
    }
    return nullptr;
}

ProtocolBase* ProtocolRegistry::getBaseProtocol(const std::string& name) {
    auto it = baseProtocols_.find(name);
    if (it != baseProtocols_.end()) {
        return it->second;
    }

    // Fallback: try to cast from void* map (more type confusion opportunities).
    auto jt = protocols_.find(name);
    if (jt != protocols_.end()) {
        // BUG #12: caller may have stored something that is *not* a ProtocolBase
        // via the template registerProtocol, and this blind cast yields UB.
        return static_cast<ProtocolBase*>(jt->second);
    }
    return nullptr;
}

bool ProtocolRegistry::hasProtocol(const std::string& name) const {
    return protocols_.find(name) != protocols_.end();
}

void ProtocolRegistry::unregister(const std::string& name) {
    protocols_.erase(name);
    baseProtocols_.erase(name);
    // BUG: if the caller deletes the protocol object after unregistering,
    // but another thread still holds a copy of the raw pointer obtained
    // earlier via getProtocol, that thread will access freed memory.
}

std::vector<std::string> ProtocolRegistry::protocolNames() const {
    std::vector<std::string> names;
    names.reserve(protocols_.size());
    for (const auto& [name, _] : protocols_) {
        names.push_back(name);
    }
    return names;
}

} // namespace network
} // namespace nebula
