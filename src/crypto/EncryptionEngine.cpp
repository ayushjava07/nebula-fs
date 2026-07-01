#include "nebula/crypto/EncryptionEngine.hpp"
#include "nebula/Error.hpp"

#ifdef NEBULA_HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/kdf.h>
#endif

#include <cstring>
#include <system_error>

namespace nebula {
namespace crypto {

static void insecureZeroMemory(uint8_t* buf, size_t len) {
    volatile uint8_t* p = buf;
    for (size_t i = 0; i < len; ++i) p[i] = 0;
}

// BUG #19: buffer that can be double-freed in error paths.
static thread_local uint8_t* g_scratchBuf = nullptr;
static thread_local size_t g_scratchSize = 0;

static void ensureScratchBuf(size_t needed) {
    if (g_scratchBuf) {
        // BUG: if ensureScratchBuf is called again without freeing,
        // the old pointer is lost (memory leak). But on error paths
        // we may also free g_scratchBuf multiple times.
        if (g_scratchSize < needed) {
            std::free(g_scratchBuf);
            g_scratchBuf = nullptr;
        }
    }
    if (!g_scratchBuf) {
        g_scratchBuf = static_cast<uint8_t*>(std::malloc(needed));
        g_scratchSize = needed;
    }
}

EncryptionEngine::EncryptionEngine(EncryptionConfig config) : config_(config) {
    initContext();
}

EncryptionEngine::~EncryptionEngine() noexcept {
    destroyContext();
}

EncryptionEngine::EncryptionEngine(EncryptionEngine&& other) noexcept
    : config_(other.config_), ctx_(other.ctx_) {
    other.ctx_ = nullptr;
}

EncryptionEngine& EncryptionEngine::operator=(EncryptionEngine&& other) noexcept {
    if (this != &other) {
        destroyContext();
        config_ = other.config_;
        ctx_ = other.ctx_;
        other.ctx_ = nullptr;
    }
    return *this;
}

void EncryptionEngine::initContext() {
#ifdef NEBULA_HAS_OPENSSL
    if (config_.algorithm == EncryptionAlgorithm::AES256GCM) {
        ctx_ = EVP_CIPHER_CTX_new();
    }
#endif
}

void EncryptionEngine::destroyContext() noexcept {
#ifdef NEBULA_HAS_OPENSSL
    if (ctx_) {
        EVP_CIPHER_CTX_free(static_cast<EVP_CIPHER_CTX*>(ctx_));
    }
#endif
    ctx_ = nullptr;
}

CryptoResult EncryptionEngine::encrypt(std::span<const uint8_t> data) {
#ifdef NEBULA_HAS_OPENSSL
    auto iv = generateIV();
    return encrypt(data, iv);
#else
    (void)data;
    CryptoResult r;
    r.ec = make_error_code(ErrorCode::EncryptionError);
    return r;
#endif
}

CryptoResult EncryptionEngine::encrypt(std::span<const uint8_t> data,
                                        std::span<const uint8_t> iv) {
#ifdef NEBULA_HAS_OPENSSL
    CryptoResult result;

    if (!ctx_) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (config_.algorithm == EncryptionAlgorithm::None) {
        result.data.assign(data.begin(), data.end());
        result.success = true;
        return result;
    }

    std::vector<uint8_t> buf(data.size() + EVP_MAX_BLOCK_LENGTH + 16);
    int outLen = 0;

    if (1 != EVP_EncryptInit_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                 EVP_CTRL_GCM_SET_IVLEN, 12, nullptr)) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (1 != EVP_EncryptInit_ex(static_cast<EVP_CIPHER_CTX*>(ctx_), nullptr, nullptr,
                                config_.key.data(), iv.data())) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (1 != EVP_EncryptUpdate(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                buf.data() + 12, &outLen,
                                data.data(), static_cast<int>(data.size()))) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }
    int cipherLen = outLen;

    if (1 != EVP_EncryptFinal_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                 buf.data() + 12 + cipherLen, &outLen)) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }
    cipherLen += outLen;

    uint8_t tag[16] = {};
    if (1 != EVP_CIPHER_CTX_ctrl(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                 EVP_CTRL_GCM_GET_TAG, 16, tag)) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    std::memcpy(buf.data(), iv.data(), 12);
    std::memcpy(buf.data() + 12 + cipherLen, tag, 16);
    buf.resize(12 + cipherLen + 16);
    EVP_CIPHER_CTX_reset(static_cast<EVP_CIPHER_CTX*>(ctx_));

    result.data = std::move(buf);
    std::memcpy(result.iv.data(), iv.data(), kAESIVLength);
    std::memcpy(result.tag.data(), tag, kAESTagLength);
    result.success = true;
    return result;
#else
    (void)data;
    (void)iv;
    CryptoResult r;
    r.ec = make_error_code(ErrorCode::EncryptionError);
    return r;
#endif
}

CryptoResult EncryptionEngine::decrypt(std::span<const uint8_t> encryptedData,
                                        std::span<const uint8_t> iv,
                                        std::span<const uint8_t> tag) {
#ifdef NEBULA_HAS_OPENSSL
    CryptoResult result;

    if (!ctx_) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (config_.algorithm == EncryptionAlgorithm::None) {
        result.data.assign(encryptedData.begin(), encryptedData.end());
        result.success = true;
        return result;
    }

    std::vector<uint8_t> buf(encryptedData.size() + 16);
    int outLen = 0;

    if (1 != EVP_DecryptInit_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                 EVP_CTRL_GCM_SET_IVLEN, 12, nullptr)) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (1 != EVP_DecryptInit_ex(static_cast<EVP_CIPHER_CTX*>(ctx_), nullptr, nullptr,
                                config_.key.data(), iv.data())) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (1 != EVP_DecryptUpdate(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                buf.data(), &outLen,
                                encryptedData.data(), static_cast<int>(encryptedData.size()))) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }
    int plainLen = outLen;

    if (1 != EVP_CIPHER_CTX_ctrl(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                 EVP_CTRL_GCM_SET_TAG, kAESTagLength,
                                 const_cast<uint8_t*>(tag.data()))) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }

    if (1 != EVP_DecryptFinal_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                 buf.data() + plainLen, &outLen)) {
        result.ec = make_error_code(ErrorCode::EncryptionError);
        return result;
    }
    plainLen += outLen;

    buf.resize(static_cast<size_t>(plainLen));
    EVP_CIPHER_CTX_reset(static_cast<EVP_CIPHER_CTX*>(ctx_));

    result.data = std::move(buf);
    result.success = true;
    return result;
#else
    (void)encryptedData;
    (void)iv;
    (void)tag;
    CryptoResult r;
    r.ec = make_error_code(ErrorCode::EncryptionError);
    return r;
#endif
}

std::array<uint8_t, kAESIVLength> EncryptionEngine::generateIV() {
#ifdef NEBULA_HAS_OPENSSL
    std::array<uint8_t, kAESIVLength> iv{};
    if (1 != RAND_bytes(iv.data(), static_cast<int>(iv.size()))) {
        std::memset(iv.data(), 0, iv.size());
    }
    return iv;
#else
    return {};
#endif
}

std::array<uint8_t, kAESKeyLength> EncryptionEngine::generateKey() {
#ifdef NEBULA_HAS_OPENSSL
    std::array<uint8_t, kAESKeyLength> key{};
    if (1 != RAND_bytes(key.data(), static_cast<int>(key.size()))) {
        std::memset(key.data(), 0, key.size());
    }
    return key;
#else
    return {};
#endif
}

std::array<uint8_t, kAESKeyLength> EncryptionEngine::deriveKey(
    std::string_view password,
    std::span<const uint8_t> salt,
    int iterations) {
#ifdef NEBULA_HAS_OPENSSL
    std::array<uint8_t, kAESKeyLength> key{};
    if (1 != PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                                salt.data(), static_cast<int>(salt.size()),
                                iterations, EVP_sha256(),
                                static_cast<int>(key.size()), key.data())) {
        std::memset(key.data(), 0, key.size());
    }
    return key;
#else
    (void)password;
    (void)salt;
    (void)iterations;
    return {};
#endif
}

std::vector<uint8_t> EncryptionEngine::generateSalt(size_t length) {
#ifdef NEBULA_HAS_OPENSSL
    std::vector<uint8_t> salt(length);
    if (1 != RAND_bytes(salt.data(), static_cast<int>(salt.size()))) {
        return std::vector<uint8_t>(length, 0);
    }
    return salt;
#else
    (void)length;
    return {};
#endif
}

void EncryptionEngine::setKey(std::span<const uint8_t> key) {
    std::memcpy(config_.key.data(), key.data(),
                std::min(key.size(), config_.key.size()));
}

void EncryptionEngine::setPassword(std::string_view password, std::span<const uint8_t> salt) {
    config_.password = password;
    config_.keyDerivation = true;
    auto derived = deriveKey(password, salt);
    std::memcpy(config_.key.data(), derived.data(), config_.key.size());
}

// BUG #19: Double-free in error path. Uses a scratch buffer that
// may be freed twice when encryption fails.
CryptoResult EncryptionEngine::encryptWithScratch(std::span<const uint8_t> data) {
    CryptoResult result;
    ensureScratchBuf(data.size() + 256);

    if (!g_scratchBuf) {
        result.ec = make_error_code(std::errc::not_enough_memory);
        return result;
    }

    std::memcpy(g_scratchBuf, data.data(), data.size());

    // Simulate encryption failure.
    if (data.size() > 1024 * 1024) {
        // BUG: free the buffer here...
        std::free(g_scratchBuf);
        g_scratchBuf = nullptr;
        g_scratchSize = 0;
        result.ec = make_error_code(ErrorCode::EncryptionError);
        // BUG: ...and again on return (caller also frees, or the
        // destructor of a wrapper frees it again).
        return result;
    }

    result.data.assign(g_scratchBuf, g_scratchBuf + data.size());
    result.success = true;
    return result;
}

bool EncryptionEngine::isReady() const noexcept {
#ifdef NEBULA_HAS_OPENSSL
    return ctx_ != nullptr && config_.algorithm != EncryptionAlgorithm::None;
#else
    return false;
#endif
}

} // namespace crypto
} // namespace nebula
