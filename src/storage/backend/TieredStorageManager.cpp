#include "nebula/storage/backend/TieredStorageManager.hpp"
#include <algorithm>

namespace nebula {
namespace storage {
namespace backend {

TieredStorageManager::TieredStorageManager(std::unique_ptr<IStorageBackend> hotTier,
                                           std::unique_ptr<IStorageBackend> coldTier,
                                           size_t maxHotBlocks)
    : hotTier_(std::move(hotTier)), coldTier_(std::move(coldTier)), maxHotBlocks_(maxHotBlocks) {}

void TieredStorageManager::touchHot(uint64_t blockId) {
    auto it = std::find(hotLruQueue_.begin(), hotLruQueue_.end(), blockId);
    if (it != hotLruQueue_.end()) {
        hotLruQueue_.erase(it);
    }
    hotLruQueue_.push_front(blockId);
}

void TieredStorageManager::spillHotToCold(size_t blocksToSpill) {
    while (blocksToSpill > 0 && !hotLruQueue_.empty()) {
        uint64_t victimId = hotLruQueue_.back();
        if (demoteBlock(victimId) == StorageError::Success) {
            blocksToSpill--;
        } else {
            hotLruQueue_.pop_back();
            blockTiers_.erase(victimId);
        }
    }
}

StorageError TieredStorageManager::demoteBlock(uint64_t blockId) {
    auto readRes = hotTier_->readBlock(blockId);
    if (!std::holds_alternative<std::vector<uint8_t>>(readRes)) {
        return std::get<StorageError>(readRes);
    }

    const auto& data = std::get<std::vector<uint8_t>>(readRes);
    StorageError writeErr = coldTier_->writeBlock(blockId, data);
    if (writeErr != StorageError::Success) {
        return writeErr;
    }

    hotTier_->deleteBlock(blockId);
    blockTiers_[blockId] = TierLevel::ColdDisk;

    auto it = std::find(hotLruQueue_.begin(), hotLruQueue_.end(), blockId);
    if (it != hotLruQueue_.end()) {
        hotLruQueue_.erase(it);
    }

    stats_.migrationsToCold++;
    return StorageError::Success;
}

StorageError TieredStorageManager::promoteBlock(uint64_t blockId) {
    auto readRes = coldTier_->readBlock(blockId);
    if (!std::holds_alternative<std::vector<uint8_t>>(readRes)) {
        return std::get<StorageError>(readRes);
    }

    const auto& data = std::get<std::vector<uint8_t>>(readRes);

    if (hotLruQueue_.size() >= maxHotBlocks_) {
        spillHotToCold(1);
    }

    StorageError writeErr = hotTier_->writeBlock(blockId, data);
    if (writeErr != StorageError::Success) {
        return writeErr;
    }

    coldTier_->deleteBlock(blockId);
    blockTiers_[blockId] = TierLevel::HotMemory;
    touchHot(blockId);

    stats_.migrationsToHot++;
    return StorageError::Success;
}

StorageError TieredStorageManager::writeBlock(uint64_t blockId, std::span<const uint8_t> data) {
    std::lock_guard<std::mutex> lock(mutex_);

    // If block exists in cold tier, remove it
    if (coldTier_->hasBlock(blockId)) {
        coldTier_->deleteBlock(blockId);
    }

    if (hotLruQueue_.size() >= maxHotBlocks_) {
        spillHotToCold(1);
    }

    StorageError err = hotTier_->writeBlock(blockId, data);
    if (err == StorageError::Success) {
        blockTiers_[blockId] = TierLevel::HotMemory;
        touchHot(blockId);
    }
    return err;
}

StorageResult<std::vector<uint8_t>> TieredStorageManager::readBlock(uint64_t blockId, bool promoteToHot) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = blockTiers_.find(blockId);
    if (it == blockTiers_.end()) {
        return StorageError::NotFound;
    }

    if (it->second == TierLevel::HotMemory) {
        auto res = hotTier_->readBlock(blockId);
        if (std::holds_alternative<std::vector<uint8_t>>(res)) {
            touchHot(blockId);
        }
        return res;
    }

    // Block is in cold disk tier
    auto res = coldTier_->readBlock(blockId);
    if (std::holds_alternative<std::vector<uint8_t>>(res) && promoteToHot) {
        promoteBlock(blockId);
    }
    return res;
}

StorageResult<std::vector<uint8_t>> TieredStorageManager::readSpan(uint64_t blockId, size_t offset, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = blockTiers_.find(blockId);
    if (it == blockTiers_.end()) {
        return StorageError::NotFound;
    }

    if (it->second == TierLevel::HotMemory) {
        return hotTier_->readSpan(blockId, offset, length);
    }
    return coldTier_->readSpan(blockId, offset, length);
}

StorageError TieredStorageManager::deleteBlock(uint64_t blockId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = blockTiers_.find(blockId);
    if (it == blockTiers_.end()) {
        return StorageError::NotFound;
    }

    StorageError err = StorageError::NotFound;
    if (it->second == TierLevel::HotMemory) {
        err = hotTier_->deleteBlock(blockId);
        auto qIt = std::find(hotLruQueue_.begin(), hotLruQueue_.end(), blockId);
        if (qIt != hotLruQueue_.end()) hotLruQueue_.erase(qIt);
    } else {
        err = coldTier_->deleteBlock(blockId);
    }

    blockTiers_.erase(it);
    return err;
}

bool TieredStorageManager::hasBlock(uint64_t blockId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return blockTiers_.find(blockId) != blockTiers_.end();
}

TierLevel TieredStorageManager::getBlockTier(uint64_t blockId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = blockTiers_.find(blockId);
    if (it != blockTiers_.end()) {
        return it->second;
    }
    return TierLevel::ColdDisk;
}

TieredStorageStats TieredStorageManager::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    TieredStorageStats s = stats_;

    auto hotStats = hotTier_->stats();
    auto coldStats = coldTier_->stats();

    s.hotBlocks = hotStats.totalBlocks;
    s.hotBytes = hotStats.totalBytes;
    s.coldBlocks = coldStats.totalBlocks;
    s.coldBytes = coldStats.totalBytes;

    return s;
}

StorageError TieredStorageManager::sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    StorageError e1 = hotTier_->sync();
    StorageError e2 = coldTier_->sync();
    return (e1 != StorageError::Success) ? e1 : e2;
}

void TieredStorageManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    hotTier_->clear();
    coldTier_->clear();
    blockTiers_.clear();
    hotLruQueue_.clear();
    stats_ = TieredStorageStats{};
}

} // namespace backend
} // namespace storage
} // namespace nebula
