#include "nebula/snapshot/DeltaEngine.hpp"
#include "nebula/utils/VarInt.hpp"

#include <unordered_set>
#include <algorithm>

namespace nebula {
namespace snapshot {

size_t SnapshotDelta::addedCount() const noexcept {
    return static_cast<size_t>(std::count_if(entries.begin(), entries.end(),
                                             [](const DeltaEntry& e) { return e.action == DeltaAction::Added; }));
}

size_t SnapshotDelta::modifiedCount() const noexcept {
    return static_cast<size_t>(std::count_if(entries.begin(), entries.end(),
                                             [](const DeltaEntry& e) { return e.action == DeltaAction::Modified; }));
}

size_t SnapshotDelta::deletedCount() const noexcept {
    return static_cast<size_t>(std::count_if(entries.begin(), entries.end(),
                                             [](const DeltaEntry& e) { return e.action == DeltaAction::Deleted; }));
}

uint64_t SnapshotDelta::addedBytes() const noexcept {
    uint64_t total = 0;
    for (const auto& e : entries) {
        if (e.action == DeltaAction::Added && e.newNode) {
            total += e.newNode->size;
        } else if (e.action == DeltaAction::Modified && e.newNode && e.oldNode) {
            if (e.newNode->size > e.oldNode->size) {
                total += (e.newNode->size - e.oldNode->size);
            }
        }
    }
    return total;
}

uint64_t SnapshotDelta::deletedBytes() const noexcept {
    uint64_t total = 0;
    for (const auto& e : entries) {
        if (e.action == DeltaAction::Deleted && e.oldNode) {
            total += e.oldNode->size;
        } else if (e.action == DeltaAction::Modified && e.newNode && e.oldNode) {
            if (e.oldNode->size > e.newNode->size) {
                total += (e.oldNode->size - e.newNode->size);
            }
        }
    }
    return total;
}

SnapshotDelta DeltaEngine::computeDelta(const SnapshotManifest& base,
                                       const SnapshotManifest& target) {
    SnapshotDelta delta;
    delta.baseSnapshotId = base.id();
    delta.targetSnapshotId = target.id();

    // Check target nodes for Added or Modified
    for (const auto& [path, targetNode] : target.nodes()) {
        auto baseNodeOpt = base.findNode(path);
        if (!baseNodeOpt) {
            DeltaEntry entry;
            entry.path = path;
            entry.action = DeltaAction::Added;
            entry.newNode = targetNode;
            entry.newBlockIds = targetNode.blockIds;
            delta.entries.push_back(std::move(entry));
        } else {
            const auto& baseNode = *baseNodeOpt;
            if (baseNode.checksum != targetNode.checksum || baseNode.size != targetNode.size) {
                DeltaEntry entry;
                entry.path = path;
                entry.action = DeltaAction::Modified;
                entry.oldNode = baseNode;
                entry.newNode = targetNode;

                std::unordered_set<uint64_t> baseBlocks(baseNode.blockIds.begin(), baseNode.blockIds.end());
                for (uint64_t bId : targetNode.blockIds) {
                    if (baseBlocks.find(bId) == baseBlocks.end()) {
                        entry.newBlockIds.push_back(bId);
                    }
                }
                delta.entries.push_back(std::move(entry));
            }
        }
    }

    // Check base nodes for Deleted
    for (const auto& [path, baseNode] : base.nodes()) {
        if (!target.hasNode(path)) {
            DeltaEntry entry;
            entry.path = path;
            entry.action = DeltaAction::Deleted;
            entry.oldNode = baseNode;
            delta.entries.push_back(std::move(entry));
        }
    }

    return delta;
}

std::vector<uint64_t> DeltaEngine::computeReusedBlocks(const SnapshotDelta& delta) {
    std::vector<uint64_t> reused;
    for (const auto& entry : delta.entries) {
        if (entry.action == DeltaAction::Modified && entry.oldNode && entry.newNode) {
            std::unordered_set<uint64_t> oldBlocks(entry.oldNode->blockIds.begin(), entry.oldNode->blockIds.end());
            for (uint64_t bId : entry.newNode->blockIds) {
                if (oldBlocks.find(bId) != oldBlocks.end()) {
                    reused.push_back(bId);
                }
            }
        }
    }
    return reused;
}

std::vector<uint8_t> SnapshotDelta::serialize() const {
    std::vector<uint8_t> buf;
    utils::VarInt::encode(baseSnapshotId, buf);
    utils::VarInt::encode(targetSnapshotId, buf);
    utils::VarInt::encode(entries.size(), buf);

    for (const auto& e : entries) {
        utils::VarInt::encode(e.path.size(), buf);
        buf.insert(buf.end(), e.path.begin(), e.path.end());

        buf.push_back(static_cast<uint8_t>(e.action));

        // Serialize newNode if present
        if (e.newNode) {
            buf.push_back(1);
            utils::VarInt::encode(e.newNode->entryId, buf);
            utils::VarInt::encode(e.newNode->size, buf);
            utils::VarInt::encode(e.newNode->checksum, buf);
            utils::VarInt::encode(e.newNode->modifiedTime, buf);
            utils::VarInt::encode(e.newNode->blockIds.size(), buf);
            for (uint64_t bId : e.newNode->blockIds) {
                utils::VarInt::encode(bId, buf);
            }
        } else {
            buf.push_back(0);
        }
    }

    return buf;
}

std::optional<SnapshotDelta> SnapshotDelta::deserialize(std::span<const uint8_t> bytes) {
    if (bytes.empty()) return std::nullopt;

    size_t offset = 0;
    auto baseRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!baseRes.valid) return std::nullopt;
    offset += baseRes.consumed;

    if (offset >= bytes.size()) return std::nullopt;
    auto targetRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!targetRes.valid) return std::nullopt;
    offset += targetRes.consumed;

    if (offset >= bytes.size()) return std::nullopt;
    auto numEntriesRes = utils::VarInt::decode(bytes.subspan(offset));
    if (!numEntriesRes.valid) return std::nullopt;
    offset += numEntriesRes.consumed;

    SnapshotDelta delta;
    delta.baseSnapshotId = baseRes.value;
    delta.targetSnapshotId = targetRes.value;
    delta.entries.reserve(static_cast<size_t>(numEntriesRes.value));

    for (uint64_t i = 0; i < numEntriesRes.value; ++i) {
        if (offset >= bytes.size()) return std::nullopt;
        auto pathLenRes = utils::VarInt::decode(bytes.subspan(offset));
        if (!pathLenRes.valid) return std::nullopt;
        offset += pathLenRes.consumed;

        size_t pathLen = static_cast<size_t>(pathLenRes.value);
        if (offset + pathLen > bytes.size()) return std::nullopt;
        std::string path(reinterpret_cast<const char*>(bytes.data() + offset), pathLen);
        offset += pathLen;

        if (offset >= bytes.size()) return std::nullopt;
        uint8_t actionByte = bytes[offset++];

        DeltaEntry entry;
        entry.path = std::move(path);
        entry.action = static_cast<DeltaAction>(actionByte);

        if (offset >= bytes.size()) return std::nullopt;
        uint8_t hasNewNode = bytes[offset++];
        if (hasNewNode) {
            SnapshotNode node;
            node.path = entry.path;

            auto eidRes = utils::VarInt::decode(bytes.subspan(offset));
            if (!eidRes.valid) return std::nullopt;
            offset += eidRes.consumed;
            node.entryId = eidRes.value;

            auto szRes = utils::VarInt::decode(bytes.subspan(offset));
            if (!szRes.valid) return std::nullopt;
            offset += szRes.consumed;
            node.size = szRes.value;

            auto ckRes = utils::VarInt::decode(bytes.subspan(offset));
            if (!ckRes.valid) return std::nullopt;
            offset += ckRes.consumed;
            node.checksum = static_cast<uint32_t>(ckRes.value);

            auto mtRes = utils::VarInt::decode(bytes.subspan(offset));
            if (!mtRes.valid) return std::nullopt;
            offset += mtRes.consumed;
            node.modifiedTime = mtRes.value;

            auto bCountRes = utils::VarInt::decode(bytes.subspan(offset));
            if (!bCountRes.valid) return std::nullopt;
            offset += bCountRes.consumed;

            for (uint64_t b = 0; b < bCountRes.value; ++b) {
                auto bIdRes = utils::VarInt::decode(bytes.subspan(offset));
                if (!bIdRes.valid) return std::nullopt;
                offset += bIdRes.consumed;
                node.blockIds.push_back(bIdRes.value);
            }
            entry.newNode = std::move(node);
        }

        delta.entries.push_back(std::move(entry));
    }

    return delta;
}

} // namespace snapshot
} // namespace nebula
