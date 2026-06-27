#include <gtest/gtest.h>
#include "nebula/filesystem/Recovery.hpp"

namespace nebula {
namespace test {

TEST(RecoveryTest, NoRecoveryNeeded) {
    filesystem::JournalManager jm;
    filesystem::Recovery recovery;
    EXPECT_FALSE(recovery.needsRecovery(jm));
}

TEST(RecoveryTest, NeedsRecovery) {
    filesystem::JournalManager jm;
    jm.beginCheckpoint();
    jm.logCreate(1, std::span<const uint8_t>());

    filesystem::Recovery recovery;
    EXPECT_TRUE(recovery.needsRecovery(jm));
}

TEST(RecoveryTest, AnalyzeEmptyJournal) {
    filesystem::JournalManager jm;
    filesystem::Recovery recovery;
    auto result = recovery.analyze(jm);
    EXPECT_TRUE(result.success);
}

TEST(RecoveryTest, AnalyzeCompleteJournal) {
    filesystem::JournalManager jm;
    jm.beginCheckpoint();
    jm.logCreate(1, std::vector<uint8_t>{1, 2, 3});
    jm.commit();

    filesystem::Recovery recovery;
    auto result = recovery.analyze(jm);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.entriesReplayed, 0);
}

TEST(RecoveryTest, EstimateRecoveryTime) {
    filesystem::JournalManager jm;
    filesystem::Recovery recovery;
    auto time = recovery.estimateRecoveryTime(jm);
    EXPECT_EQ(time, std::chrono::seconds(0));

    jm.logCreate(1, std::span<const uint8_t>());
    time = recovery.estimateRecoveryTime(jm);
    EXPECT_GE(time, std::chrono::seconds(0));
}

TEST(RecoveryTest, RecoverAutomatic) {
    filesystem::JournalManager jm;
    jm.beginCheckpoint();
    jm.logCreate(42, std::vector<uint8_t>{1, 2, 3});
    jm.commit();

    filesystem::RecoveryConfig config;
    config.strategy = filesystem::RecoveryStrategy::Automatic;

    filesystem::Recovery recovery(config);
    size_t replayed = 0;
    auto result = recovery.recover(jm,
        [&](const JournalEntry&) -> std::error_code {
            ++replayed;
            return std::error_code();
        });

    EXPECT_TRUE(result.success);
}

TEST(RecoveryTest, RecoverBestEffort) {
    filesystem::JournalManager jm;
    jm.beginCheckpoint();
    jm.logCreate(1, std::span<const uint8_t>());

    filesystem::RecoveryConfig config;
    config.strategy = filesystem::RecoveryStrategy::BestEffort;

    filesystem::Recovery recovery(config);
    auto result = recovery.recover(jm,
        [&](const JournalEntry&) -> std::error_code {
            return std::error_code();
        });

    EXPECT_TRUE(result.success);
}

TEST(RecoveryTest, ManualStrategy) {
    filesystem::JournalManager jm;
    jm.beginCheckpoint();
    jm.logCreate(1, std::span<const uint8_t>());

    filesystem::RecoveryConfig config;
    config.strategy = filesystem::RecoveryStrategy::Manual;

    filesystem::Recovery recovery(config);
    auto result = recovery.recover(jm,
        [&](const JournalEntry&) -> std::error_code {
            return std::error_code();
        });

    EXPECT_TRUE(result.success);
}

} // namespace test
} // namespace nebula
