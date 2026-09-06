#include <gtest/gtest.h>
#include "nebula/storage/backend/StorageBackend.hpp"
#include "nebula/storage/backend/MemoryStorageBackend.hpp"
#include "nebula/storage/backend/FileStorageBackend.hpp"
#include "nebula/storage/backend/TieredStorageManager.hpp"

#include <filesystem>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace nebula::storage::backend;

// --- MemoryStorageBackend Tests ---

TEST(MemoryStorageBackendTest, BasicWriteAndRead) {
    MemoryStorageBackend backend(1024 * 1024);
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};

    EXPECT_EQ(backend.writeBlock(100, data), StorageError::Success);
    EXPECT_TRUE(backend.hasBlock(100));

    auto res = backend.readBlock(100);
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(res));
    EXPECT_EQ(std::get<std::vector<uint8_t>>(res), data);

    auto spanRes = backend.readSpan(100, 2, 4);
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(spanRes));
    std::vector<uint8_t> expectedSpan = {3, 4, 5, 6};
    EXPECT_EQ(std::get<std::vector<uint8_t>>(spanRes), expectedSpan);

    EXPECT_EQ(backend.deleteBlock(100), StorageError::Success);
    EXPECT_FALSE(backend.hasBlock(100));
}

TEST(MemoryStorageBackendTest, CapacityExceeded) {
    MemoryStorageBackend backend(100); // 100 bytes capacity
    std::vector<uint8_t> bigData(150, 0xFF);

    EXPECT_EQ(backend.writeBlock(1, bigData), StorageError::CapacityExceeded);
    EXPECT_FALSE(backend.hasBlock(1));
}

TEST(MemoryStorageBackendTest, ConcurrentReadsAndWrites) {
    MemoryStorageBackend backend(10 * 1024 * 1024);
    constexpr size_t numThreads = 8;
    constexpr size_t opsPerThread = 50;

    std::vector<std::thread> threads;
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&backend, t]() {
            for (size_t i = 0; i < opsPerThread; ++i) {
                uint64_t blockId = t * 1000 + i;
                std::vector<uint8_t> payload = {static_cast<uint8_t>(t), static_cast<uint8_t>(i)};
                backend.writeBlock(blockId, payload);
                auto res = backend.readBlock(blockId);
                EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(res));
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(backend.stats().totalBlocks, numThreads * opsPerThread);
}

// --- FileStorageBackend Tests ---

TEST(FileStorageBackendTest, PersistenceAndReload) {
    fs::path testDir = fs::temp_directory_path() / "nebula_storage_test_file";
    fs::remove_all(testDir);

    {
        FileStorageBackend fileBackend(testDir);
        std::vector<uint8_t> data1 = {10, 20, 30, 40};
        std::vector<uint8_t> data2 = {50, 60, 70, 80};

        EXPECT_EQ(fileBackend.writeBlock(1, data1), StorageError::Success);
        EXPECT_EQ(fileBackend.writeBlock(2, data2), StorageError::Success);
        EXPECT_TRUE(fileBackend.hasBlock(1));
        EXPECT_TRUE(fileBackend.hasBlock(2));
    }

    // Reopen from disk in a fresh instance
    {
        FileStorageBackend reloaded(testDir);
        EXPECT_TRUE(reloaded.hasBlock(1));
        EXPECT_TRUE(reloaded.hasBlock(2));

        auto res1 = reloaded.readBlock(1);
        ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(res1));
        EXPECT_EQ(std::get<std::vector<uint8_t>>(res1), (std::vector<uint8_t>{10, 20, 30, 40}));

        EXPECT_EQ(reloaded.deleteBlock(1), StorageError::Success);
        EXPECT_FALSE(reloaded.hasBlock(1));
    }

    fs::remove_all(testDir);
}

// --- TieredStorageManager Tests ---

TEST(TieredStorageManagerTest, HotToColdSpillAndPromotion) {
    fs::path coldDir = fs::temp_directory_path() / "nebula_tiered_test";
    fs::remove_all(coldDir);

    auto hot = std::make_unique<MemoryStorageBackend>(1024 * 1024);
    auto cold = std::make_unique<FileStorageBackend>(coldDir);

    // Max hot blocks = 2
    TieredStorageManager tiered(std::move(hot), std::move(cold), 2);

    std::vector<uint8_t> b1 = {1, 1, 1};
    std::vector<uint8_t> b2 = {2, 2, 2};
    std::vector<uint8_t> b3 = {3, 3, 3};

    EXPECT_EQ(tiered.writeBlock(1, b1), StorageError::Success);
    EXPECT_EQ(tiered.writeBlock(2, b2), StorageError::Success);
    EXPECT_EQ(tiered.getBlockTier(1), TierLevel::HotMemory);
    EXPECT_EQ(tiered.getBlockTier(2), TierLevel::HotMemory);

    // Writing 3rd block exceeds maxHotBlocks(2), so oldest (block 1) spills to cold
    EXPECT_EQ(tiered.writeBlock(3, b3), StorageError::Success);
    EXPECT_EQ(tiered.getBlockTier(1), TierLevel::ColdDisk);
    EXPECT_EQ(tiered.getBlockTier(2), TierLevel::HotMemory);
    EXPECT_EQ(tiered.getBlockTier(3), TierLevel::HotMemory);
    EXPECT_EQ(tiered.stats().migrationsToCold, 1);

    // Reading block 1 promotes it back to hot tier
    auto res1 = tiered.readBlock(1, true);
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(res1));
    EXPECT_EQ(std::get<std::vector<uint8_t>>(res1), b1);
    EXPECT_EQ(tiered.getBlockTier(1), TierLevel::HotMemory);
    EXPECT_EQ(tiered.stats().migrationsToHot, 1);

    fs::remove_all(coldDir);
}
