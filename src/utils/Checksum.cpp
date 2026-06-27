#include "nebula/utils/Checksum.hpp"

#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <zlib.h>

#ifdef NEBULA_HAS_OPENSSL
#include <openssl/evp.h>
#else
#include "sha256.h"
#endif

namespace nebula {
namespace utils {

ChecksumEngine::ChecksumEngine(HashAlgorithm algo) : algo_(algo) {
    initContext();
}

ChecksumEngine::ChecksumEngine(const ChecksumEngine& other) : algo_(other.algo_) {
    if (other.ctx_) {
        initContext();
        copyContext(other.ctx_);
    }
}

ChecksumEngine::ChecksumEngine(ChecksumEngine&& other) noexcept
    : algo_(other.algo_), ctx_(other.ctx_) {
    other.ctx_ = nullptr;
}

ChecksumEngine& ChecksumEngine::operator=(const ChecksumEngine& other) {
    if (this != &other) {
        destroyContext();
        algo_ = other.algo_;
        if (other.ctx_) {
            initContext();
            copyContext(other.ctx_);
        }
    }
    return *this;
}

ChecksumEngine& ChecksumEngine::operator=(ChecksumEngine&& other) noexcept {
    if (this != &other) {
        destroyContext();
        algo_ = other.algo_;
        ctx_ = other.ctx_;
        other.ctx_ = nullptr;
    }
    return *this;
}

ChecksumEngine::~ChecksumEngine() noexcept {
    destroyContext();
}

void ChecksumEngine::initContext() {
#ifdef NEBULA_HAS_OPENSSL
    if (algo_ == HashAlgorithm::SHA256 || algo_ == HashAlgorithm::Blake3) {
        ctx_ = EVP_MD_CTX_new();
        if (algo_ == HashAlgorithm::SHA256) {
            EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(ctx_), EVP_sha256(), nullptr);
        } else {
            EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(ctx_), EVP_blake2b512(), nullptr);
        }
    }
#else
    if (algo_ == HashAlgorithm::SHA256 || algo_ == HashAlgorithm::Blake3) {
        auto* sha = new SHA256_CTX;
        sha256_init(sha);
        ctx_ = sha;
    }
#endif
}

void ChecksumEngine::destroyContext() noexcept {
    if (ctx_) {
#ifdef NEBULA_HAS_OPENSSL
        EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(ctx_));
#else
        delete static_cast<SHA256_CTX*>(ctx_);
#endif
        ctx_ = nullptr;
    }
}

void ChecksumEngine::copyContext(void* otherCtx) {
    if (ctx_ && otherCtx) {
#ifdef NEBULA_HAS_OPENSSL
        EVP_MD_CTX_copy_ex(static_cast<EVP_MD_CTX*>(ctx_),
                           static_cast<EVP_MD_CTX*>(otherCtx));
#else
        std::memcpy(ctx_, otherCtx, sizeof(SHA256_CTX));
#endif
    }
}

void ChecksumEngine::reset() {
    destroyContext();
    initContext();
}

void ChecksumEngine::update(std::span<const uint8_t> data) {
    update(data.data(), data.size());
}

void ChecksumEngine::update(const uint8_t* data, size_t length) {
    if (ctx_ && length > 0) {
#ifdef NEBULA_HAS_OPENSSL
        EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(ctx_), data, length);
#else
        sha256_update(static_cast<SHA256_CTX*>(ctx_), data, length);
#endif
    }
}

void ChecksumEngine::update(std::string_view data) {
    update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

ChecksumValue ChecksumEngine::finalize() {
    ChecksumValue hash{};
    if (ctx_) {
#ifdef NEBULA_HAS_OPENSSL
        unsigned int len = static_cast<unsigned int>(hash.size());
        EVP_DigestFinal_ex(static_cast<EVP_MD_CTX*>(ctx_), hash.data(), &len);
#else
        sha256_final(static_cast<SHA256_CTX*>(ctx_), hash.data());
#endif
    }
    return hash;
}

ChecksumValue ChecksumEngine::compute(std::span<const uint8_t> data, HashAlgorithm algo) {
    ChecksumEngine engine(algo);
    engine.update(data);
    return engine.finalize();
}

ChecksumValue ChecksumEngine::compute(std::span<const std::span<const uint8_t>> buffers,
                                       HashAlgorithm algo) {
    ChecksumEngine engine(algo);
    for (const auto& buf : buffers) {
        engine.update(buf);
    }
    return engine.finalize();
}

uint32_t ChecksumEngine::crc32(std::span<const uint8_t> data) {
    return crc32(data.data(), data.size());
}

uint32_t ChecksumEngine::crc32(const uint8_t* data, size_t length) {
    return ::crc32(0, data, static_cast<uInt>(length));
}

bool ChecksumEngine::verify(std::span<const uint8_t> data,
                             const ChecksumValue& expected,
                             HashAlgorithm algo) {
    auto computed = compute(data, algo);
    return computed == expected;
}

size_t ChecksumEngine::hashSize() const noexcept {
    switch (algo_) {
        case HashAlgorithm::CRC32:  return 4;
        case HashAlgorithm::SHA256: return 32;
        case HashAlgorithm::Blake3: return 64;
    }
    return 32;
}

std::string ChecksumEngine::toHex(const ChecksumValue& checksum) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto byte : checksum) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

ChecksumValue ChecksumEngine::fromHex(std::string_view hex) {
    ChecksumValue result{};
    if (hex.size() != 64) return result;
    for (size_t i = 0; i < 32; ++i) {
        auto sub = hex.substr(i * 2, 2);
        result[i] = static_cast<uint8_t>(std::stoul(std::string(sub), nullptr, 16));
    }
    return result;
}

} // namespace utils
} // namespace nebula
