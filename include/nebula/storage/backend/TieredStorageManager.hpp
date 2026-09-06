#pragma once

#include "nebula/storage/backend/StorageBackend.hpp"
#include <memory>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>

namespace nebula {
namespace storage {
namespace backend {

enum class TierLevel : uint8_t {
    HotMemory = 0,
    ColdDisk = 1
};

struct TieredStorageStats {
    uint64_t hotBlocks{0};
    uint64_t coldBlocks{0};
    uint64_t hotBytes{0};
    uint64_t coldBytes{0};
    uint64_t migrationsToCold{0};
    uint64_t migrationsToHot{0};
};

class TieredStorageManager {
public:
    TieredStorageManager(std::unique_ptr<IStorageBackend> hotTier,
                         std::unique_ptr<IStorageBackend> coldTier,
                         size_t maxHotBlocks = 500);

    StorageError writeBlock(uint64_t blockId, std::span<const uint8_t> data);
    StorageResult<std::vector<uint8_t>> readBlock(uint64_t blockId, bool promoteToHot = true);
    StorageResult<std::vector<uint8_t>> readSpan(uint64_t blockId, size_t offset, size_t length);
    StorageError deleteBlock(uint64_t blockId);
    bool hasBlock(uint64_t blockId) const;

    StorageError demoteBlock(uint64_t blockId);
    StorageError promoteBlock(uint64_t blockId);

    void spillHotToCold(size_t blocksToSpill = 1);

    [[nodiscard]] TierLevel getBlockTier(uint64_t blockId) const;
    [[nodiscard]] TieredStorageStats stats() const;
    StorageError sync();
    void clear();

private:
    std::unique_ptr<IStorageBackend> hotTier_;
    std::unique_ptr<IStorageBackend> coldTier_;
    size_t maxHotBlocks_;

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, TierLevel> blockTiers_;
    std::deque<uint64_t> hotLruQueue_;
    TieredStorageStats stats_;

    void touchHot(uint64_t blockId);
};

} // namespace backend
} // namespace storage
} // namespace nebula
