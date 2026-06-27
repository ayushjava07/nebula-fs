#include "nebula/crypto/EncryptionEngine.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/kdf.h>

#include <cstring>
#include <system_error>

namespace nebula {
namespace crypto {

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
    if (config_.algorithm == EncryptionAlgorithm::AES256GCM) {
        ctx_ = EVP_CIPHER_CTX_new();
    }
}

void EncryptionEngine::destroyContext() noexcept {
    if (ctx_) {
        EVP_CIPHER_CTX_free(static_cast<EVP_CIPHER_CTX*>(ctx_));
        ctx_ = nullptr;
    }
}

CryptoResult EncryptionEngine::encrypt(std::span<const uint8_t> data) {
    auto iv = generateIV();
    return encrypt(data, iv);
}

CryptoResult EncryptionEngine::encrypt(std::span<const uint8_t> data,
                                        std::span<const uint8_t> iv) {
    CryptoResult result;

    if (config_.algorithm == EncryptionAlgorithm::None) {
        result.data.assign(data.begin(), data.end());
        result.success = true;
        return result;
    }

    if (!ctx_) {
        result.ec = make_error_code(std::errc::invalid_argument);
        return result;
    }

    const EVP_CIPHER* cipher = EVP_aes_256_gcm();
    int len = 0;

    if (1 != EVP_EncryptInit_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                cipher, nullptr, nullptr, nullptr)) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                  EVP_CTRL_GCM_SET_IVLEN,
                                  static_cast<int>(iv.size()), nullptr)) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    if (1 != EVP_EncryptInit_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                nullptr, nullptr,
                                config_.key.data(), iv.data())) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.data.resize(data.size() + kAESTagLength);

    if (1 != EVP_EncryptUpdate(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                result.data.data(), &len,
                                data.data(), static_cast<int>(data.size()))) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }
    size_t totalLen = static_cast<size_t>(len);

    if (1 != EVP_EncryptFinal_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                  result.data.data() + totalLen, &len)) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }
    totalLen += static_cast<size_t>(len);

    if (1 != EVP_CIPHER_CTX_ctrl(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                  EVP_CTRL_GCM_GET_TAG, kAESTagLength,
                                  result.tag.data())) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.data.resize(totalLen);
    std::copy(iv.begin(), iv.end(), result.iv.begin());
    result.success = true;
    return result;
}

CryptoResult EncryptionEngine::decrypt(std::span<const uint8_t> encryptedData,
                                        std::span<const uint8_t> iv,
                                        std::span<const uint8_t> tag) {
    CryptoResult result;

    if (config_.algorithm == EncryptionAlgorithm::None) {
        result.data.assign(encryptedData.begin(), encryptedData.end());
        result.success = true;
        return result;
    }

    if (!ctx_) {
        result.ec = make_error_code(std::errc::invalid_argument);
        return result;
    }

    const EVP_CIPHER* cipher = EVP_aes_256_gcm();
    int len = 0;

    if (1 != EVP_DecryptInit_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                cipher, nullptr, nullptr, nullptr)) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                  EVP_CTRL_GCM_SET_IVLEN,
                                  static_cast<int>(iv.size()), nullptr)) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    if (1 != EVP_DecryptInit_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                nullptr, nullptr,
                                config_.key.data(), iv.data())) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    result.data.resize(encryptedData.size());

    if (1 != EVP_DecryptUpdate(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                result.data.data(), &len,
                                encryptedData.data(),
                                static_cast<int>(encryptedData.size()))) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }
    size_t totalLen = static_cast<size_t>(len);

    if (1 != EVP_CIPHER_CTX_ctrl(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                  EVP_CTRL_GCM_SET_TAG,
                                  static_cast<int>(tag.size()),
                                  const_cast<uint8_t*>(tag.data()))) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }

    int ret = EVP_DecryptFinal_ex(static_cast<EVP_CIPHER_CTX*>(ctx_),
                                   result.data.data() + totalLen, &len);
    if (ret <= 0) {
        result.ec = make_error_code(std::errc::io_error);
        return result;
    }
    totalLen += static_cast<size_t>(len);

    result.data.resize(totalLen);
    result.success = true;
    return result;
}

std::array<uint8_t, kAESIVLength> EncryptionEngine::generateIV() {
    std::array<uint8_t, kAESIVLength> iv{};
    RAND_bytes(iv.data(), static_cast<int>(iv.size()));
    return iv;
}

std::array<uint8_t, kAESKeyLength> EncryptionEngine::generateKey() {
    std::array<uint8_t, kAESKeyLength> key{};
    RAND_bytes(key.data(), static_cast<int>(key.size()));
    return key;
}

std::array<uint8_t, kAESKeyLength> EncryptionEngine::deriveKey(
    std::string_view password,
    std::span<const uint8_t> salt,
    int iterations) {
    std::array<uint8_t, kAESKeyLength> key{};

    PKCS5_PBKDF2_HMAC_SHA1(
        password.data(), static_cast<int>(password.size()),
        salt.data(), static_cast<int>(salt.size()),
        iterations,
        static_cast<int>(key.size()),
        key.data());

    return key;
}

std::vector<uint8_t> EncryptionEngine::generateSalt(size_t length) {
    std::vector<uint8_t> salt(length);
    RAND_bytes(salt.data(), static_cast<int>(length));
    return salt;
}

void EncryptionEngine::setKey(std::span<const uint8_t> key) {
    std::copy(key.begin(), key.end(), config_.key.begin());
}

void EncryptionEngine::setPassword(std::string_view password, std::span<const uint8_t> salt) {
    config_.password = password;
    config_.keyDerivation = true;
    config_.key = deriveKey(password, salt, config_.pbkdf2Iterations);
}

bool EncryptionEngine::isReady() const noexcept {
    if (config_.algorithm == EncryptionAlgorithm::None) return true;
    bool hasKey = false;
    for (auto byte : config_.key) {
        if (byte != 0) { hasKey = true; break; }
    }
    return hasKey || config_.keyDerivation;
}

} // namespace crypto
} // namespace nebula
