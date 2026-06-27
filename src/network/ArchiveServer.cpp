#include "nebula/network/ArchiveServer.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <system_error>
#include <sstream>

namespace nebula {
namespace network {

ArchiveServer::ArchiveServer() : ArchiveServer(Config{}) {}

ArchiveServer::ArchiveServer(Config config) : config_(config) {}

ArchiveServer::~ArchiveServer() noexcept {
    stop();
}

ArchiveServer::ArchiveServer(ArchiveServer&& other) noexcept
    : config_(other.config_)
    , reader_(std::move(other.reader_))
    , serverThread_(std::move(other.serverThread_))
    , serverFd_(other.serverFd_) {
    running_.store(other.running_.load());
    other.serverFd_ = -1;
}

ArchiveServer& ArchiveServer::operator=(ArchiveServer&& other) noexcept {
    if (this != &other) {
        stop();
        config_ = other.config_;
        reader_ = std::move(other.reader_);
        serverThread_ = std::move(other.serverThread_);
        serverFd_ = other.serverFd_;
        running_.store(other.running_.load());
        other.serverFd_ = -1;
    }
    return *this;
}

std::error_code ArchiveServer::serve(const std::string& archivePath) {
    reader_ = std::make_unique<archive::ArchiveReader>();
    auto ec = reader_->open(archivePath);
    if (ec) return ec;

    return start();
}

std::error_code ArchiveServer::start() {
    if (running_.load()) {
        return make_error_code(ErrorCode::InvalidOperation);
    }

    serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        return std::error_code(errno, std::generic_category());
    }

    int optval = 1;
    ::setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr{};
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    if (::inet_pton(AF_INET, config_.bindAddress.c_str(), &addr.sin_addr) <= 0) {
        ::close(serverFd_);
        serverFd_ = -1;
        return make_error_code(ErrorCode::InvalidOperation);
    }

    if (::bind(serverFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(serverFd_);
        serverFd_ = -1;
        return std::error_code(errno, std::generic_category());
    }

    if (::listen(serverFd_, static_cast<int>(config_.maxConnections)) < 0) {
        ::close(serverFd_);
        serverFd_ = -1;
        return std::error_code(errno, std::generic_category());
    }

    running_.store(true);
    serverThread_ = std::make_unique<std::thread>(&ArchiveServer::serverLoop, this);

    return std::error_code();
}

void ArchiveServer::stop() {
    running_.store(false);
    if (serverFd_ >= 0) {
        ::close(serverFd_);
        serverFd_ = -1;
    }
    if (serverThread_ && serverThread_->joinable()) {
        serverThread_->join();
    }
    serverThread_.reset();
    reader_.reset();
}

void ArchiveServer::serverLoop() {
    while (running_.load()) {
        struct sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientFd = ::accept(serverFd_,
                                reinterpret_cast<struct sockaddr*>(&clientAddr),
                                &clientLen);
        if (clientFd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        struct timeval tv;
        tv.tv_sec = config_.timeoutSeconds;
        tv.tv_usec = 0;
        ::setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ::setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        handleConnection(clientFd);
        ::close(clientFd);
    }
}

void ArchiveServer::handleConnection(int clientFd) {
    uint8_t cmdByte;
    auto ec = recvExact(clientFd, &cmdByte, 1);
    if (ec) return;

    Command cmd = static_cast<Command>(cmdByte);

    if (cmd == Command::Shutdown) {
        sendResponse(clientFd, ResponseCode::Success, {});
        stop();
        return;
    }

    if (cmd == Command::Ping) {
        sendResponse(clientFd, ResponseCode::Success, {});
        return;
    }

    uint8_t lenBuf[4];
    ec = recvExact(clientFd, lenBuf, 4);
    if (ec) return;

    uint32_t payloadLen = static_cast<uint32_t>(lenBuf[0]) |
                          (static_cast<uint32_t>(lenBuf[1]) << 8) |
                          (static_cast<uint32_t>(lenBuf[2]) << 16) |
                          (static_cast<uint32_t>(lenBuf[3]) << 24);

    std::vector<uint8_t> payload(payloadLen);
    if (payloadLen > 0) {
        ec = recvExact(clientFd, payload.data(), payloadLen);
        if (ec) return;
    }

    processCommand(clientFd, cmd, payload);
}

std::error_code ArchiveServer::processCommand(int clientFd, Command cmd,
                                                std::span<const uint8_t> payload) {
    if (!reader_ || !reader_->isOpen()) {
        return sendResponse(clientFd, ResponseCode::Error, {});
    }

    switch (cmd) {
        case Command::ListEntries: {
            auto entriesResult = reader_->listEntries();
            if (isError(entriesResult)) {
                return sendResponse(clientFd, ResponseCode::Error, {});
            }
            auto& entries = getValue(entriesResult);
            std::vector<uint8_t> responseData;
            for (const auto& entry : entries) {
                auto idBytes = reinterpret_cast<const uint8_t*>(&entry.id);
                responseData.insert(responseData.end(), idBytes, idBytes + sizeof(entry.id));
            }
            return sendResponse(clientFd, ResponseCode::Success, responseData);
        }
        case Command::FindEntry: {
            std::string path(reinterpret_cast<const char*>(payload.data()), payload.size());
            auto entryResult = reader_->findEntry(path);
            if (isError(entryResult)) {
                return sendResponse(clientFd, ResponseCode::NotFound, {});
            }
            auto& entry = getValue(entryResult);
            std::vector<uint8_t> responseData(sizeof(EntryID) + sizeof(Offset) + sizeof(Length));
            std::memcpy(responseData.data(), &entry.id, sizeof(EntryID));
            std::memcpy(responseData.data() + sizeof(EntryID), &entry.offset, sizeof(Offset));
            std::memcpy(responseData.data() + sizeof(EntryID) + sizeof(Offset),
                       &entry.storedSize, sizeof(Length));
            return sendResponse(clientFd, ResponseCode::Success, responseData);
        }
        case Command::ExtractEntry: {
            EntryID id;
            if (payload.size() >= sizeof(EntryID)) {
                std::memcpy(&id, payload.data(), sizeof(EntryID));
            } else {
                return sendResponse(clientFd, ResponseCode::InvalidRequest, {});
            }
            auto dataResult = reader_->extractEntry(id);
            if (isError(dataResult)) {
                return sendResponse(clientFd, ResponseCode::NotFound, {});
            }
            auto& data = getValue(dataResult);
            return sendResponse(clientFd, ResponseCode::Success, data);
        }
        case Command::GetMetadata: {
            std::string key(reinterpret_cast<const char*>(payload.data()), payload.size());
            auto value = reader_->getMetadata(key);
            if (!value) {
                return sendResponse(clientFd, ResponseCode::NotFound, {});
            }
            std::vector<uint8_t> responseData(value->begin(), value->end());
            return sendResponse(clientFd, ResponseCode::Success, responseData);
        }
        case Command::GetHeader: {
            auto& header = reader_->header();
            auto hdrData = header.serialize();
            return sendResponse(clientFd, ResponseCode::Success, hdrData);
        }
        default:
            return sendResponse(clientFd, ResponseCode::InvalidRequest, {});
    }
}

std::error_code ArchiveServer::sendResponse(int clientFd, ResponseCode code,
                                              std::span<const uint8_t> data) {
    std::vector<uint8_t> response;
    response.push_back(static_cast<uint8_t>(code));

    uint32_t len = static_cast<uint32_t>(data.size());
    response.push_back(static_cast<uint8_t>(len & 0xFF));
    response.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    response.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    response.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));

    response.insert(response.end(), data.begin(), data.end());

    return sendAll(clientFd, response);
}

std::error_code ArchiveServer::sendAll(int fd, std::span<const uint8_t> data) {
    size_t sent = 0;
    while (sent < data.size()) {
        auto n = ::write(fd, data.data() + sent, data.size() - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            return std::error_code(errno, std::generic_category());
        }
        sent += static_cast<size_t>(n);
    }
    return std::error_code();
}

std::error_code ArchiveServer::recvExact(int fd, uint8_t* buf, size_t size) {
    size_t received = 0;
    while (received < size) {
        auto n = ::read(fd, buf + received, size - received);
        if (n < 0) {
            if (errno == EINTR) continue;
            return std::error_code(errno, std::generic_category());
        }
        if (n == 0) return make_error_code(ErrorCode::IOError);
        received += static_cast<size_t>(n);
    }
    return std::error_code();
}

} // namespace network
} // namespace nebula
