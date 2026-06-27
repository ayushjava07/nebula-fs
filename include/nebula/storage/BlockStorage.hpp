#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include "../compression/CompressionBlock.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <system_error>

namespace nebula {
namespace storage {

/// Manages the storage and retrieval of compressed data blocks.
///
/// Blocks are the physical storage units in the archive.
/// Each block has a header with metadata and compressed/encrypted payload.
class BlockStorage {
public:
    BlockStorage() = default;
    ~BlockStorage() noexcept = default;

    /// Move-only
    BlockStorage(BlockStorage&& other) noexcept;
    BlockStorage& operator=(BlockStorage&& other) noexcept;
    BlockStorage(const BlockStorage&) = delete;
    BlockStorage& operator=(const BlockStorage&) = delete;

    /// Write a block and return its descriptor.
    [[nodiscard]] std::error_code writeBlock(std::span<const uint8_t> data,
                                              CompressionAlgorithm algo,
                                              bool encrypt,
                                              ChunkDescriptor& descriptor);

    /// Read and reconstruct a block from its descriptor.
    [[nodiscard]] std::error_code readBlock(const ChunkDescriptor& descriptor,
                                              std::vector<uint8_t>& output);

    /// Read raw block data from the blocks section.
    [[nodiscard]] std::vector<uint8_t> readRawBlock(std::span<const uint8_t> blocksSection,
                                                      const ChunkDescriptor& descriptor);

    /// Serialize a block to bytes.
    [[nodiscard]] std::vector<uint8_t> serializeBlock(const compression::CompressionBlock& block);

    /// Deserialize a block from bytes.
    [[nodiscard]] Result<compression::CompressionBlock> deserializeBlock(
        std::span<const uint8_t> data, size_t offset);

    /// Verify block integrity (checksum + alignment).
    [[nodiscard]] bool verifyBlock(std::span<const uint8_t> blockData);

    /// Get alignment requirement
    [[nodiscard]] static size_t blockAlignment() noexcept { return 4; }

private:
    std::vector<uint8_t> blockData_;
};

} // namespace storage
} // namespace nebula
