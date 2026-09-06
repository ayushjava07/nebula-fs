#include "nebula/storage/backend/MemoryStorageBackend.hpp"
#include "nebula/utils/Checksum.hpp"

#include <mutex>

namespace nebula {
namespace storage {
namespace backend {

MemoryStorageBackend::MemoryStorageBackend(size_t maxCapacityBytes)
    : maxCapacityBytes_(maxCapacityBytes) {
    stats_.capacityBytes = maxCapacityBytes_;
}

void MemoryStorageBackend::setCapacity(size_t maxCapacityBytes) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    maxCapacityBytes_ = maxCapacityBytes;
    stats_.capacityBytes = maxCapacityBytes_;
}

StorageError MemoryStorageBackend::writeBlock(uint64_t blockId, std::span<const uint8_t> data) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    size_t existingSize = 0;
    auto it = blocks_.find(blockId);
    if (it != blocks_.end()) {
        existingSize = it->second.data.size();
    }

    if (currentBytes_ + data.size() - existingSize > maxCapacityBytes_) {
        return StorageError::CapacityExceeded;
    }

    InternalBlock block;
    block.data.assign(data.begin(), data.end());
    block.checksum = utils::ChecksumEngine::crc32(data);

    currentBytes_ = currentBytes_ + data.size() - existingSize;
    blocks_[blockId] = std::move(block);

    stats_.writeOps++;
    stats_.totalBlocks = blocks_.size();
    stats_.totalBytes = currentBytes_;

    return StorageError::Success;
}

StorageResult<std::vector<uint8_t>> MemoryStorageBackend::readBlock(uint64_t blockId) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = blocks_.find(blockId);
    if (it == blocks_.end()) {
        return StorageError::NotFound;
    }

    uint32_t currentChecksum = utils::ChecksumEngine::crc32(it->second.data);
    if (currentChecksum != it->second.checksum) {
        return StorageError::CorruptBlock;
    }

    stats_.readOps++;
    return it->second.data;
}

StorageResult<std::vector<uint8_t>> MemoryStorageBackend::readSpan(uint64_t blockId, size_t offset, size_t length) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = blocks_.find(blockId);
    if (it == blocks_.end()) {
        return StorageError::NotFound;
    }

    const auto& data = it->second.data;
    if (offset + length > data.size()) {
        return StorageError::InvalidOffset;
    }

    stats_.readOps++;
    return std::vector<uint8_t>(data.begin() + static_cast<ptrdiff_t>(offset),
                                data.begin() + static_cast<ptrdiff_t>(offset + length));
}

StorageError MemoryStorageBackend::deleteBlock(uint64_t blockId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = blocks_.find(blockId);
    if (it == blocks_.end()) {
        return StorageError::NotFound;
    }

    currentBytes_ -= it->second.data.size();
    blocks_.erase(it);

    stats_.deleteOps++;
    stats_.totalBlocks = blocks_.size();
    stats_.totalBytes = currentBytes_;

    return StorageError::Success;
}

bool MemoryStorageBackend::hasBlock(uint64_t blockId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return blocks_.find(blockId) != blocks_.end();
}

StorageStats MemoryStorageBackend::stats() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    StorageStats s = stats_;
    s.totalBlocks = blocks_.size();
    s.totalBytes = currentBytes_;
    s.capacityBytes = maxCapacityBytes_;
    return s;
}

StorageError MemoryStorageBackend::sync() {
    return StorageError::Success;
}

void MemoryStorageBackend::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    blocks_.clear();
    currentBytes_ = 0;
    stats_.totalBlocks = 0;
    stats_.totalBytes = 0;
}

} // namespace backend
} // namespace storage
} // namespace nebula
