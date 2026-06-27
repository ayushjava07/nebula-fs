#include "nebula/compression/CompressionBlock.hpp"
#include "nebula/utils/Checksum.hpp"
#include <cstring>
#include <algorithm>

namespace nebula {
namespace compression {

std::span<const uint8_t> CompressionBlock::getDecompressedData() const {
    if (!isDecompressed) {
        if (algo == CompressionAlgorithm::None) {
            decompressedData = compressedData;
        } else {
            CompressionEngine engine(CompressionConfig{algo, 0, 0, false, 0});
            auto result = engine.decompress(compressedData, originalSize);
            if (result.success) {
                decompressedData = std::move(result.data);
            }
        }
        isDecompressed = true;
    }
    return decompressedData;
}

bool CompressionBlock::verifyIntegrity() const {
    auto data = getDecompressedData();
    if (data.empty() && originalSize > 0) return false;
    if (checksum == 0) return true;  // no checksum to verify
    auto computed = utils::ChecksumEngine::crc32(data);
    return computed == checksum;
}

size_t CompressionBlock::headerSize() const noexcept {
    return sizeof(nebula::format::BlockHeader);
}

void BlockCollection::addBlock(CompressionBlock block) {
    blocks_.push_back(std::move(block));
}

const CompressionBlock* BlockCollection::getBlock(size_t index) const {
    if (index >= blocks_.size()) return nullptr;
    return &blocks_[index];
}

size_t BlockCollection::totalCompressedSize() const noexcept {
    size_t total = 0;
    for (const auto& block : blocks_) {
        total += block.compressedSize;
    }
    return total;
}

size_t BlockCollection::totalOriginalSize() const noexcept {
    size_t total = 0;
    for (const auto& block : blocks_) {
        total += block.originalSize;
    }
    return total;
}

const CompressionBlock* BlockCollection::findBlock(uint64_t originalOffset) const {
    uint64_t offset = 0;
    for (const auto& block : blocks_) {
        if (originalOffset >= offset && originalOffset < offset + block.originalSize) {
            return &block;
        }
        offset += block.originalSize;
    }
    return nullptr;
}

void BlockCollection::clear() noexcept {
    blocks_.clear();
}

void BlockCollection::reserve(size_t count) {
    blocks_.reserve(count);
}

} // namespace compression
} // namespace nebula
