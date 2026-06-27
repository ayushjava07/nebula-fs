#pragma once

#include "../Config.hpp"
#include "../Types.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <span>
#include <optional>
#include <system_error>

namespace nebula {
namespace metadata {

/// Key-value metadata store for archive-level metadata.
///
/// Stores arbitrary key-value pairs describing the archive:
/// creation time, tool version, source description, etc.
/// Values can be strings or binary blobs.
class MetadataStore {
public:
    MetadataStore() = default;
    ~MetadataStore() noexcept = default;

    /// Copy/move
    MetadataStore(const MetadataStore& other) = default;
    MetadataStore(MetadataStore&& other) noexcept = default;
    MetadataStore& operator=(const MetadataStore& other) = default;
    MetadataStore& operator=(MetadataStore&& other) noexcept = default;

    /// Set a string value.
    void set(const std::string& key, const std::string& value);

    /// Set a binary value.
    void set(const std::string& key, std::span<const uint8_t> value);

    /// Get a string value.
    [[nodiscard]] std::optional<std::string> getString(const std::string& key) const;

    /// Get a binary value.
    [[nodiscard]] std::optional<std::vector<uint8_t>> getBinary(const std::string& key) const;

    /// Check if a key exists.
    [[nodiscard]] bool contains(const std::string& key) const noexcept;

    /// Remove a key.
    bool remove(const std::string& key) noexcept;

    /// Get all keys.
    [[nodiscard]] std::vector<std::string> keys() const;

    /// Get the number of entries.
    [[nodiscard]] size_t size() const noexcept { return entries_.size(); }

    /// Check if empty.
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    /// Clear all entries.
    void clear() noexcept;

    /// Serialize to binary.
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize from binary.
    [[nodiscard]] std::error_code deserialize(std::span<const uint8_t> data);

    /// Validate the store.
    [[nodiscard]] bool validate() const noexcept;

    /// Merge another metadata store into this one.
    void merge(const MetadataStore& other);

    /// Get all entries.
    [[nodiscard]] const std::unordered_map<std::string, std::vector<uint8_t>>& entries() const noexcept {
        return entries_;
    }

private:
    std::unordered_map<std::string, std::vector<uint8_t>> entries_;
};

} // namespace metadata
} // namespace nebula
