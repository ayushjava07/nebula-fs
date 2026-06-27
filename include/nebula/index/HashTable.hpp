#pragma once

#include "../Config.hpp"
#include "../Types.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <span>
#include <optional>
#include <functional>
#include <system_error>

namespace nebula {
namespace index {

/// Open-addressing hash table for fast path-to-ID lookups.
///
/// Uses Robin Hood hashing with linear probing for
/// excellent cache performance and lookup speed.
class HashTable {
public:
    /// Entry in the hash table
    struct Entry {
        uint64_t hash        = 0;  ///< Full 64-bit hash of the key
        EntryID  entryId     = 0;
        uint32_t probeLength = 0;  ///< Distance from ideal slot (Robin Hood)
        bool     occupied    = false;
        bool     tombstone   = false;
    };

    explicit HashTable(size_t initialCapacity = kDefaultHashTableSize);
    ~HashTable() noexcept = default;

    /// Move-only
    HashTable(HashTable&& other) noexcept;
    HashTable& operator=(HashTable&& other) noexcept;
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    /// Insert a key-value pair
    void insert(const std::string& key, EntryID value);

    /// Find a value by key
    [[nodiscard]] std::optional<EntryID> find(const std::string& key) const;

    /// Check if a key exists
    [[nodiscard]] bool contains(const std::string& key) const;

    /// Remove an entry
    bool remove(const std::string& key);

    /// Get the number of entries
    [[nodiscard]] size_t size() const noexcept { return size_; }

    /// Get the capacity
    [[nodiscard]] size_t capacity() const noexcept { return entries_.size(); }

    /// Get load factor
    [[nodiscard]] double loadFactor() const noexcept;

    /// Clear all entries
    void clear();

    /// Reserve space
    void reserve(size_t newCapacity);

    /// Serialize to binary
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize from binary
    [[nodiscard]] std::error_code deserialize(std::span<const uint8_t> data);

    /// Iterator for traversing all entries
    class Iterator {
    public:
        std::pair<std::string, EntryID> operator*() const;
        bool operator!=(const Iterator& other) const;
        Iterator& operator++();
    private:
        friend class HashTable;
        const HashTable* table_;
        size_t index_;
        Iterator(const HashTable* table, size_t index);
    };

    [[nodiscard]] Iterator begin() const;
    [[nodiscard]] Iterator end() const;

private:
    std::vector<Entry> entries_;
    size_t size_ = 0;

    [[nodiscard]] uint64_t hashKey(const std::string& key) const noexcept;
    [[nodiscard]] size_t idealSlot(uint64_t hash) const noexcept;
    void rehash(size_t newCapacity);
    void growIfNeeded();
};

} // namespace index
} // namespace nebula
