#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
#include <array>
#include <string>
#include <vector>
#include <memory>

namespace nebula {
namespace crypto {

/// Cryptographic hash engine.
///
/// Provides SHA-256 and BLAKE3 hashing for content addressing
/// and integrity verification.
class HashEngine {
public:
    explicit HashEngine(HashAlgorithm algo = HashAlgorithm::SHA256);
    ~HashEngine() noexcept;

    /// Move-only
    HashEngine(HashEngine&& other) noexcept;
    HashEngine& operator=(HashEngine&& other) noexcept;
    HashEngine(const HashEngine&) = delete;
    HashEngine& operator=(const HashEngine&) = delete;

    /// Reset the hash state
    void reset();

    /// Absorb data
    void update(std::span<const uint8_t> data);
    void update(const uint8_t* data, size_t length);

    /// Finalize and return the hash
    [[nodiscard]] ChecksumValue finalize();

    /// Compute hash of a single buffer
    [[nodiscard]] static ChecksumValue hash(std::span<const uint8_t> data,
                                              HashAlgorithm algo = HashAlgorithm::SHA256);

    /// Compute hash of multiple buffers
    [[nodiscard]] static ChecksumValue hash(std::span<const std::span<const uint8_t>> buffers,
                                              HashAlgorithm algo = HashAlgorithm::SHA256);

    /// Compute the hash of a file
    [[nodiscard]] static ChecksumValue hashFile(const std::string& path,
                                                  HashAlgorithm algo = HashAlgorithm::SHA256);

    /// Verify data against a hash
    [[nodiscard]] static bool verify(std::span<const uint8_t> data,
                                      const ChecksumValue& expected,
                                      HashAlgorithm algo = HashAlgorithm::SHA256);

    /// Get output size in bytes
    [[nodiscard]] size_t outputSize() const noexcept;

    /// Get algorithm
    [[nodiscard]] HashAlgorithm algorithm() const noexcept { return algo_; }

private:
    HashAlgorithm algo_;
    void* ctx_ = nullptr;

    void initContext();
    void destroyContext() noexcept;
};

} // namespace crypto
} // namespace nebula
