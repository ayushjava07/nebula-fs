#include "nebula/snapshot/PatchApplier.hpp"

namespace nebula {
namespace snapshot {

std::optional<SnapshotManifest> PatchApplier::applyPatch(const SnapshotManifest& base,
                                                         const SnapshotDelta& delta,
                                                         const std::string& targetLabel) {
    if (delta.baseSnapshotId != base.id()) {
        return std::nullopt; // Base mismatch
    }

    std::string label = targetLabel.empty() ? ("reconstructed_snap_" + std::to_string(delta.targetSnapshotId)) : targetLabel;
    SnapshotManifest target(delta.targetSnapshotId, base.id(), label);

    // Initialize with all base nodes
    for (const auto& [path, node] : base.nodes()) {
        target.addNode(node);
    }

    // Apply delta modifications
    for (const auto& entry : delta.entries) {
        switch (entry.action) {
            case DeltaAction::Added:
            case DeltaAction::Modified:
                if (entry.newNode) {
                    target.addNode(*entry.newNode);
                }
                break;
            case DeltaAction::Deleted:
                target.removeNode(entry.path);
                break;
            case DeltaAction::Unchanged:
                break;
        }
    }

    return target;
}

} // namespace snapshot
} // namespace nebula
