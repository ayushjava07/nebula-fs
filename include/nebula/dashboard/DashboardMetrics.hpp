#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>

namespace nebula {
namespace dashboard {

enum class BlockEntropyClass : uint8_t {
    ZeroFilled,
    LowEntropyText,
    StructuredBinary,
    HighEntropyCompressed,
    EncryptedUniform
};

struct EntropySample {
    uint64_t offset{0};
    uint64_t size{0};
    double shannonEntropy{0.0};
    BlockEntropyClass classification{BlockEntropyClass::ZeroFilled};
};

struct DashboardMetricsSnapshot {
    uint64_t totalEntries{0};
    uint64_t uncompressedBytes{0};
    uint64_t compressedBytes{0};
    double compressionRatio{1.0};
    double spaceSavingsPercent{0.0};
    size_t activeBlocks{0};
    double fragmentationIndex{0.0};
    uint64_t journalCommittedTx{0};
    uint64_t journalPendingTx{0};
    uint64_t cacheHits{0};
    uint64_t cacheMisses{0};
    double cacheHitRatio{0.0};
};

class DashboardMetricsCollector {
public:
    DashboardMetricsCollector() = default;

    static double calculateShannonEntropy(std::span<const uint8_t> data) noexcept;
    static BlockEntropyClass classifyEntropy(double entropy, size_t nonZeroBytes, size_t totalBytes) noexcept;
    static EntropySample sampleBlockEntropy(uint64_t offset, std::span<const uint8_t> blockData) noexcept;

    static double calculateFragmentation(const std::vector<uint64_t>& blockOffsets,
                                         const std::vector<uint64_t>& blockSizes,
                                         uint64_t totalSpan) noexcept;

    void recordCacheAccess(bool hit) noexcept;
    void recordJournalTx(bool committed) noexcept;
    void updateStorageStats(uint64_t entries, uint64_t rawBytes, uint64_t compBytes, size_t blocks) noexcept;

    [[nodiscard]] DashboardMetricsSnapshot snapshot() const noexcept;
    void reset() noexcept;

private:
    uint64_t totalEntries_{0};
    uint64_t uncompressedBytes_{0};
    uint64_t compressedBytes_{0};
    size_t activeBlocks_{0};
    uint64_t journalCommitted_{0};
    uint64_t journalPending_{0};
    uint64_t cacheHits_{0};
    uint64_t cacheMisses_{0};
    double cachedFragmentation_{0.0};
};

} // namespace dashboard
} // namespace nebula
