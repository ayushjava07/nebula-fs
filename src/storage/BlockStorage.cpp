#include "nebula/storage/BlockStorage.hpp"
#include "nebula/utils/Checksum.hpp"
#include <cstring>
#include <system_error>
#include <algorithm>

namespace nebula {
namespace storage {

BlockStorage::BlockStorage(BlockStorage&& other) noexcept
    : blockData_(std::move(other.blockData_)) {}

BlockStorage& BlockStorage::operator=(BlockStorage&& other) noexcept {
    if (this != &other) {
        blockData_ = std::move(other.blockData_);
    }
    return *this;
}

std::error_code BlockStorage::writeBlock(std::span<const uint8_t> data,
                                          CompressionAlgorithm algo,
                                          bool encrypt,
                                          ChunkDescriptor& descriptor) {
    descriptor.originalSize = data.size();
    descriptor.compression = algo;

    compression::CompressionBlock block;
    block.originalSize = data.size();
    block.algo = algo;
    block.compressedData.assign(data.begin(), data.end());
    block.compressedSize = data.size();
    block.checksum = utils::ChecksumEngine::crc32(data);

    if (algo != CompressionAlgorithm::None) {
        compression::CompressionEngine engine(
            compression::CompressionConfig{algo, 3, 0, false, 0});
        auto result = engine.compress(data);
        if (result.success) {
            block.compressedData = std::move(result.data);
            block.compressedSize = block.compressedData.size();
        }
    }

    auto serialized = serializeBlock(block);
    descriptor.offset = 0;
    descriptor.compressedSize = serialized.size();

    blockData_ = std::move(serialized);
    return std::error_code();
}

std::error_code BlockStorage::readBlock(const ChunkDescriptor& descriptor,
                                         std::vector<uint8_t>& output) {
    if (descriptor.compression == CompressionAlgorithm::None) {
        output.resize(descriptor.originalSize);
        return std::error_code();
    }

    compression::CompressionEngine engine(
        compression::CompressionConfig{descriptor.compression, 3, 0, false, 0});
    auto result = engine.decompress(blockData_, static_cast<size_t>(descriptor.originalSize));
    if (!result.success) {
        return make_error_code(ErrorCode::DecompressionError);
    }

    output = std::move(result.data);
    return std::error_code();
}

std::vector<uint8_t> BlockStorage::readRawBlock(std::span<const uint8_t> blocksSection,
                                                  const ChunkDescriptor& descriptor) {
    if (descriptor.offset + descriptor.compressedSize > blocksSection.size()) {
        return {};
    }

    return std::vector<uint8_t>(
        blocksSection.begin() + static_cast<ptrdiff_t>(descriptor.offset),
        blocksSection.begin() + static_cast<ptrdiff_t>(descriptor.offset + descriptor.compressedSize));
}

std::vector<uint8_t> BlockStorage::serializeBlock(const compression::CompressionBlock& block) {
    format::BlockHeader header;
    header.sync[0] = 'N';
    header.sync[1] = 'B';
    header.compressedSize = block.compressedSize;
    header.originalSize = block.originalSize;
    header.compression = block.algo;
    header.encryption = EncryptionAlgorithm::None;
    header.encrypted = block.encrypted ? 1 : 0;

    std::vector<uint8_t> result(sizeof(header));
    std::memcpy(result.data(), &header, sizeof(header));
    result.insert(result.end(), block.compressedData.begin(), block.compressedData.end());

    return result;
}

Result<compression::CompressionBlock> BlockStorage::deserializeBlock(
    std::span<const uint8_t> data, size_t offset) {
    if (offset + sizeof(format::BlockHeader) > data.size()) {
        return toParseError(ErrorCode::CorruptBlock, ParserState::CompressedBlocks,
                           static_cast<uint64_t>(offset), "block header truncated");
    }

    format::BlockHeader header;
    std::memcpy(&header, &data[offset], sizeof(format::BlockHeader));

    if (header.sync[0] != 'N' || header.sync[1] != 'B') {
        return toParseError(ErrorCode::CorruptBlock, ParserState::CompressedBlocks,
                           static_cast<uint64_t>(offset), "invalid block sync");
    }

    compression::CompressionBlock block;
    block.offset = offset;
    block.compressedSize = static_cast<size_t>(header.compressedSize);
    block.originalSize = static_cast<size_t>(header.originalSize);
    block.algo = header.compression;
    block.encrypted = header.encrypted != 0;

    size_t blockStart = offset + sizeof(format::BlockHeader);
    if (blockStart + block.compressedSize > data.size()) {
        return toParseError(ErrorCode::CorruptBlock, ParserState::CompressedBlocks,
                           static_cast<uint64_t>(blockStart), "block data truncated");
    }

    block.compressedData.assign(
        data.begin() + static_cast<ptrdiff_t>(blockStart),
        data.begin() + static_cast<ptrdiff_t>(blockStart + block.compressedSize));

    return block;
}

bool BlockStorage::verifyBlock(std::span<const uint8_t> blockData) {
    if (blockData.size() < sizeof(format::BlockHeader)) return false;

    format::BlockHeader header;
    std::memcpy(&header, blockData.data(), sizeof(format::BlockHeader));

    return header.sync[0] == 'N' && header.sync[1] == 'B';
}

} // namespace storage
} // namespace nebula
