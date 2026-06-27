#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <array>
#include <string_view>
#include <functional>

namespace nebula {
namespace utils {

/// Checksum computation engine.
///
/// Supports CRC32 for fast non-cryptographic checksums
/// and SHA-256 for cryptographic integrity verification.
class ChecksumEngine {
public:
    explicit ChecksumEngine(HashAlgorithm algo = HashAlgorithm::SHA256);

    /// Copy/Move
    ChecksumEngine(const ChecksumEngine& other);
    ChecksumEngine(ChecksumEngine&& other) noexcept;
    ChecksumEngine& operator=(const ChecksumEngine& other);
    ChecksumEngine& operator=(ChecksumEngine&& other) noexcept;

    ~ChecksumEngine() noexcept;

    /// Reset the engine state
    void reset();

    /// Absorb data into the running hash
    void update(std::span<const uint8_t> data);
    void update(const uint8_t* data, size_t length);
    void update(std::string_view data);

    /// Finalize and produce the checksum
    [[nodiscard]] ChecksumValue finalize();

    /// Compute hash of a single buffer
    [[nodiscard]] static ChecksumValue compute(std::span<const uint8_t> data, HashAlgorithm algo = HashAlgorithm::SHA256);

    /// Compute hash of multiple buffers (streaming)
    [[nodiscard]] static ChecksumValue compute(std::span<const std::span<const uint8_t>> buffers,
                                                 HashAlgorithm algo = HashAlgorithm::SHA256);

    /// Compute CRC32 quickly
    [[nodiscard]] static uint32_t crc32(std::span<const uint8_t> data);
    [[nodiscard]] static uint32_t crc32(const uint8_t* data, size_t length);

    /// Verify data against a checksum
    [[nodiscard]] static bool verify(std::span<const uint8_t> data,
                                      const ChecksumValue& expected,
                                      HashAlgorithm algo = HashAlgorithm::SHA256);

    /// Get the algorithm
    [[nodiscard]] HashAlgorithm algorithm() const noexcept { return algo_; }

    /// Get the hash output size in bytes
    [[nodiscard]] size_t hashSize() const noexcept;

    /// Convert checksum to hex string
    [[nodiscard]] static std::string toHex(const ChecksumValue& checksum);

    /// Parse hex string to checksum
    [[nodiscard]] static ChecksumValue fromHex(std::string_view hex);

private:
    HashAlgorithm algo_;
    void* ctx_ = nullptr;

    void initContext();
    void destroyContext() noexcept;
    void copyContext(void* otherCtx);
};

} // namespace utils
} // namespace nebula
