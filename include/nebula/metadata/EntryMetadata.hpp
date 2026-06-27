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

/// Per-entry metadata container.
///
/// Each entry in the archive can have its own metadata
/// beyond the basic fields (name, permissions, timestamps).
/// This includes extended attributes, ACLs, and custom tags.
class EntryMetadata {
public:
    EntryMetadata() = default;
    ~EntryMetadata() noexcept = default;

    /// Copy/move
    EntryMetadata(const EntryMetadata& other) = default;
    EntryMetadata(EntryMetadata&& other) noexcept = default;
    EntryMetadata& operator=(const EntryMetadata& other) = default;
    EntryMetadata& operator=(EntryMetadata&& other) noexcept = default;

    /// Set an extended attribute.
    void setAttribute(const std::string& name, const std::string& value);
    void setAttribute(const std::string& name, std::span<const uint8_t> value);

    /// Get an extended attribute.
    [[nodiscard]] std::optional<std::string> getAttribute(const std::string& name) const;

    /// Get all attribute names.
    [[nodiscard]] std::vector<std::string> attributeNames() const;

    /// Check if an attribute exists.
    [[nodiscard]] bool hasAttribute(const std::string& name) const noexcept;

    /// Remove an attribute.
    bool removeAttribute(const std::string& name) noexcept;

    /// Set tags.
    void setTags(const std::vector<std::string>& tags);

    /// Get tags.
    [[nodiscard]] const std::vector<std::string>& tags() const noexcept { return tags_; }

    /// Add a single tag.
    void addTag(const std::string& tag);

    /// Remove a tag.
    bool removeTag(const std::string& tag);

    /// Check if a tag exists.
    [[nodiscard]] bool hasTag(const std::string& tag) const;

    /// Set the content type (MIME).
    void setContentType(std::string_view mimeType) { contentType_ = mimeType; }

    /// Get the content type.
    [[nodiscard]] const std::string& contentType() const noexcept { return contentType_; }

    /// Set the original source path.
    void setSourcePath(std::string_view path) { sourcePath_ = path; }

    /// Get the original source path.
    [[nodiscard]] const std::string& sourcePath() const noexcept { return sourcePath_; }

    /// Set custom notes.
    void setNotes(std::string_view notes) { notes_ = notes; }

    /// Get custom notes.
    [[nodiscard]] const std::string& notes() const noexcept { return notes_; }

    /// Get the number of attributes.
    [[nodiscard]] size_t attributeCount() const noexcept { return attributes_.size(); }

    /// Clear all metadata.
    void clear() noexcept;

    /// Serialize to binary.
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize from binary.
    [[nodiscard]] std::error_code deserialize(std::span<const uint8_t> data);

private:
    std::unordered_map<std::string, std::vector<uint8_t>> attributes_;
    std::vector<std::string> tags_;
    std::string contentType_;
    std::string sourcePath_;
    std::string notes_;
};

} // namespace metadata
} // namespace nebula
