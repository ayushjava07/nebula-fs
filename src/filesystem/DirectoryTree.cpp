#include "nebula/filesystem/DirectoryTree.hpp"
#include "nebula/utils/VarInt.hpp"
#include <algorithm>
#include <cstring>
#include <system_error>
#include <sstream>

namespace nebula {
namespace filesystem {

DirectoryTree::DirectoryTree(DirectoryTree&& other) noexcept
    : root_(std::move(other.root_))
    , nodeCount_(other.nodeCount_) {}

DirectoryTree& DirectoryTree::operator=(DirectoryTree&& other) noexcept {
    if (this != &other) {
        root_ = std::move(other.root_);
        nodeCount_ = other.nodeCount_;
    }
    return *this;
}

std::error_code DirectoryTree::insert(const ArchiveEntry& entry) {
    if (entry.path.empty() || entry.path == "/") {
        root_.name = "/";
        root_.id = entry.id;
        root_.parentId = 0;
        root_.modifiedAt = entry.modifiedAt;
        root_.permissions = entry.permissions;
        if (entry.type == EntryType::File || entry.type == EntryType::Block) {
            root_.files.push_back(entry.id);
        }
        nodeCount_ = 1;
        return std::error_code();
    }

    auto parts = splitPath(entry.path);
    if (parts.empty()) {
        return make_error_code(ErrorCode::InvalidPath);
    }

    DirectoryNode* current = &root_;
    std::string accumulated;

    for (size_t i = 0; i < parts.size(); ++i) {
        if (!accumulated.empty()) accumulated += "/";
        accumulated += parts[i];

        if (i == parts.size() - 1) {
            DirectoryNode newNode;
            newNode.name = parts[i];
            newNode.id = entry.id;
            newNode.parentId = current->id;
            newNode.modifiedAt = entry.modifiedAt;
            newNode.permissions = entry.permissions;
            if (entry.type != EntryType::Directory) {
                current->files.push_back(entry.id);
            }
            current->children.push_back(std::move(newNode));
            ++nodeCount_;
        } else {
            auto child = findChild(*current, parts[i]);
            if (!child) {
                DirectoryNode newNode;
                newNode.name = parts[i];
                newNode.id = entry.id + 1;
                newNode.parentId = current->id;
                current->children.push_back(std::move(newNode));
                current = &current->children.back();
                ++nodeCount_;
            } else {
                current = const_cast<DirectoryNode*>(*child);
            }
        }
    }

    return std::error_code();
}

std::error_code DirectoryTree::insertNode(DirectoryNode node) {
    if (node.parentId == 0 || node.name == "/") {
        root_ = std::move(node);
        nodeCount_ = 1;
        return std::error_code();
    }
    return make_error_code(ErrorCode::NotImplemented);
}

std::optional<DirectoryNode> DirectoryTree::find(const std::string& path) const {
    if (path == "/" || path.empty()) {
        return root_;
    }

    auto parts = splitPath(path);
    const DirectoryNode* current = &root_;

    for (const auto& part : parts) {
        auto child = findChild(*current, part);
        if (!child) return std::nullopt;
        current = *child;
    }

    return *current;
}

std::optional<const DirectoryNode*> DirectoryTree::findChild(
    const DirectoryNode& parent, const std::string& name) const {
    for (const auto& child : parent.children) {
        if (child.name == name) {
            return &child;
        }
    }
    return std::nullopt;
}

std::vector<EntryID> DirectoryTree::listDirectory(const std::string& path) const {
    std::vector<EntryID> result;
    auto node = find(path);
    if (!node) return result;

    for (const auto& child : node->children) {
        result.push_back(child.id);
    }
    return result;
}

std::vector<EntryID> DirectoryTree::getAllEntries() const {
    std::vector<EntryID> result;
    collectEntries(root_, result);
    return result;
}

void DirectoryTree::collectEntries(const DirectoryNode& node,
                                    std::vector<EntryID>& entries) const {
    entries.push_back(node.id);
    for (const auto& child : node.children) {
        collectEntries(child, entries);
    }
}

std::vector<std::string> DirectoryTree::splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream stream(path);
    std::string part;
    while (std::getline(stream, part, '/')) {
        if (!part.empty() && part != "." && part != "..") {
            parts.push_back(part);
        }
    }
    return parts;
}

std::string DirectoryTree::normalizePath(const std::string& path) {
    auto parts = splitPath(path);
    if (parts.empty()) return "/";

    std::string result;
    for (const auto& part : parts) {
        result += "/" + part;
    }
    return result;
}

bool DirectoryTree::exists(const std::string& path) const {
    return find(path).has_value();
}

size_t DirectoryTree::pathDepth(const std::string& path) {
    return splitPath(path).size();
}

bool DirectoryTree::remove(const std::string& path) {
    if (path == "/" || path.empty()) {
        clear();
        return true;
    }

    auto parts = splitPath(path);
    if (parts.empty()) return false;

    DirectoryNode* current = &root_;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        bool found = false;
        for (auto& c : current->children) {
            if (c.name == parts[i]) {
                current = &c;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    auto& children = current->children;
    auto it = std::find_if(children.begin(), children.end(),
        [&](const DirectoryNode& n) { return n.name == parts.back(); });
    if (it == children.end()) return false;

    auto removedId = it->id;
    children.erase(it);
    --nodeCount_;

    auto& files = current->files;
    auto fit = std::find(files.begin(), files.end(), removedId);
    if (fit != files.end()) {
        files.erase(fit);
    }

    return true;
}

void DirectoryTree::clear() noexcept {
    root_ = DirectoryNode{};
    root_.name = "/";
    nodeCount_ = 0;
}

std::vector<uint8_t> DirectoryTree::serialize() const {
    std::vector<uint8_t> out;
    utils::VarInt::encode(nodeCount_, out);
    serializeNode(root_, out);
    return out;
}

void DirectoryTree::serializeNode(const DirectoryNode& node,
                                   std::vector<uint8_t>& out) const {
    utils::VarInt::encode(static_cast<uint64_t>(node.name.size()), out);
    out.insert(out.end(), node.name.begin(), node.name.end());
    utils::VarInt::encode(node.id, out);
    utils::VarInt::encode(node.parentId, out);
    utils::VarInt::encode(static_cast<uint64_t>(node.files.size()), out);
    for (auto fid : node.files) {
        utils::VarInt::encode(fid, out);
    }
    utils::VarInt::encode(static_cast<uint64_t>(node.children.size()), out);
    for (const auto& child : node.children) {
        serializeNode(child, out);
    }
}

std::error_code DirectoryTree::deserialize(std::span<const uint8_t> data) {
    clear();

    size_t offset = 0;
    auto countResult = utils::VarInt::decode(data);
    if (!countResult.valid) return make_error_code(ErrorCode::CorruptDirectory);
    nodeCount_ = static_cast<size_t>(countResult.value);
    offset += countResult.consumed;

    return deserializeNode(root_, data, offset);
}

std::error_code DirectoryTree::deserializeNode(DirectoryNode& node,
                                                 std::span<const uint8_t>& data,
                                                 size_t& offset) {
    auto nameLenResult = utils::VarInt::decode(data.subspan(offset));
    if (!nameLenResult.valid) return make_error_code(ErrorCode::CorruptDirectory);
    offset += nameLenResult.consumed;

    size_t nameLen = static_cast<size_t>(nameLenResult.value);
    if (offset + nameLen > data.size()) return make_error_code(ErrorCode::CorruptDirectory);
    node.name.assign(reinterpret_cast<const char*>(&data[offset]), nameLen);
    offset += nameLen;

    auto idResult = utils::VarInt::decode(data.subspan(offset));
    if (!idResult.valid) return make_error_code(ErrorCode::CorruptDirectory);
    node.id = static_cast<EntryID>(idResult.value);
    offset += idResult.consumed;

    auto parentResult = utils::VarInt::decode(data.subspan(offset));
    if (!parentResult.valid) return make_error_code(ErrorCode::CorruptDirectory);
    node.parentId = static_cast<EntryID>(parentResult.value);
    offset += parentResult.consumed;

    auto fileCountResult = utils::VarInt::decode(data.subspan(offset));
    if (!fileCountResult.valid) return make_error_code(ErrorCode::CorruptDirectory);
    size_t fileCount = static_cast<size_t>(fileCountResult.value);
    offset += fileCountResult.consumed;

    for (size_t i = 0; i < fileCount; ++i) {
        auto fidResult = utils::VarInt::decode(data.subspan(offset));
        if (!fidResult.valid) return make_error_code(ErrorCode::CorruptDirectory);
        node.files.push_back(static_cast<EntryID>(fidResult.value));
        offset += fidResult.consumed;
    }

    auto childCountResult = utils::VarInt::decode(data.subspan(offset));
    if (!childCountResult.valid) return make_error_code(ErrorCode::CorruptDirectory);
    size_t childCount = static_cast<size_t>(childCountResult.value);
    offset += childCountResult.consumed;

    for (size_t i = 0; i < childCount; ++i) {
        DirectoryNode child;
        auto ec = deserializeNode(child, data, offset);
        if (ec) return ec;
        node.children.push_back(std::move(child));
    }

    return std::error_code();
}

DirectoryNode* DirectoryTree::findMutable(const std::string& path) {
    if (path == "/" || path.empty()) return &root_;

    auto parts = splitPath(path);
    DirectoryNode* current = &root_;

    for (const auto& part : parts) {
        bool found = false;
        for (auto& child : current->children) {
            if (child.name == part) {
                current = &child;
                found = true;
                break;
            }
        }
        if (!found) return nullptr;
    }

    return current;
}

} // namespace filesystem
} // namespace nebula
