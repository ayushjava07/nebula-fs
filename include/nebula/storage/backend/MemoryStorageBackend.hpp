#pragma once

#include "nebula/storage/backend/StorageBackend.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <string>

namespace nebula {
namespace storage {
namespace backend {

class MemoryStorageBackend : public IStorageBackend {
public:
    explicit MemoryStorageBackend(size_t maxCapacityBytes = 64 * 1024 * 1024);
    ~MemoryStorageBackend() override = default;

    StorageError writeBlock(uint64_t blockId, std::span<const uint8_t> data) override;
    StorageResult<std::vector<uint8_t>> readBlock(uint64_t blockId) override;
    StorageResult<std::vector<uint8_t>> readSpan(uint64_t blockId, size_t offset, size_t length) override;
    StorageError deleteBlock(uint64_t blockId) override;
    bool hasBlock(uint64_t blockId) const override;
    StorageStats stats() const override;
    StorageError sync() override;
    void clear() override;
    const std::string& name() const noexcept override { return name_; }

    void setCapacity(size_t maxCapacityBytes);

private:
    struct InternalBlock {
        std::vector<uint8_t> data;
        uint32_t checksum{0};
    };

    std::string name_{"MemoryStorageBackend"};
    size_t maxCapacityBytes_;
    uint64_t currentBytes_{0};
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint64_t, InternalBlock> blocks_;
    mutable StorageStats stats_;
};

} // namespace backend
} // namespace storage
} // namespace nebula
