#include "nebula/storage/backend/FileStorageBackend.hpp"
#include "nebula/utils/Checksum.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace nebula {
namespace storage {
namespace backend {

namespace {
constexpr uint32_t kBlockMagic = 0x4E424C4B; // 'NBLK'

#pragma pack(push, 1)
struct DiskBlockHeader {
    uint32_t magic{kBlockMagic};
    uint32_t checksum{0};
    uint64_t dataSize{0};
};
#pragma pack(pop)
} // namespace

FileStorageBackend::FileStorageBackend(fs::path rootDirectory, size_t maxCapacityBytes)
    : rootDirectory_(std::move(rootDirectory)), maxCapacityBytes_(maxCapacityBytes) {
    stats_.capacityBytes = maxCapacityBytes_;
    std::error_code ec;
    fs::create_directories(rootDirectory_, ec);

    // Scan existing blocks to populate stats
    if (fs::exists(rootDirectory_)) {
        for (const auto& entry : fs::recursive_directory_iterator(rootDirectory_, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".blk") {
                blockCount_++;
                currentBytes_ += entry.file_size();
            }
        }
    }
}

fs::path FileStorageBackend::getBlockPath(uint64_t blockId) const {
    uint32_t bucket = static_cast<uint32_t>(blockId % 256);
    std::ostringstream ss;
    ss << std::hex << std::setw(2) << std::setfill('0') << bucket;
    return rootDirectory_ / ss.str() / (std::to_string(blockId) + ".blk");
}

StorageError FileStorageBackend::writeBlock(uint64_t blockId, std::span<const uint8_t> data) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t totalNewBytes = sizeof(DiskBlockHeader) + data.size();
    if (currentBytes_ + totalNewBytes > maxCapacityBytes_) {
        return StorageError::CapacityExceeded;
    }

    fs::path targetPath = getBlockPath(blockId);
    std::error_code ec;
    fs::create_directories(targetPath.parent_path(), ec);
    if (ec) {
        return StorageError::IOError;
    }

    fs::path tempPath = targetPath;
    tempPath += ".tmp";

    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return StorageError::IOError;
    }

    DiskBlockHeader header;
    header.magic = kBlockMagic;
    header.checksum = utils::ChecksumEngine::crc32(data);
    header.dataSize = data.size();

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }

    out.flush();
    if (!out) {
        fs::remove(tempPath, ec);
        return StorageError::IOError;
    }
    out.close();

    bool existed = fs::exists(targetPath, ec);
    size_t oldSize = existed ? fs::file_size(targetPath, ec) : 0;

    fs::rename(tempPath, targetPath, ec);
    if (ec) {
        fs::remove(tempPath, ec);
        return StorageError::IOError;
    }

    if (!existed) {
        blockCount_++;
    }
    currentBytes_ = currentBytes_ + totalNewBytes - oldSize;
    stats_.writeOps++;
    stats_.totalBlocks = blockCount_;
    stats_.totalBytes = currentBytes_;

    return StorageError::Success;
}

StorageResult<std::vector<uint8_t>> FileStorageBackend::readBlock(uint64_t blockId) {
    std::lock_guard<std::mutex> lock(mutex_);

    fs::path path = getBlockPath(blockId);
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return StorageError::NotFound;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return StorageError::IOError;
    }

    DiskBlockHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in || header.magic != kBlockMagic) {
        return StorageError::CorruptBlock;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(header.dataSize));
    if (header.dataSize > 0) {
        in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(header.dataSize));
        if (!in) {
            return StorageError::CorruptBlock;
        }
    }

    uint32_t computed = utils::ChecksumEngine::crc32(buffer);
    if (computed != header.checksum) {
        return StorageError::CorruptBlock;
    }

    stats_.readOps++;
    return buffer;
}

StorageResult<std::vector<uint8_t>> FileStorageBackend::readSpan(uint64_t blockId, size_t offset, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);

    fs::path path = getBlockPath(blockId);
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return StorageError::NotFound;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return StorageError::IOError;
    }

    DiskBlockHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in || header.magic != kBlockMagic) {
        return StorageError::CorruptBlock;
    }

    if (offset + length > header.dataSize) {
        return StorageError::InvalidOffset;
    }

    in.seekg(static_cast<std::streamoff>(sizeof(DiskBlockHeader) + offset));
    if (!in) {
        return StorageError::IOError;
    }

    std::vector<uint8_t> buffer(length);
    if (length > 0) {
        in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(length));
        if (!in) {
            return StorageError::IOError;
        }
    }

    stats_.readOps++;
    return buffer;
}

StorageError FileStorageBackend::deleteBlock(uint64_t blockId) {
    std::lock_guard<std::mutex> lock(mutex_);

    fs::path path = getBlockPath(blockId);
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return StorageError::NotFound;
    }

    auto sz = fs::file_size(path, ec);
    if (!fs::remove(path, ec)) {
        return StorageError::IOError;
    }

    if (blockCount_ > 0) blockCount_--;
    currentBytes_ = (currentBytes_ >= sz) ? (currentBytes_ - sz) : 0;
    stats_.deleteOps++;
    stats_.totalBlocks = blockCount_;
    stats_.totalBytes = currentBytes_;

    return StorageError::Success;
}

bool FileStorageBackend::hasBlock(uint64_t blockId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    return fs::exists(getBlockPath(blockId), ec);
}

StorageStats FileStorageBackend::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    StorageStats s = stats_;
    s.totalBlocks = blockCount_;
    s.totalBytes = currentBytes_;
    s.capacityBytes = maxCapacityBytes_;
    return s;
}

StorageError FileStorageBackend::sync() {
    return StorageError::Success;
}

void FileStorageBackend::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    fs::remove_all(rootDirectory_, ec);
    fs::create_directories(rootDirectory_, ec);
    blockCount_ = 0;
    currentBytes_ = 0;
    stats_.totalBlocks = 0;
    stats_.totalBytes = 0;
}

} // namespace backend
} // namespace storage
} // namespace nebula
