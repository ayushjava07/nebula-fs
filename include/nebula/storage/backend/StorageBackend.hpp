#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <span>
#include <variant>
#include <memory>

namespace nebula {
namespace storage {
namespace backend {

enum class StorageError : uint8_t {
    Success = 0,
    NotFound,
    AlreadyExists,
    CapacityExceeded,
    IOError,
    CorruptBlock,
    InvalidOffset,
    InvalidParameter
};

template <typename T>
using StorageResult = std::variant<T, StorageError>;

inline bool isSuccess(StorageError err) noexcept {
    return err == StorageError::Success;
}

inline std::string storageErrorMessage(StorageError err) {
    switch (err) {
        case StorageError::Success: return "Success";
        case StorageError::NotFound: return "Block not found";
        case StorageError::AlreadyExists: return "Block already exists";
        case StorageError::CapacityExceeded: return "Storage capacity exceeded";
        case StorageError::IOError: return "I/O subsystem failure";
        case StorageError::CorruptBlock: return "Block data corruption detected";
        case StorageError::InvalidOffset: return "Invalid block offset or length";
        case StorageError::InvalidParameter: return "Invalid parameter";
    }
    return "Unknown storage error";
}

struct StorageBlock {
    uint64_t blockId{0};
    uint64_t physicalOffset{0};
    uint64_t logicalSize{0};
    uint32_t checksum{0};
};

struct StorageStats {
    uint64_t totalBlocks{0};
    uint64_t totalBytes{0};
    uint64_t capacityBytes{0};
    uint64_t readOps{0};
    uint64_t writeOps{0};
    uint64_t deleteOps{0};
};

class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    virtual StorageError writeBlock(uint64_t blockId, std::span<const uint8_t> data) = 0;
    virtual StorageResult<std::vector<uint8_t>> readBlock(uint64_t blockId) = 0;
    virtual StorageResult<std::vector<uint8_t>> readSpan(uint64_t blockId, size_t offset, size_t length) = 0;
    virtual StorageError deleteBlock(uint64_t blockId) = 0;
    virtual bool hasBlock(uint64_t blockId) const = 0;
    virtual StorageStats stats() const = 0;
    virtual StorageError sync() = 0;
    virtual void clear() = 0;
    virtual const std::string& name() const noexcept = 0;
};

using StorageBackendPtr = std::unique_ptr<IStorageBackend>;

} // namespace backend
} // namespace storage
} // namespace nebula
