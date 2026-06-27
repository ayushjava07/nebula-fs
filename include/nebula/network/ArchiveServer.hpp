#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../archive/ArchiveReader.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>
#include <system_error>
#include <memory>
#include <thread>
#include <atomic>

namespace nebula {
namespace network {

/// Protocol command identifiers.
enum class Command : uint8_t {
    ListEntries       = 0x01,
    FindEntry         = 0x02,
    ExtractEntry      = 0x03,
    GetMetadata       = 0x04,
    GetHeader         = 0x05,
    Ping              = 0x06,
    Shutdown          = 0xFF
};

/// Response status codes.
enum class ResponseCode : uint8_t {
    Success         = 0x00,
    NotFound        = 0x01,
    Error           = 0x02,
    AuthRequired    = 0x03,
    InvalidRequest  = 0x04
};

/// Simple embedded archive server for network access.
///
/// Provides read-only access to a NebulaFS archive over TCP.
/// Supports listing, finding, and extracting entries remotely.
class ArchiveServer {
public:
    /// Configuration for the server.
    struct Config {
        uint16_t port            = 9871;
        std::string bindAddress  = "127.0.0.1";
        size_t maxConnections    = 10;
        size_t bufferSize        = kIOBufferSize;
        int timeoutSeconds       = 30;
        bool requireAuth         = false;
        std::string authToken;
    };

    ArchiveServer();
    explicit ArchiveServer(Config config);
    ~ArchiveServer() noexcept;

    /// Move-only
    ArchiveServer(ArchiveServer&& other) noexcept;
    ArchiveServer& operator=(ArchiveServer&& other) noexcept;
    ArchiveServer(const ArchiveServer&) = delete;
    ArchiveServer& operator=(const ArchiveServer&) = delete;

    /// Open an archive and start serving.
    [[nodiscard]] std::error_code serve(const std::string& archivePath);

    /// Start the server (non-blocking).
    [[nodiscard]] std::error_code start();

    /// Stop the server.
    void stop();

    /// Check if server is running.
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

    /// Get the port number.
    [[nodiscard]] uint16_t port() const noexcept { return config_.port; }

private:
    Config config_;
    std::unique_ptr<archive::ArchiveReader> reader_;
    std::unique_ptr<std::thread> serverThread_;
    std::atomic<bool> running_{false};
    int serverFd_ = -1;

    void serverLoop();
    void handleConnection(int clientFd);
    [[nodiscard]] std::error_code processCommand(int clientFd, Command cmd,
                                                  std::span<const uint8_t> payload);
    [[nodiscard]] std::error_code sendResponse(int clientFd, ResponseCode code,
                                                 std::span<const uint8_t> data);
    [[nodiscard]] std::error_code sendAll(int fd, std::span<const uint8_t> data);
    [[nodiscard]] std::error_code recvExact(int fd, uint8_t* buf, size_t size);
};

} // namespace network
} // namespace nebula
