#pragma once

#include "../Config.hpp"
#include "../Types.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <optional>
#include <system_error>

namespace nebula {
namespace filesystem {

/// Represents the hierarchical directory structure of an archive.
///
/// Stores entries in a tree structure supporting nested directories,
/// path resolution, and subtree traversal.
class DirectoryTree {
public:
    DirectoryTree() = default;
    ~DirectoryTree() noexcept = default;

    /// Move-only
    DirectoryTree(DirectoryTree&& other) noexcept;
    DirectoryTree& operator=(DirectoryTree&& other) noexcept;
    DirectoryTree(const DirectoryTree&) = delete;
    DirectoryTree& operator=(const DirectoryTree&) = delete;

    /// Insert a directory entry.
    [[nodiscard]] std::error_code insert(const ArchiveEntry& entry);

    /// Insert a directory node.
    [[nodiscard]] std::error_code insertNode(DirectoryNode node);

    /// Find an entry by path.
    [[nodiscard]] std::optional<DirectoryNode> find(const std::string& path) const;

    /// Find a child by name under a parent.
    [[nodiscard]] std::optional<const DirectoryNode*> findChild(
        const DirectoryNode& parent, const std::string& name) const;

    /// Get the root node.
    [[nodiscard]] const DirectoryNode& root() const noexcept { return root_; }

    /// Get the root node (mutable).
    [[nodiscard]] DirectoryNode& root() noexcept { return root_; }

    /// Get all entries in a directory.
    [[nodiscard]] std::vector<EntryID> listDirectory(const std::string& path) const;

    /// Recursively get all entry IDs.
    [[nodiscard]] std::vector<EntryID> getAllEntries() const;

    /// Resolve a path to its component parts.
    [[nodiscard]] static std::vector<std::string> splitPath(const std::string& path);

    /// Normalize a path.
    [[nodiscard]] static std::string normalizePath(const std::string& path);

    /// Check if a path exists.
    [[nodiscard]] bool exists(const std::string& path) const;

    /// Get the depth of a path.
    [[nodiscard]] static size_t pathDepth(const std::string& path);

    /// Remove a directory entry.
    bool remove(const std::string& path);

    /// Clear the tree.
    void clear() noexcept;

    /// Get the number of nodes.
    [[nodiscard]] size_t nodeCount() const noexcept { return nodeCount_; }

    /// Serialize to binary.
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize from binary.
    [[nodiscard]] std::error_code deserialize(std::span<const uint8_t> data);

private:
    DirectoryNode root_;
    size_t nodeCount_ = 0;

    void collectEntries(const DirectoryNode& node, std::vector<EntryID>& entries) const;
    void serializeNode(const DirectoryNode& node, std::vector<uint8_t>& out) const;
    std::error_code deserializeNode(DirectoryNode& node, std::span<const uint8_t>& data,
                                     size_t& offset);
    DirectoryNode* findMutable(const std::string& path);
};

} // namespace filesystem
} // namespace nebula
