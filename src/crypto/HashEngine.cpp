#include "nebula/crypto/HashEngine.hpp"

#include <openssl/evp.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <system_error>

namespace nebula {
namespace crypto {

HashEngine::HashEngine(HashAlgorithm algo) : algo_(algo) {
    initContext();
}

HashEngine::~HashEngine() noexcept {
    destroyContext();
}

HashEngine::HashEngine(HashEngine&& other) noexcept
    : algo_(other.algo_), ctx_(other.ctx_) {
    other.ctx_ = nullptr;
}

HashEngine& HashEngine::operator=(HashEngine&& other) noexcept {
    if (this != &other) {
        destroyContext();
        algo_ = other.algo_;
        ctx_ = other.ctx_;
        other.ctx_ = nullptr;
    }
    return *this;
}

void HashEngine::initContext() {
    ctx_ = EVP_MD_CTX_new();
    const EVP_MD* md = nullptr;
    switch (algo_) {
        case HashAlgorithm::SHA256: md = EVP_sha256(); break;
        case HashAlgorithm::Blake3: md = EVP_blake2b512(); break;
        default: md = EVP_sha256(); break;
    }
    if (ctx_) {
        EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(ctx_), md, nullptr);
    }
}

void HashEngine::destroyContext() noexcept {
    if (ctx_) {
        EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(ctx_));
        ctx_ = nullptr;
    }
}

void HashEngine::reset() {
    destroyContext();
    initContext();
}

void HashEngine::update(std::span<const uint8_t> data) {
    update(data.data(), data.size());
}

void HashEngine::update(const uint8_t* data, size_t length) {
    if (ctx_ && length > 0) {
        EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(ctx_), data, length);
    }
}

ChecksumValue HashEngine::finalize() {
    ChecksumValue hash{};
    if (ctx_) {
        unsigned int len = static_cast<unsigned int>(hash.size());
        EVP_DigestFinal_ex(static_cast<EVP_MD_CTX*>(ctx_), hash.data(), &len);
    }
    return hash;
}

ChecksumValue HashEngine::hash(std::span<const uint8_t> data, HashAlgorithm algo) {
    HashEngine engine(algo);
    engine.update(data);
    return engine.finalize();
}

ChecksumValue HashEngine::hash(std::span<const std::span<const uint8_t>> buffers,
                                 HashAlgorithm algo) {
    HashEngine engine(algo);
    for (const auto& buf : buffers) {
        engine.update(buf);
    }
    return engine.finalize();
}

ChecksumValue HashEngine::hashFile(const std::string& path, HashAlgorithm algo) {
    HashEngine engine(algo);

    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1) return ChecksumValue{};

    uint8_t buf[65536];
    ssize_t n;
    while ((n = ::read(fd, buf, sizeof(buf))) > 0) {
        engine.update(buf, static_cast<size_t>(n));
    }
    ::close(fd);

    return engine.finalize();
}

bool HashEngine::verify(std::span<const uint8_t> data,
                         const ChecksumValue& expected,
                         HashAlgorithm algo) {
    auto computed = hash(data, algo);
    return computed == expected;
}

size_t HashEngine::outputSize() const noexcept {
    switch (algo_) {
        case HashAlgorithm::CRC32:  return 4;
        case HashAlgorithm::SHA256: return 32;
        case HashAlgorithm::Blake3: return 64;
    }
    return 32;
}

} // namespace crypto
} // namespace nebula
