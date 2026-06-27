#include <gtest/gtest.h>
#include "nebula/filesystem/JournalManager.hpp"

namespace nebula {
namespace test {

TEST(JournalManagerTest, BeginEndCheckpoint) {
    filesystem::JournalManager jm;
    EXPECT_FALSE(jm.hasUncommitted());

    auto ec = jm.beginCheckpoint();
    EXPECT_FALSE(ec);
    EXPECT_TRUE(jm.hasUncommitted());

    ec = jm.endCheckpoint();
    EXPECT_FALSE(ec);
    EXPECT_FALSE(jm.hasUncommitted());
}

TEST(JournalManagerTest, LogEntry) {
    filesystem::JournalManager jm;
    JournalEntry entry;
    entry.type = JournalEntryType::CreateEntry;
    entry.sequence = 1;
    entry.data = {1, 2, 3};

    auto ec = jm.log(entry);
    EXPECT_FALSE(ec);
    EXPECT_EQ(jm.entryCount(), 1);
}

TEST(JournalManagerTest, LogCreate) {
    filesystem::JournalManager jm;
    std::vector<uint8_t> data = {1, 2, 3, 4};
    auto ec = jm.logCreate(42, data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(jm.entryCount(), 1);
}

TEST(JournalManagerTest, LogUpdate) {
    filesystem::JournalManager jm;
    auto ec = jm.logUpdate(1, std::span<const uint8_t>());
    EXPECT_FALSE(ec);
}

TEST(JournalManagerTest, LogDelete) {
    filesystem::JournalManager jm;
    auto ec = jm.logDelete(1);
    EXPECT_FALSE(ec);
}

TEST(JournalManagerTest, Commit) {
    filesystem::JournalManager jm;
    jm.beginCheckpoint();
    jm.logCreate(1, std::span<const uint8_t>());
    auto ec = jm.commit();
    EXPECT_FALSE(ec);
    EXPECT_FALSE(jm.needsRecovery());
}

TEST(JournalManagerTest, Abort) {
    filesystem::JournalManager jm;
    jm.beginCheckpoint();
    jm.logCreate(1, std::span<const uint8_t>());
    auto ec = jm.abort();
    EXPECT_FALSE(ec);
    EXPECT_FALSE(jm.hasUncommitted());
}

TEST(JournalManagerTest, Clear) {
    filesystem::JournalManager jm;
    jm.logCreate(1, std::span<const uint8_t>());
    jm.logCreate(2, std::span<const uint8_t>());
    EXPECT_EQ(jm.entryCount(), 2);

    jm.clear();
    EXPECT_EQ(jm.entryCount(), 0);
    EXPECT_EQ(jm.currentSequence(), 0);
}

TEST(JournalManagerTest, SerializeDeserialize) {
    filesystem::JournalManager jm;
    jm.logCreate(1, std::vector<uint8_t>{10, 20, 30});
    jm.logCreate(2, std::vector<uint8_t>{40, 50});
    jm.commit();

    auto data = jm.serialize();
    EXPECT_FALSE(data.empty());

    filesystem::JournalManager parsed;
    auto ec = parsed.deserialize(data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(parsed.entryCount(), 3);
}

TEST(JournalManagerTest, VerifyIntegrity) {
    filesystem::JournalManager jm;
    jm.logCreate(1, std::vector<uint8_t>{1, 2, 3});
    EXPECT_TRUE(jm.verifyIntegrity());
}

TEST(JournalManagerTest, NeedsRecovery) {
    filesystem::JournalManager jm;
    EXPECT_FALSE(jm.needsRecovery());

    jm.beginCheckpoint();
    EXPECT_TRUE(jm.needsRecovery());
}

TEST(JournalManagerTest, SequenceNumbers) {
    filesystem::JournalManager jm;
    EXPECT_EQ(jm.currentSequence(), 0);

    jm.logCreate(1, std::span<const uint8_t>());
    EXPECT_EQ(jm.currentSequence(), 1);

    jm.logCreate(2, std::span<const uint8_t>());
    EXPECT_EQ(jm.currentSequence(), 2);
}

TEST(JournalManagerTest, ReplayEntry) {
    filesystem::JournalManager jm;
    JournalEntry entry;
    entry.type = JournalEntryType::Commit;
    entry.data = {};

    auto ec = jm.replayEntry(entry);
    EXPECT_FALSE(ec);
}

TEST(JournalManagerTest, Recover) {
    filesystem::JournalManager jm;
    jm.beginCheckpoint();
    jm.logCreate(1, std::span<const uint8_t>());

    EXPECT_TRUE(jm.needsRecovery());
    auto ec = jm.recover();
    EXPECT_FALSE(ec);
}

TEST(JournalManagerTest, Flush) {
    filesystem::JournalManager jm;
    auto ec = jm.flush();
    EXPECT_FALSE(ec);
}

} // namespace test
} // namespace nebula
