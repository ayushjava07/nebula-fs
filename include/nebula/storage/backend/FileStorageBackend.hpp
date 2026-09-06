#pragma once

#include "nebula/storage/backend/StorageBackend.hpp"
#include <filesystem>
#include <mutex>
#include <string>

namespace nebula {
namespace storage {
namespace backend {

class FileStorageBackend : public IStorageBackend {
public:
    explicit FileStorageBackend(std::filesystem::path rootDirectory, size_t maxCapacityBytes = 1024ULL * 1024 * 1024);
    ~FileStorageBackend() override = default;

    StorageError writeBlock(uint64_t blockId, std::span<const uint8_t> data) override;
    StorageResult<std::vector<uint8_t>> readBlock(uint64_t blockId) override;
    StorageResult<std::vector<uint8_t>> readSpan(uint64_t blockId, size_t offset, size_t length) override;
    StorageError deleteBlock(uint64_t blockId) override;
    bool hasBlock(uint64_t blockId) const override;
    StorageStats stats() const override;
    StorageError sync() override;
    void clear() override;
    const std::string& name() const noexcept override { return name_; }

    [[nodiscard]] const std::filesystem::path& rootDirectory() const noexcept { return rootDirectory_; }

private:
    std::filesystem::path getBlockPath(uint64_t blockId) const;

    std::string name_{"FileStorageBackend"};
    std::filesystem::path rootDirectory_;
    size_t maxCapacityBytes_;
    uint64_t currentBytes_{0};
    uint64_t blockCount_{0};
    mutable std::mutex mutex_;
    mutable StorageStats stats_;
};

} // namespace backend
} // namespace storage
} // namespace nebula
