#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "DirectoryTree.hpp"
#include "../index/IndexManager.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <optional>
#include <system_error>
#include <functional>

namespace nebula {
namespace filesystem {

/// Resolves file paths to archive entries.
///
/// Combines the directory tree with the index to provide
/// efficient path-to-entry resolution and traversal.
class FileResolver {
public:
    FileResolver(const DirectoryTree* tree, const index::IndexManager* index);

    /// Resolve a path to an ArchiveEntry.
    [[nodiscard]] std::optional<ArchiveEntry> resolve(const std::string& path) const;

    /// Resolve by EntryID.
    [[nodiscard]] std::optional<ArchiveEntry> resolve(EntryID id) const;

    /// List directory contents.
    [[nodiscard]] std::vector<ArchiveEntry> listDirectory(const std::string& path) const;

    /// Recursively list all entries.
    [[nodiscard]] std::vector<ArchiveEntry> listAll() const;

    /// Check if a path exists.
    [[nodiscard]] bool exists(const std::string& path) const;

    /// Get the parent directory path.
    [[nodiscard]] static std::string parentPath(const std::string& path);

    /// Get the filename from a path.
    [[nodiscard]] static std::string fileName(const std::string& path);

    /// Check if a path is a directory.
    [[nodiscard]] bool isDirectory(const std::string& path) const;

    /// Glob matching (simple wildcard).
    [[nodiscard]] std::vector<ArchiveEntry> glob(const std::string& pattern) const;

private:
    const DirectoryTree* tree_;
    const index::IndexManager* index_;

    /// Simple glob pattern matching
    [[nodiscard]] static bool matchGlob(const std::string& pattern, const std::string& str);
};

} // namespace filesystem
} // namespace nebula
