#include "nebula/storage/ChunkManager.hpp"
#include "nebula/utils/VarInt.hpp"
#include <cstring>
#include <system_error>
#include <algorithm>

namespace nebula {
namespace storage {

ChunkManager::ChunkManager(ChunkConfig config) : config_(config) {
    checksum_ = std::make_unique<utils::ChecksumEngine>(config_.hashAlgorithm);
}

ChunkManager::~ChunkManager() noexcept = default;

ChunkManager::ChunkManager(ChunkManager&& other) noexcept
    : config_(other.config_)
    , chunks_(std::move(other.chunks_))
    , hashIndex_(std::move(other.hashIndex_))
    , totalDataSize_(other.totalDataSize_)
    , totalStoredSize_(other.totalStoredSize_)
    , checksum_(std::move(other.checksum_)) {}

ChunkManager& ChunkManager::operator=(ChunkManager&& other) noexcept {
    if (this != &other) {
        config_ = other.config_;
        chunks_ = std::move(other.chunks_);
        hashIndex_ = std::move(other.hashIndex_);
        totalDataSize_ = other.totalDataSize_;
        totalStoredSize_ = other.totalStoredSize_;
        checksum_ = std::move(other.checksum_);
    }
    return *this;
}

std::vector<ChunkDescriptor> ChunkManager::chunkData(std::span<const uint8_t> data) {
    std::vector<ChunkDescriptor> descriptors;

    auto boundaries = (config_.dedupStrategy == DedupStrategy::Content)
        ? findChunkBoundaries(data)
        : findFixedBoundaries(data.size());

    size_t start = 0;
    for (size_t boundary : boundaries) {
        auto chunk = data.subspan(start, boundary - start);
        auto descriptor = findOrCreateChunk(chunk);
        descriptors.push_back(descriptor);
        start = boundary;
    }

    if (start < data.size()) {
        auto chunk = data.subspan(start);
        auto descriptor = findOrCreateChunk(chunk);
        descriptors.push_back(descriptor);
    }

    return descriptors;
}

ChunkDescriptor ChunkManager::findOrCreateChunk(std::span<const uint8_t> data) {
    auto hash = computeHash(data);

    auto it = hashIndex_.find(*reinterpret_cast<const uint64_t*>(hash.data()));
    if (it != hashIndex_.end()) {
        return chunks_[it->second];
    }

    ChunkDescriptor desc;
    desc.hash = hash;
    desc.originalSize = data.size();
    desc.compressedSize = 0;
    desc.compression = CompressionAlgorithm::None;
    desc.encrypted = false;

    chunks_.push_back(desc);
    hashIndex_[*reinterpret_cast<const uint64_t*>(hash.data())] = chunks_.size() - 1;
    totalDataSize_ += data.size();
    totalStoredSize_ += data.size();

    return desc;
}

bool ChunkManager::hasChunk(const ChecksumValue& hash) const {
    return hashIndex_.find(*reinterpret_cast<const uint64_t*>(hash.data())) != hashIndex_.end();
}

void ChunkManager::registerChunk(const ChunkDescriptor& chunk) {
    auto it = hashIndex_.find(*reinterpret_cast<const uint64_t*>(chunk.hash.data()));
    if (it != hashIndex_.end()) return;

    chunks_.push_back(chunk);
    hashIndex_[*reinterpret_cast<const uint64_t*>(chunk.hash.data())] = chunks_.size() - 1;
    totalDataSize_ += chunk.originalSize;
    totalStoredSize_ += chunk.compressedSize;
}

std::optional<ChunkDescriptor> ChunkManager::getChunk(const ChecksumValue& hash) const {
    auto it = hashIndex_.find(*reinterpret_cast<const uint64_t*>(hash.data()));
    if (it == hashIndex_.end()) return std::nullopt;
    return chunks_[it->second];
}

double ChunkManager::dedupRatio() const noexcept {
    if (totalDataSize_ == 0) return 1.0;
    return static_cast<double>(totalStoredSize_) / static_cast<double>(totalDataSize_);
}

void ChunkManager::clear() noexcept {
    chunks_.clear();
    hashIndex_.clear();
    totalDataSize_ = 0;
    totalStoredSize_ = 0;
}

std::vector<uint8_t> ChunkManager::serialize() const {
    std::vector<uint8_t> result;
    utils::VarInt::encode(static_cast<uint64_t>(chunks_.size()), result);

    for (const auto& chunk : chunks_) {
        result.insert(result.end(), chunk.hash.begin(), chunk.hash.end());
        utils::VarInt::encode(chunk.offset, result);
        utils::VarInt::encode(chunk.compressedSize, result);
        utils::VarInt::encode(chunk.originalSize, result);
        result.push_back(static_cast<uint8_t>(chunk.compression));
        result.push_back(chunk.encrypted ? 1 : 0);
    }

    return result;
}

std::error_code ChunkManager::deserialize(std::span<const uint8_t> data) {
    clear();

    size_t offset = 0;
    auto countResult = utils::VarInt::decode(data);
    if (!countResult.valid) {
        return make_error_code(ErrorCode::CorruptChunkTable);
    }

    uint64_t count = countResult.value;
    offset = countResult.consumed;

    constexpr size_t MAX_CHUNKS = 1024 * 1024;
    if (count > MAX_CHUNKS) {
        return make_error_code(ErrorCode::CorruptChunkTable);
    }

    constexpr size_t MIN_CHUNK_SIZE = 37; // 32 hash + 3×VarInt min + 2 flags
    if (offset <= data.size() && count > (data.size() - offset) / MIN_CHUNK_SIZE) {
        return make_error_code(ErrorCode::CorruptChunkTable);
    }

    chunks_.reserve(static_cast<size_t>(count));

    for (uint64_t i = 0; i < count; ++i) {
        if (offset + 32 > data.size()) {
            return make_error_code(ErrorCode::CorruptChunkTable);
        }

        ChunkDescriptor chunk;
        std::memcpy(chunk.hash.data(), &data[offset], 32);
        offset += 32;

        auto offResult = utils::VarInt::decode(data.subspan(offset));
        if (!offResult.valid) return make_error_code(ErrorCode::CorruptChunkTable);
        chunk.offset = offResult.value;
        offset += offResult.consumed;

        auto csResult = utils::VarInt::decode(data.subspan(offset));
        if (!csResult.valid) return make_error_code(ErrorCode::CorruptChunkTable);
        chunk.compressedSize = csResult.value;
        offset += csResult.consumed;

        auto osResult = utils::VarInt::decode(data.subspan(offset));
        if (!osResult.valid) return make_error_code(ErrorCode::CorruptChunkTable);
        chunk.originalSize = osResult.value;
        offset += osResult.consumed;

        chunk.compression = static_cast<CompressionAlgorithm>(data[offset++]);
        chunk.encrypted = (data[offset++] != 0);

        registerChunk(chunk);
    }

    return std::error_code();
}

ChecksumValue ChunkManager::computeHash(std::span<const uint8_t> data) const {
    return utils::ChecksumEngine::compute(data, config_.hashAlgorithm);
}

std::vector<size_t> ChunkManager::findChunkBoundaries(std::span<const uint8_t> data) const {
    std::vector<size_t> boundaries;

    if (data.size() <= config_.maxChunkSize) {
        return boundaries;
    }

    const size_t windowSize = 48;
    const uint32_t mask = static_cast<uint32_t>(config_.avgChunkSize) - 1;
    size_t lastBoundary = 0;

    for (size_t i = config_.minChunkSize; i < data.size(); ++i) {
        if (i - lastBoundary >= config_.maxChunkSize) {
            boundaries.push_back(i);
            lastBoundary = i;
            continue;
        }

        if (i - lastBoundary < config_.minChunkSize) continue;

        size_t windowStart = (i >= windowSize) ? (i - windowSize) : 0;
        size_t windowLen = std::min(windowSize, data.size() - windowStart);
        auto window = data.subspan(windowStart, windowLen);
        uint32_t hash = buzhash(window);

        if ((hash & mask) == 0) {
            boundaries.push_back(i);
            lastBoundary = i;
        }
    }

    return boundaries;
}

std::vector<size_t> ChunkManager::findFixedBoundaries(size_t dataSize) const {
    std::vector<size_t> boundaries;
    for (size_t i = config_.maxChunkSize; i < dataSize; i += config_.maxChunkSize) {
        boundaries.push_back(i);
    }
    return boundaries;
}

uint32_t ChunkManager::buzhash(std::span<const uint8_t> data) const noexcept {
    static const uint32_t table[256] = {
        0x458be752, 0xc10748cc, 0xbb6bbbed, 0x6cdc21c3, 0x9c79ed0a, 0x5c64cb2f,
        0xb1c0f24f, 0xe42b8bf5, 0xbe97f3cb, 0x90c52b3e, 0x2389a777, 0xccfb7b41,
        0x21b60c9b, 0x7f00e742, 0x70a11b46, 0x3045f586, 0xef4f51c7, 0x64bb9e50,
        0x8f13ecba, 0xe1a469c2, 0x8d124b7d, 0x94e4318a, 0x6685e47a, 0x6f9a4bb6,
        0x2e1e5cd7, 0x0159ee3c, 0x87fc8e3c, 0xe36fcb8c, 0xa90fa72a, 0x22b3d8f6,
        0x62b0ca27, 0x9e57f4d0, 0x8163986b, 0x4c1578c5, 0xefa9dc7f, 0xb974582c,
        0x3182f82e, 0xaa0f09e3, 0xbce08e63, 0x1e2f5e5d, 0x7ab1b6a7, 0xf7416dbd,
        0xd53c15f2, 0xb917cc82, 0x481754b5, 0x287aefb7, 0x506ebfa8, 0x4f43ec03,
        0x0418f5f4, 0xf1c1b5f8, 0x16a8e1d4, 0xa53ba74e, 0x4c8eb6d9, 0x7703e8f0,
        0x5e75bd1f, 0xfaca8804, 0x765b5739, 0x0bb1b0ac, 0x5ed6ef36, 0x760a05e4,
        0x5f6122ce, 0x87a4244e, 0x50cd137c, 0xc1e08db9, 0x25f77ab2, 0x169558bc,
        0x9e9d48ad, 0x97f3f72e, 0xcffae03c, 0x1a6134d8, 0x2fc1e4dd, 0x3b9c1d41,
        0x0270c4ac, 0x1701f6dd, 0x777a49e6, 0xc81c7c22, 0x5909bee8, 0x5689c44b,
        0x0e6393cc, 0x5144f36c, 0xae88d62e, 0x11e0c7d5, 0xa4bf9e3e, 0xcc9b82db,
        0xaa58dfc2, 0x95f27b82, 0x0c37acd4, 0xac2f7a07, 0xdccbecb5, 0xc68bd3ad,
        0x69764450, 0x9e30798a, 0xe6f4c52e, 0x6cbb5cf1, 0x8d18ade5, 0xd1b46b36,
        0xad3a21b9, 0xe37ae2f7, 0x5759b49d, 0x8cf9b4fb, 0x32bf5214, 0x57454ed2,
        0xb094145a, 0xc20b44c5, 0xe02acfae, 0x5bad8a0a, 0x7632e312, 0x894264ed,
        0x6180f56c, 0x5a551689, 0xd1b80e02, 0x4f61b581, 0x0a7d93bb, 0x29aecbd3,
        0x3089bea2, 0xb944408d, 0x5a1de2e5, 0xe5cbebdc, 0xfee6ed4e, 0x65cc849c,
        0x72ec34b3, 0x504501c7, 0x5983a27f, 0x1cccaf2a, 0x5f35cb47, 0x82e317c5,
        0x96e37f23, 0xdfd64e22, 0x35b8c8b4, 0xeb90c242, 0xa74821db, 0x3082dd7d,
        0x82aa1eb1, 0xdf1804e6, 0xf17766ea, 0x04a2b4b7, 0x1339a829, 0xb9e180be,
        0x5dfc02f7, 0xf62344d9, 0xe4e9c4c2, 0x2217407d, 0x948a70f3, 0x46361439,
        0x21a3f2e9, 0x537d415a, 0x6058e22f, 0x0a74baf2, 0x7cd7258a, 0x38b09fd3,
        0xbccbbcf5, 0x8835f7aa, 0x2bd7bfa0, 0x5a5155ba, 0x7da575d8, 0xcbf6f8d0,
        0x40b77a7d, 0x78fe5dea, 0x7d378fb8, 0xa50f498f, 0x8c8370e0, 0x87485f95,
        0x1d6c92a7, 0xd77aac51, 0xacb164b3, 0xd4312513, 0x4ff2eceb, 0x6049a0de,
        0x0426bce8, 0x541e05e7, 0x2b9d55c5, 0x4726137e, 0x4a8062df, 0xfbac45da,
        0xbfb0ebe9, 0xdef6efb8, 0xa17eb9be, 0xd65a8559, 0x37ea5d98, 0xf2d9d74b,
        0xbbd5634b, 0xe11ebacc, 0x27a49a55, 0xe748e3e7, 0x73ec42b4, 0xd23ad3ba,
        0x2c6a8967, 0xad01eeb0, 0x0917a370, 0x9ba6eaa3, 0x28a92e2b, 0xf6355cb6,
        0x4ea32b96, 0xb90c0f47, 0xdee405d5, 0xd449afce, 0x3c8e7c63, 0x5df1924c,
        0x0f99cf36, 0x2a6173a9, 0x76929070, 0x106574e1, 0x9df3ae4b, 0x4cecb6c9,
        0x539cde0d, 0x373b45a4, 0x75d792a5, 0x2e4466ff, 0x886ba96a, 0xb1238e82,
        0xc027537d, 0x633ab3a0, 0x57f925e0, 0x382514f4
    };

    uint32_t hash = 0;
    for (uint8_t byte : data) {
        hash = (hash << 1) | (hash >> 31);
        hash ^= table[byte];
    }
    return hash;
}

} // namespace storage
} // namespace nebula
