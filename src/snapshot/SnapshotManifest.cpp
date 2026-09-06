#include "nebula/snapshot/SnapshotManifest.hpp"
#include "nebula/utils/VarInt.hpp"

#include <chrono>

namespace nebula {
namespace snapshot {

SnapshotManifest::SnapshotManifest(SnapshotID id, SnapshotID parentId, std::string label, uint64_t timestamp)
    : id_(id), parentId_(parentId), label_(std::move(label)), timestamp_(timestamp) {
    if (timestamp_ == 0) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        timestamp_ = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
    }
}

void SnapshotManifest::addNode(SnapshotNode node) {
    nodes_[node.path] = std::move(node);
}

bool SnapshotManifest::removeNode(const std::string& path) {
    return nodes_.erase(path) > 0;
}

std::optional<SnapshotNode> SnapshotManifest::findNode(const std::string& path) const {
    auto it = nodes_.find(path);
    if (it != nodes_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool SnapshotManifest::hasNode(const std::string& path) const {
    return nodes_.find(path) != nodes_.end();
}

uint64_t SnapshotManifest::totalSize() const noexcept {
    uint64_t sum = 0;
    for (const auto& [_, node] : nodes_) {
        sum += node.size;
    }
    return sum;
}

void SnapshotManifest::clear() noexcept {
    nodes_.clear();
}

std::vector<uint8_t> SnapshotManifest::serialize() const {
    std::vector<uint8_t> buf;

    utils::VarInt::encode(id_, buf);
    utils::VarInt::encode(parentId_, buf);
    utils::VarInt::encode(timestamp_, buf);

    utils::VarInt::encode(label_.size(), buf);
    buf.insert(buf.end(), label_.begin(), label_.end());

    utils::VarInt::encode(nodes_.size(), buf);
    for (const auto& [path, node] : nodes_) {
        utils::VarInt::encode(path.size(), buf);
        buf.insert(buf.end(), path.begin(), path.end());

        utils::VarInt::encode(node.entryId, buf);
        utils::VarInt::encode(node.size, buf);
        utils::VarInt::encode(node.checksum, buf);
        utils::VarInt::encode(node.modifiedTime, buf);

        utils::VarInt::encode(node.blockIds.size(), buf);
        for (uint64_t bId : node.blockIds) {
            utils::VarInt::encode(bId, buf);
        }
    }

    return buf;
}

std::optional<SnapshotManifest> SnapshotManifest::deserialize(std::span<const uint8_t> bytes) {
    if (bytes.empty()) return std::nullopt;

    size_t offset = 0;

    auto idRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!idRes.valid) return std::nullopt;
    offset += idRes.consumed;

    if (offset >= bytes.size()) return std::nullopt;
    auto parentIdRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!parentIdRes.valid) return std::nullopt;
    offset += parentIdRes.consumed;

    if (offset >= bytes.size()) return std::nullopt;
    auto tsRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!tsRes.valid) return std::nullopt;
    offset += tsRes.consumed;

    if (offset >= bytes.size()) return std::nullopt;
    auto labelLenRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!labelLenRes.valid) return std::nullopt;
    offset += labelLenRes.consumed;

    size_t labelLen = static_cast<size_t>(labelLenRes.value);
    if (offset + labelLen > bytes.size()) return std::nullopt;
    std::string label(reinterpret_cast<const char*>(bytes.data() + offset), labelLen);
    offset += labelLen;

    SnapshotManifest manifest(idRes.value, parentIdRes.value, std::move(label), tsRes.value);

    if (offset >= bytes.size()) return manifest;
    auto numNodesRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!numNodesRes.valid) return std::nullopt;
    offset += numNodesRes.consumed;

    for (uint64_t i = 0; i < numNodesRes.value; ++i) {
        if (offset >= bytes.size()) return std::nullopt;
        auto pathLenRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!pathLenRes.valid) return std::nullopt;
        offset += pathLenRes.consumed;

        size_t pathLen = static_cast<size_t>(pathLenRes.value);
        if (offset + pathLen > bytes.size()) return std::nullopt;
        std::string path(reinterpret_cast<const char*>(bytes.data() + offset), pathLen);
        offset += pathLen;

        if (offset >= bytes.size()) return std::nullopt;
        auto eidRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!eidRes.valid) return std::nullopt;
        offset += eidRes.consumed;

        if (offset >= bytes.size()) return std::nullopt;
        auto szRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!szRes.valid) return std::nullopt;
        offset += szRes.consumed;

        if (offset >= bytes.size()) return std::nullopt;
        auto ckRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!ckRes.valid) return std::nullopt;
        offset += ckRes.consumed;

        if (offset >= bytes.size()) return std::nullopt;
        auto mtimeRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!mtimeRes.valid) return std::nullopt;
        offset += mtimeRes.consumed;

        if (offset >= bytes.size()) return std::nullopt;
        auto numBlocksRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!numBlocksRes.valid) return std::nullopt;
        offset += numBlocksRes.consumed;

        std::vector<uint64_t> blockIds;
        blockIds.reserve(static_cast<size_t>(numBlocksRes.value));
        for (uint64_t b = 0; b < numBlocksRes.value; ++b) {
            if (offset >= bytes.size()) return std::nullopt;
            auto bIdRes = utils::VarInt::decode(bytes.subspan(offset));
            if (!bIdRes.valid) return std::nullopt;
            offset += bIdRes.consumed;
            blockIds.push_back(bIdRes.value);
        }

        SnapshotNode node;
        node.entryId = eidRes.value;
        node.path = path;
        node.size = szRes.value;
        node.checksum = static_cast<uint32_t>(ckRes.value);
        node.modifiedTime = mtimeRes.value;
        node.blockIds = std::move(blockIds);

        manifest.addNode(std::move(node));
    }

    return manifest;
}

} // namespace snapshot
} // namespace nebula
