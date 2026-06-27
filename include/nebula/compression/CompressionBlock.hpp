#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include "CompressionEngine.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <memory>

namespace nebula {
namespace compression {

/// A single compression block within the archive.
///
/// Each block can be individually compressed, decompressed,
/// and verified, enabling random access to archive contents.
struct CompressionBlock {
    /// Block header magic
    static constexpr uint16_t kSyncWord = 0x424E;  ///< "NB" in little-endian

    /// Block descriptor
    uint64_t offset          = 0;   ///< Offset in the blocks section
    uint64_t compressedSize  = 0;   ///< Size of compressed data
    uint64_t originalSize    = 0;   ///< Size before compression
    CompressionAlgorithm algo = CompressionAlgorithm::None;
    uint32_t checksum        = 0;   ///< CRC32 of uncompressed data
    bool     encrypted       = false;

    /// Compressed data
    std::vector<uint8_t> compressedData;

    /// Decompressed data (cached after first decompression)
    mutable std::vector<uint8_t> decompressedData;

    /// Whether the block has been decompressed
    mutable bool isDecompressed = false;

    /// Decompress this block (lazy)
    [[nodiscard]] std::span<const uint8_t> getDecompressedData() const;

    /// Verify block integrity
    [[nodiscard]] bool verifyIntegrity() const;

    /// Calculate the serialized size of this block's header
    [[nodiscard]] size_t headerSize() const noexcept;
};

/// Manages a collection of compression blocks.
class BlockCollection {
public:
    BlockCollection() = default;

    /// Add a block to the collection
    void addBlock(CompressionBlock block);

    /// Get a block by index
    [[nodiscard]] const CompressionBlock* getBlock(size_t index) const;

    /// Get the number of blocks
    [[nodiscard]] size_t blockCount() const noexcept { return blocks_.size(); }

    /// Get total compressed size
    [[nodiscard]] size_t totalCompressedSize() const noexcept;

    /// Get total original size
    [[nodiscard]] size_t totalOriginalSize() const noexcept;

    /// Find the block containing a specific offset
    [[nodiscard]] const CompressionBlock* findBlock(uint64_t originalOffset) const;

    /// Clear all blocks
    void clear() noexcept;

    /// Reserve space for blocks
    void reserve(size_t count);

private:
    std::vector<CompressionBlock> blocks_;
};

} // namespace compression
} // namespace nebula
