#pragma once

#include "nebula/snapshot/SnapshotManifest.hpp"
#include <vector>
#include <optional>
#include <span>

namespace nebula {
namespace snapshot {

enum class DeltaAction : uint8_t {
    Unchanged = 0,
    Added = 1,
    Modified = 2,
    Deleted = 3
};

struct DeltaEntry {
    std::string path;
    DeltaAction action{DeltaAction::Unchanged};
    std::optional<SnapshotNode> oldNode;
    std::optional<SnapshotNode> newNode;
    std::vector<uint64_t> newBlockIds;

    bool operator==(const DeltaEntry& other) const {
        return path == other.path && action == other.action &&
               oldNode == other.oldNode && newNode == other.newNode &&
               newBlockIds == other.newBlockIds;
    }
};

struct SnapshotDelta {
    SnapshotID baseSnapshotId{0};
    SnapshotID targetSnapshotId{0};
    std::vector<DeltaEntry> entries;

    [[nodiscard]] size_t addedCount() const noexcept;
    [[nodiscard]] size_t modifiedCount() const noexcept;
    [[nodiscard]] size_t deletedCount() const noexcept;
    [[nodiscard]] uint64_t addedBytes() const noexcept;
    [[nodiscard]] uint64_t deletedBytes() const noexcept;

    [[nodiscard]] std::vector<uint8_t> serialize() const;
    static std::optional<SnapshotDelta> deserialize(std::span<const uint8_t> bytes);
};

class DeltaEngine {
public:
    DeltaEngine() = default;

    [[nodiscard]] static SnapshotDelta computeDelta(const SnapshotManifest& base,
                                                   const SnapshotManifest& target);

    [[nodiscard]] static std::vector<uint64_t> computeReusedBlocks(const SnapshotDelta& delta);
};

} // namespace snapshot
} // namespace nebula
