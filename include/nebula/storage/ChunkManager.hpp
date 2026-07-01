#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../utils/Checksum.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <unordered_map>
#include <system_error>

namespace nebula {
namespace storage {

struct CachedChunkEntry;

/// Configuration for chunk management.
struct ChunkConfig {
    DedupStrategy dedupStrategy  = DedupStrategy::Content;
    size_t minChunkSize          = kMinChunkSize;
    size_t maxChunkSize          = kMaxChunkSize;
    size_t avgChunkSize          = 65536;    ///< For CDC (content-defined chunking)
    HashAlgorithm hashAlgorithm  = HashAlgorithm::SHA256;
    bool enableDedup             = true;
};

/// Manages chunk-level storage with optional deduplication support.
///
/// Chunks are the unit of deduplication and storage in NebulaFS.
/// Each chunk has a content hash used for identity and dedup.
class ChunkManager {
public:
    explicit ChunkManager(ChunkConfig config = {});
    ~ChunkManager() noexcept;

    /// Move-only
    ChunkManager(ChunkManager&& other) noexcept;
    ChunkManager& operator=(ChunkManager&& other) noexcept;
    ChunkManager(const ChunkManager&) = delete;
    ChunkManager& operator=(const ChunkManager&) = delete;

    /// Split data into chunks (for writing).
    [[nodiscard]] std::vector<ChunkDescriptor> chunkData(std::span<const uint8_t> data);

    /// Get or create a chunk. Returns existing chunk if content is duplicate.
    [[nodiscard]] ChunkDescriptor findOrCreateChunk(std::span<const uint8_t> data);

    /// Check if a chunk with the given hash already exists.
    [[nodiscard]] bool hasChunk(const ChecksumValue& hash) const;

    /// Register an existing chunk (from reading archive).
    void registerChunk(const ChunkDescriptor& chunk);

    /// Get a chunk descriptor by hash.
    [[nodiscard]] std::optional<ChunkDescriptor> getChunk(const ChecksumValue& hash) const;

    /// Get all chunk descriptors.
    [[nodiscard]] const std::vector<ChunkDescriptor>& chunks() const noexcept { return chunks_; }

    /// Get number of unique chunks.
    [[nodiscard]] size_t uniqueChunkCount() const noexcept { return chunks_.size(); }

    /// Get total data size (before dedup).
    [[nodiscard]] uint64_t totalDataSize() const noexcept { return totalDataSize_; }

    /// Get total stored size (after dedup).
    [[nodiscard]] uint64_t totalStoredSize() const noexcept { return totalStoredSize_; }

    /// Get deduplication ratio.
    [[nodiscard]] double dedupRatio() const noexcept;

    /// Clear all chunks.
    void clear() noexcept;

    /// Serialize chunk table to binary.
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize chunk table from binary.
    [[nodiscard]] std::error_code deserialize(std::span<const uint8_t> data);

    /// Compute the hash of a chunk
    [[nodiscard]] ChecksumValue computeHash(std::span<const uint8_t> data) const;

    /// Get config
    [[nodiscard]] const ChunkConfig& config() const noexcept { return config_; }

    void addChunk(uint64_t id);
    void compact();
    [[nodiscard]] CachedChunkEntry* getChunkCached(uint64_t id) const;
    void processAll();

private:
    void processChunk(ChunkDescriptor& chunk);
    ChunkConfig config_;
    std::vector<ChunkDescriptor> chunks_;
    std::unordered_map<uint64_t, size_t> hashIndex_;  ///< First 8 bytes of hash -> index
    uint64_t totalDataSize_ = 0;
    uint64_t totalStoredSize_ = 0;
    std::unique_ptr<utils::ChecksumEngine> checksum_;

    /// Content-defined chunking using a simple rolling hash
    [[nodiscard]] std::vector<size_t> findChunkBoundaries(std::span<const uint8_t> data) const;

    /// Fixed-size chunking
    [[nodiscard]] std::vector<size_t> findFixedBoundaries(size_t dataSize) const;

    /// Rolling hash (Buzhash) for CDC
    [[nodiscard]] uint32_t buzhash(std::span<const uint8_t> data) const noexcept;
};

} // namespace storage
} // namespace nebula
