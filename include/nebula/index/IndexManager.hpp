#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <optional>
#include <unordered_map>
#include <system_error>

namespace nebula {
namespace index {

/// Configuration for the index manager.
struct IndexConfig {
    size_t btreeOrder = kBTreeDefaultOrder;
    size_t cacheSize  = kBTreeCacheSize;
    bool enableCache  = true;
};

/// Manages the index table of the archive.
///
/// Provides fast lookup of entries by ID or path using
/// a combination of B-tree indexing and hash table caching.
class IndexManager {
public:
    explicit IndexManager(IndexConfig config = {});
    ~IndexManager() noexcept;

    /// Move-only
    IndexManager(IndexManager&& other) noexcept;
    IndexManager& operator=(IndexManager&& other) noexcept;
    IndexManager(const IndexManager&) = delete;
    IndexManager& operator=(const IndexManager&) = delete;

    /// Insert an entry into the index.
    void insert(const ArchiveEntry& entry);

    /// Insert multiple entries.
    void insert(std::span<const ArchiveEntry> entries);

    /// Find an entry by ID.
    [[nodiscard]] std::optional<IndexEntry> findById(EntryID id) const;

    /// Find an entry by path.
    [[nodiscard]] std::optional<IndexEntry> findByPath(const std::string& path) const;

    /// Find entries matching a path prefix.
    [[nodiscard]] std::vector<IndexEntry> findByPrefix(const std::string& prefix) const;

    /// Find entries by type.
    [[nodiscard]] std::vector<IndexEntry> findByType(EntryType type) const;

    /// Check if an entry exists.
    [[nodiscard]] bool contains(EntryID id) const;
    [[nodiscard]] bool containsPath(const std::string& path) const;

    /// Remove an entry.
    bool remove(EntryID id);

    /// Get the number of indexed entries.
    [[nodiscard]] size_t size() const noexcept { return entries_.size(); }

    /// Get all entries.
    [[nodiscard]] const std::vector<IndexEntry>& entries() const noexcept { return entries_; }

    /// Get all entries (mutable).
    [[nodiscard]] std::vector<IndexEntry>& entries() noexcept { return entries_; }

    /// Clear the index.
    void clear() noexcept;

    /// Serialize index to binary.
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize index from binary.
    [[nodiscard]] std::error_code deserialize(std::span<const uint8_t> data);

    /// Validate index integrity.
    [[nodiscard]] bool validate() const;

    /// Build path-to-ID mapping.
    const std::unordered_map<std::string, EntryID>& pathMap() const noexcept { return pathMap_; }

    /// Get config.
    [[nodiscard]] const IndexConfig& config() const noexcept { return config_; }

private:
    IndexConfig config_;
    std::vector<IndexEntry> entries_;
    std::unordered_map<EntryID, size_t> idIndex_;         ///< EntryID -> index in entries_
    std::unordered_map<std::string, EntryID> pathMap_;     ///< Path -> EntryID
    std::unordered_map<EntryType, std::vector<size_t>> typeIndex_;  ///< EntryType -> indices
    bool sorted_ = false;

    /// Ensure entries are sorted by ID for binary search
    void ensureSorted();

    void clearAndNotify();
    void processEntry(IndexEntry* entry);

    std::vector<IndexEntry*> rawEntryCache_;
};

} // namespace index
} // namespace nebula
