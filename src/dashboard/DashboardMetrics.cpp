#include "nebula/dashboard/DashboardMetrics.hpp"

#include <cmath>
#include <array>
#include <numeric>
#include <algorithm>

namespace nebula {
namespace dashboard {

double DashboardMetricsCollector::calculateShannonEntropy(std::span<const uint8_t> data) noexcept {
    if (data.empty()) {
        return 0.0;
    }

    std::array<size_t, 256> frequencies{};
    for (uint8_t b : data) {
        frequencies[b]++;
    }

    const double total = static_cast<double>(data.size());
    double entropy = 0.0;

    for (size_t count : frequencies) {
        if (count > 0) {
            double p = static_cast<double>(count) / total;
            entropy -= p * std::log2(p);
        }
    }

    return entropy;
}

BlockEntropyClass DashboardMetricsCollector::classifyEntropy(double entropy, size_t nonZeroBytes, size_t totalBytes) noexcept {
    if (totalBytes == 0 || nonZeroBytes == 0) {
        return BlockEntropyClass::ZeroFilled;
    }
    if (entropy < 5.0) {
        return BlockEntropyClass::LowEntropyText;
    }
    if (entropy < 6.8) {
        return BlockEntropyClass::StructuredBinary;
    }
    if (entropy < 7.85) {
        return BlockEntropyClass::HighEntropyCompressed;
    }
    return BlockEntropyClass::EncryptedUniform;
}

EntropySample DashboardMetricsCollector::sampleBlockEntropy(uint64_t offset, std::span<const uint8_t> blockData) noexcept {
    EntropySample sample;
    sample.offset = offset;
    sample.size = blockData.size();
    if (blockData.empty()) {
        sample.shannonEntropy = 0.0;
        sample.classification = BlockEntropyClass::ZeroFilled;
        return sample;
    }

    sample.shannonEntropy = calculateShannonEntropy(blockData);

    size_t nonZero = 0;
    for (uint8_t b : blockData) {
        if (b != 0) nonZero++;
    }

    sample.classification = classifyEntropy(sample.shannonEntropy, nonZero, blockData.size());
    return sample;
}

double DashboardMetricsCollector::calculateFragmentation(const std::vector<uint64_t>& blockOffsets,
                                                         const std::vector<uint64_t>& blockSizes,
                                                         uint64_t totalSpan) noexcept {
    if (blockOffsets.size() <= 1 || totalSpan == 0) {
        return 0.0;
    }

    size_t n = std::min(blockOffsets.size(), blockSizes.size());
    uint64_t totalWastedGap = 0;

    for (size_t i = 1; i < n; ++i) {
        uint64_t prevEnd = blockOffsets[i - 1] + blockSizes[i - 1];
        if (blockOffsets[i] > prevEnd) {
            totalWastedGap += (blockOffsets[i] - prevEnd);
        }
    }

    double ratio = static_cast<double>(totalWastedGap) / static_cast<double>(totalSpan);
    return std::clamp(ratio, 0.0, 1.0);
}

void DashboardMetricsCollector::recordCacheAccess(bool hit) noexcept {
    if (hit) {
        cacheHits_++;
    } else {
        cacheMisses_++;
    }
}

void DashboardMetricsCollector::recordJournalTx(bool committed) noexcept {
    if (committed) {
        journalCommitted_++;
    } else {
        journalPending_++;
    }
}

void DashboardMetricsCollector::updateStorageStats(uint64_t entries, uint64_t rawBytes, uint64_t compBytes, size_t blocks) noexcept {
    totalEntries_ = entries;
    uncompressedBytes_ = rawBytes;
    compressedBytes_ = compBytes;
    activeBlocks_ = blocks;
}

DashboardMetricsSnapshot DashboardMetricsCollector::snapshot() const noexcept {
    DashboardMetricsSnapshot s;
    s.totalEntries = totalEntries_;
    s.uncompressedBytes = uncompressedBytes_;
    s.compressedBytes = compressedBytes_;
    s.activeBlocks = activeBlocks_;

    if (compressedBytes_ > 0) {
        s.compressionRatio = static_cast<double>(uncompressedBytes_) / static_cast<double>(compressedBytes_);
    } else if (uncompressedBytes_ > 0) {
        s.compressionRatio = 1.0;
    } else {
        s.compressionRatio = 1.0;
    }

    if (uncompressedBytes_ > 0 && uncompressedBytes_ >= compressedBytes_) {
        s.spaceSavingsPercent = (1.0 - (static_cast<double>(compressedBytes_) / static_cast<double>(uncompressedBytes_))) * 100.0;
    } else {
        s.spaceSavingsPercent = 0.0;
    }

    s.journalCommittedTx = journalCommitted_;
    s.journalPendingTx = journalPending_;
    s.cacheHits = cacheHits_;
    s.cacheMisses = cacheMisses_;

    uint64_t totalCache = cacheHits_ + cacheMisses_;
    s.cacheHitRatio = (totalCache > 0) ? (static_cast<double>(cacheHits_) / static_cast<double>(totalCache)) : 0.0;
    s.fragmentationIndex = cachedFragmentation_;

    return s;
}

void DashboardMetricsCollector::reset() noexcept {
    totalEntries_ = 0;
    uncompressedBytes_ = 0;
    compressedBytes_ = 0;
    activeBlocks_ = 0;
    journalCommitted_ = 0;
    journalPending_ = 0;
    cacheHits_ = 0;
    cacheMisses_ = 0;
    cachedFragmentation_ = 0.0;
}

} // namespace dashboard
} // namespace nebula
