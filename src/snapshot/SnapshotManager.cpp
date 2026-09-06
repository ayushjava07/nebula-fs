#include "nebula/snapshot/SnapshotManager.hpp"
#include <mutex>
#include <algorithm>

namespace nebula {
namespace snapshot {

SnapshotID SnapshotManager::createSnapshot(const std::string& label, const std::vector<SnapshotNode>& currentNodes) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    SnapshotID newId = nextSnapshotId_++;
    SnapshotManifest manifest(newId, currentSnapshotId_, label);
    for (const auto& node : currentNodes) {
        manifest.addNode(node);
    }

    manifests_[newId] = manifest;
    currentSnapshotId_ = newId;
    return newId;
}

bool SnapshotManager::registerSnapshot(SnapshotManifest manifest) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    SnapshotID id = manifest.id();
    if (manifests_.find(id) != manifests_.end()) {
        return false;
    }
    if (id >= nextSnapshotId_) {
        nextSnapshotId_ = id + 1;
    }
    currentSnapshotId_ = id;
    manifests_[id] = std::move(manifest);
    return true;
}

std::optional<SnapshotManifest> SnapshotManager::getSnapshot(SnapshotID id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = manifests_.find(id);
    if (it != manifests_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<SnapshotManifest> SnapshotManager::currentSnapshot() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = manifests_.find(currentSnapshotId_);
    if (it != manifests_.end()) {
        return it->second;
    }
    return std::nullopt;
}

SnapshotID SnapshotManager::currentSnapshotId() const noexcept {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return currentSnapshotId_;
}

bool SnapshotManager::checkoutSnapshot(SnapshotID id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (manifests_.find(id) == manifests_.end()) {
        return false;
    }
    currentSnapshotId_ = id;
    return true;
}

bool SnapshotManager::deleteSnapshot(SnapshotID id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = manifests_.find(id);
    if (it == manifests_.end()) {
        return false;
    }
    manifests_.erase(it);
    if (currentSnapshotId_ == id) {
        currentSnapshotId_ = manifests_.empty() ? 0 : manifests_.begin()->first;
    }
    return true;
}

std::vector<SnapshotManifest> SnapshotManager::listSnapshots() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<SnapshotManifest> list;
    list.reserve(manifests_.size());
    for (const auto& [_, m] : manifests_) {
        list.push_back(m);
    }
    std::sort(list.begin(), list.end(), [](const SnapshotManifest& a, const SnapshotManifest& b) {
        return a.id() < b.id();
    });
    return list;
}

std::optional<SnapshotDelta> SnapshotManager::createDelta(SnapshotID fromId, SnapshotID toId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto fromIt = manifests_.find(fromId);
    auto toIt = manifests_.find(toId);
    if (fromIt == manifests_.end() || toIt == manifests_.end()) {
        return std::nullopt;
    }
    return DeltaEngine::computeDelta(fromIt->second, toIt->second);
}

size_t SnapshotManager::snapshotCount() const noexcept {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return manifests_.size();
}

void SnapshotManager::clear() noexcept {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    manifests_.clear();
    currentSnapshotId_ = 0;
    nextSnapshotId_ = 1;
}

} // namespace snapshot
} // namespace nebula
