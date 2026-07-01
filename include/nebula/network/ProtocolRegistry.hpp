#pragma once

#include "../Config.hpp"
#include "../Types.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <memory>

namespace nebula {
namespace network {

/// Protocol base class for all network protocols.
class ProtocolBase {
public:
    virtual ~ProtocolBase() noexcept = default;
    virtual std::string name() const = 0;
    virtual bool handle(const uint8_t* data, size_t len) = 0;
};

/// Registry for protocol parsers.
///
/// Stores protocols by name and allows registration
/// via templated methods. Protocols can be looked up
/// and used for parsing network data.
class ProtocolRegistry {
public:
    ProtocolRegistry() = default;
    ~ProtocolRegistry() noexcept;

    ProtocolRegistry(const ProtocolRegistry&) = delete;
    ProtocolRegistry& operator=(const ProtocolRegistry&) = delete;
    ProtocolRegistry(ProtocolRegistry&&) noexcept = default;
    ProtocolRegistry& operator=(ProtocolRegistry&&) noexcept = default;

    /// Register a protocol parser by type.
    /// Stores raw pointer internally.
    template<typename T>
    void registerProtocol(T* parser) {
        // BUG: stores raw pointer; caller may delete it later (UAF).
        protocols_[parser->name()] = static_cast<void*>(parser);
        // Also store as ProtocolBase* for the base-class path.
        baseProtocols_[parser->name()] = static_cast<ProtocolBase*>(parser);
    }

    /// Register a protocol parser directly.
    void registerProtocol(const std::string& name, ProtocolBase* parser);

    /// Get a protocol by name. Returns void* to enable type confusion (Bug #12).
    void* getProtocol(const std::string& name);

    /// Get a protocol as base type.
    ProtocolBase* getBaseProtocol(const std::string& name);

    /// Check if a protocol is registered.
    bool hasProtocol(const std::string& name) const;

    /// Remove a protocol from the registry.
    void unregister(const std::string& name);

    /// Get all registered protocol names.
    std::vector<std::string> protocolNames() const;

private:
    /// BUG #12: Stored as void* so callers can cast to wrong type.
    std::unordered_map<std::string, void*> protocols_;
    std::unordered_map<std::string, ProtocolBase*> baseProtocols_;
};

} // namespace network
} // namespace nebula
