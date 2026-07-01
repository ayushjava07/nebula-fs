#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <memory>
#include <system_error>

namespace nebula {
namespace compression {

/// Configuration for the compression engine.
struct CompressionConfig {
    CompressionAlgorithm algorithm = CompressionAlgorithm::Zstd;
    int compressionLevel          = 3;   ///< 0-22 for Zstd, 1-9 for Zlib, 0=default for LZ4
    size_t blockSize              = kDefaultBlockSize;
    bool enableChecksum           = true;
    size_t workMemory             = 0;   ///< 0 = use default
};

/// Result of a compression or decompression operation.
struct CompressionResult {
    std::vector<uint8_t> data;
    size_t originalSize   = 0;
    size_t compressedSize = 0;
    bool success          = false;
    std::error_code ec;
};

/// Compression engine supporting multiple algorithms.
///
/// Provides streaming compress/decompress with configurable
/// block sizes and compression levels.
class CompressionEngine {
public:
    explicit CompressionEngine(CompressionConfig config = {});
    ~CompressionEngine() noexcept;

    /// Move-only
    CompressionEngine(CompressionEngine&& other) noexcept;
    CompressionEngine& operator=(CompressionEngine&& other) noexcept;
    CompressionEngine(const CompressionEngine&) = delete;
    CompressionEngine& operator=(const CompressionEngine&) = delete;

    /// Compress a data buffer.
    /// Returns compressed data with algorithm metadata.
    [[nodiscard]] CompressionResult compress(std::span<const uint8_t> data);

    /// Decompress previously compressed data.
    [[nodiscard]] CompressionResult decompress(std::span<const uint8_t> compressed,
                                                  size_t originalSize);

    /// Compress data in blocks (streaming).
    /// Each block is independently compressed.
    [[nodiscard]] std::vector<CompressionResult> compressBlocks(
        std::span<const uint8_t> data, size_t blockSize = kDefaultBlockSize);

    /// Decompress blocks back into a single buffer.
    [[nodiscard]] CompressionResult decompressBlocks(
        std::span<const CompressionResult> blocks, size_t totalOriginalSize);

    /// Get the configured algorithm
    [[nodiscard]] CompressionAlgorithm algorithm() const noexcept { return config_.algorithm; }

    /// BUG #20: Type confusion -- decompress with detected algorithm,
    /// but may misidentify and apply the wrong decompression.
    [[nodiscard]] CompressionResult decompressWithDetection(
        std::span<const uint8_t> compressed, size_t originalSize,
        CompressionAlgorithm detectedAlgo);

    /// Estimate compressed size (may be inexact)
    [[nodiscard]] size_t estimateCompressedSize(size_t originalSize) const noexcept;

    /// Check if compression is supported for the configured algorithm
    [[nodiscard]] bool isSupported() const noexcept;

    /// Get maximum expansion size during compression
    [[nodiscard]] static size_t maxCompressedSize(size_t inputSize) noexcept;

    /// Get the config
    [[nodiscard]] const CompressionConfig& config() const noexcept { return config_; }

    /// Reconfigure the engine
    void setConfig(const CompressionConfig& config);

    /// Get a human-readable name for the configured algorithm
    [[nodiscard]] std::string_view algorithmName() const noexcept;

private:
    CompressionConfig config_;
    void* ctx_ = nullptr;

    CompressionResult compressLZ4(std::span<const uint8_t> data);
    CompressionResult decompressLZ4(std::span<const uint8_t> data, size_t originalSize);
    CompressionResult compressZlib(std::span<const uint8_t> data);
    CompressionResult decompressZlib(std::span<const uint8_t> data, size_t originalSize);
    CompressionResult compressZstd(std::span<const uint8_t> data);
    CompressionResult decompressZstd(std::span<const uint8_t> data, size_t originalSize);

    void initContext();
    void destroyContext() noexcept;
};

} // namespace compression
} // namespace nebula
