#pragma once

#include "nebula/snapshot/SnapshotManifest.hpp"
#include "nebula/snapshot/DeltaEngine.hpp"
#include "nebula/snapshot/PatchApplier.hpp"

#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

namespace nebula {
namespace snapshot {

class SnapshotManager {
public:
    SnapshotManager() = default;
    ~SnapshotManager() = default;

    SnapshotID createSnapshot(const std::string& label, const std::vector<SnapshotNode>& currentNodes);
    bool registerSnapshot(SnapshotManifest manifest);

    [[nodiscard]] std::optional<SnapshotManifest> getSnapshot(SnapshotID id) const;
    [[nodiscard]] std::optional<SnapshotManifest> currentSnapshot() const;
    [[nodiscard]] SnapshotID currentSnapshotId() const noexcept;

    bool checkoutSnapshot(SnapshotID id);
    bool deleteSnapshot(SnapshotID id);

    [[nodiscard]] std::vector<SnapshotManifest> listSnapshots() const;
    [[nodiscard]] std::optional<SnapshotDelta> createDelta(SnapshotID fromId, SnapshotID toId) const;

    [[nodiscard]] size_t snapshotCount() const noexcept;
    void clear() noexcept;

private:
    mutable std::shared_mutex mutex_;
    SnapshotID currentSnapshotId_{0};
    SnapshotID nextSnapshotId_{1};
    std::unordered_map<SnapshotID, SnapshotManifest> manifests_;
};

} // namespace snapshot
} // namespace nebula
