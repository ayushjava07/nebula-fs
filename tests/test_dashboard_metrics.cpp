#include <gtest/gtest.h>
#include "nebula/dashboard/DashboardMetrics.hpp"

#include <vector>
#include <random>

using namespace nebula::dashboard;

TEST(DashboardMetricsTest, ZeroFilledDataEntropyIsZero) {
    std::vector<uint8_t> zeros(1024, 0);
    double entropy = DashboardMetricsCollector::calculateShannonEntropy(zeros);
    EXPECT_DOUBLE_EQ(entropy, 0.0);

    auto sample = DashboardMetricsCollector::sampleBlockEntropy(0, zeros);
    EXPECT_EQ(sample.classification, BlockEntropyClass::ZeroFilled);
    EXPECT_DOUBLE_EQ(sample.shannonEntropy, 0.0);
}

TEST(DashboardMetricsTest, AsciiTextEntropyClassification) {
    std::string text = "The quick brown fox jumps over the lazy dog repeatedly to generate textual entropy patterns.";
    std::vector<uint8_t> textBytes(text.begin(), text.end());

    double entropy = DashboardMetricsCollector::calculateShannonEntropy(textBytes);
    EXPECT_GT(entropy, 2.0);
    EXPECT_LT(entropy, 5.0);

    auto sample = DashboardMetricsCollector::sampleBlockEntropy(0, textBytes);
    EXPECT_EQ(sample.classification, BlockEntropyClass::LowEntropyText);
}

TEST(DashboardMetricsTest, HighEntropyClassification) {
    std::vector<uint8_t> uniform(2048);
    for (size_t i = 0; i < uniform.size(); ++i) {
        uniform[i] = static_cast<uint8_t>((i * 137 + 73) % 256);
    }

    double entropy = DashboardMetricsCollector::calculateShannonEntropy(uniform);
    EXPECT_GT(entropy, 7.5);

    auto sample = DashboardMetricsCollector::sampleBlockEntropy(100, uniform);
    EXPECT_TRUE(sample.classification == BlockEntropyClass::HighEntropyCompressed ||
                sample.classification == BlockEntropyClass::EncryptedUniform);
}

TEST(DashboardMetricsTest, FragmentationCalculation) {
    std::vector<uint64_t> offsets = {0, 1500, 3000};
    std::vector<uint64_t> sizes   = {1000, 1000, 1000};
    // Gaps: (1500 - 1000) = 500, (3000 - 2500) = 500. Total gap = 1000. Total span = 4000.
    // Ratio: 1000 / 4000 = 0.25
    double frag = DashboardMetricsCollector::calculateFragmentation(offsets, sizes, 4000);
    EXPECT_DOUBLE_EQ(frag, 0.25);
}

TEST(DashboardMetricsTest, SnapshotMetricsCalculations) {
    DashboardMetricsCollector collector;
    collector.updateStorageStats(10, 10000, 2500, 4);

    collector.recordCacheAccess(true);
    collector.recordCacheAccess(true);
    collector.recordCacheAccess(false);

    collector.recordJournalTx(true);
    collector.recordJournalTx(false);

    auto snap = collector.snapshot();
    EXPECT_EQ(snap.totalEntries, 10);
    EXPECT_EQ(snap.uncompressedBytes, 10000);
    EXPECT_EQ(snap.compressedBytes, 2500);
    EXPECT_DOUBLE_EQ(snap.compressionRatio, 4.0);
    EXPECT_DOUBLE_EQ(snap.spaceSavingsPercent, 75.0);
    EXPECT_EQ(snap.cacheHits, 2);
    EXPECT_EQ(snap.cacheMisses, 1);
    EXPECT_NEAR(snap.cacheHitRatio, 0.66666, 0.001);
    EXPECT_EQ(snap.journalCommittedTx, 1);
    EXPECT_EQ(snap.journalPendingTx, 1);

    collector.reset();
    auto resetSnap = collector.snapshot();
    EXPECT_EQ(resetSnap.totalEntries, 0);
    EXPECT_EQ(resetSnap.cacheHits, 0);
}
