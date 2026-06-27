#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <array>
#include <memory>
#include <system_error>
#include <string>

namespace nebula {
namespace crypto {

/// Encryption configuration.
struct EncryptionConfig {
    EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256GCM;
    std::array<uint8_t, kAESKeyLength> key{};  ///< 256-bit key
    bool keyDerivation = false;                  ///< Use PBKDF2 key derivation
    std::string password;                        ///< Password (if key derivation enabled)
    int pbkdf2Iterations = 100000;               ///< PBKDF2 iterations
};

/// Result of an encryption or decryption operation.
struct CryptoResult {
    std::vector<uint8_t> data;
    std::array<uint8_t, kAESIVLength> iv{};
    std::array<uint8_t, kAESTagLength> tag{};
    bool success = false;
    std::error_code ec;
};

/// Encryption engine for securing archive sections.
///
/// Supports AES-256-GCM for authenticated encryption.
/// Keys can be provided directly or derived from passwords.
class EncryptionEngine {
public:
    explicit EncryptionEngine(EncryptionConfig config = {});
    ~EncryptionEngine() noexcept;

    /// Move-only
    EncryptionEngine(EncryptionEngine&& other) noexcept;
    EncryptionEngine& operator=(EncryptionEngine&& other) noexcept;
    EncryptionEngine(const EncryptionEngine&) = delete;
    EncryptionEngine& operator=(const EncryptionEngine&) = delete;

    /// Encrypt data. Returns encrypted data with IV and auth tag.
    [[nodiscard]] CryptoResult encrypt(std::span<const uint8_t> data);

    /// Decrypt data. Requires the IV and auth tag.
    [[nodiscard]] CryptoResult decrypt(std::span<const uint8_t> encryptedData,
                                         std::span<const uint8_t> iv,
                                         std::span<const uint8_t> tag);

    /// Encrypt with explicit IV
    [[nodiscard]] CryptoResult encrypt(std::span<const uint8_t> data,
                                         std::span<const uint8_t> iv);

    /// Generate a random IV
    [[nodiscard]] static std::array<uint8_t, kAESIVLength> generateIV();

    /// Generate a random key
    [[nodiscard]] static std::array<uint8_t, kAESKeyLength> generateKey();

    /// Derive a key from a password using PBKDF2
    [[nodiscard]] static std::array<uint8_t, kAESKeyLength> deriveKey(
        std::string_view password,
        std::span<const uint8_t> salt,
        int iterations = 100000);

    /// Generate a random salt
    [[nodiscard]] static std::vector<uint8_t> generateSalt(size_t length = 16);

    /// Set the encryption key
    void setKey(std::span<const uint8_t> key);

    /// Set a password (will derive key)
    void setPassword(std::string_view password, std::span<const uint8_t> salt);

    /// Check if the engine is properly configured
    [[nodiscard]] bool isReady() const noexcept;

    /// Get the configured algorithm
    [[nodiscard]] EncryptionAlgorithm algorithm() const noexcept { return config_.algorithm; }

private:
    EncryptionConfig config_;
    void* ctx_ = nullptr;

    void initContext();
    void destroyContext() noexcept;
};

} // namespace crypto
} // namespace nebula
