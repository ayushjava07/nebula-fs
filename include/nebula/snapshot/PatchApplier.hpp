#pragma once

#include "nebula/snapshot/DeltaEngine.hpp"
#include <optional>

namespace nebula {
namespace snapshot {

class PatchApplier {
public:
    PatchApplier() = default;

    static std::optional<SnapshotManifest> applyPatch(const SnapshotManifest& base,
                                                      const SnapshotDelta& delta,
                                                      const std::string& targetLabel = "");
};

} // namespace snapshot
} // namespace nebula
