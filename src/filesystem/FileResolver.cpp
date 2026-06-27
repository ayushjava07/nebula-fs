#include "nebula/filesystem/FileResolver.hpp"
#include <algorithm>
#include <sstream>

namespace nebula {
namespace filesystem {

FileResolver::FileResolver(const DirectoryTree* tree, const index::IndexManager* index)
    : tree_(tree), index_(index) {}

std::optional<ArchiveEntry> FileResolver::resolve(const std::string& path) const {
    if (!tree_ || !index_) return std::nullopt;

    auto idxEntry = index_->findByPath(path);
    if (!idxEntry) return std::nullopt;

    auto fullEntry = index_->findById(idxEntry->entryId);
    if (!fullEntry) return std::nullopt;

    ArchiveEntry entry;
    entry.id = fullEntry->entryId;
    entry.offset = fullEntry->offset;
    entry.storedSize = fullEntry->size;
    entry.checksum = fullEntry->checksum;
    entry.path = path;
    return entry;
}

std::optional<ArchiveEntry> FileResolver::resolve(EntryID id) const {
    if (!index_) return std::nullopt;

    auto idxEntry = index_->findById(id);
    if (!idxEntry) return std::nullopt;

    ArchiveEntry entry;
    entry.id = idxEntry->entryId;
    entry.offset = idxEntry->offset;
    entry.storedSize = idxEntry->size;
    entry.checksum = idxEntry->checksum;
    return entry;
}

std::vector<ArchiveEntry> FileResolver::listDirectory(const std::string& path) const {
    std::vector<ArchiveEntry> result;
    if (!tree_ || !index_) return result;

    auto entryIds = tree_->listDirectory(path);
    for (auto id : entryIds) {
        auto resolved = resolve(id);
        if (resolved) {
            result.push_back(*resolved);
        }
    }

    return result;
}

std::vector<ArchiveEntry> FileResolver::listAll() const {
    std::vector<ArchiveEntry> result;
    if (!tree_ || !index_) return result;

    for (const auto& idxEntry : index_->entries()) {
        ArchiveEntry entry;
        entry.id = idxEntry.entryId;
        entry.offset = idxEntry.offset;
        entry.storedSize = idxEntry.size;
        entry.checksum = idxEntry.checksum;
        result.push_back(entry);
    }

    return result;
}

bool FileResolver::exists(const std::string& path) const {
    if (!tree_) return false;
    return tree_->exists(path);
}

std::string FileResolver::parentPath(const std::string& path) {
    if (path == "/" || path.empty()) return "/";

    auto pos = path.find_last_of('/');
    if (pos == 0) return "/";
    if (pos == std::string::npos) return "/";

    return path.substr(0, pos);
}

std::string FileResolver::fileName(const std::string& path) {
    if (path == "/" || path.empty()) return "";

    auto pos = path.find_last_of('/');
    if (pos == std::string::npos) return path;

    return path.substr(pos + 1);
}

bool FileResolver::isDirectory(const std::string& path) const {
    if (!tree_) return false;
    auto node = tree_->find(path);
    return node.has_value();
}

std::vector<ArchiveEntry> FileResolver::glob(const std::string& pattern) const {
    std::vector<ArchiveEntry> result;
    if (!index_) return result;

    for (const auto& [path, id] : index_->pathMap()) {
        if (matchGlob(pattern, path)) {
            auto resolved = resolve(id);
            if (resolved) {
                result.push_back(*resolved);
            }
        }
    }

    return result;
}

bool FileResolver::matchGlob(const std::string& pattern, const std::string& str) {
    size_t pi = 0, si = 0;
    size_t starPos = std::string::npos, matchPos = 0;

    while (si < str.size()) {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == str[si])) {
            ++pi; ++si;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            starPos = pi;
            matchPos = si;
            ++pi;
        } else if (starPos != std::string::npos) {
            pi = starPos + 1;
            ++matchPos;
            si = matchPos;
        } else {
            return false;
        }
    }

    while (pi < pattern.size() && pattern[pi] == '*') {
        ++pi;
    }

    return pi == pattern.size();
}

} // namespace filesystem
} // namespace nebula
