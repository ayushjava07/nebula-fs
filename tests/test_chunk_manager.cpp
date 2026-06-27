#include <gtest/gtest.h>
#include "nebula/storage/ChunkManager.hpp"

namespace nebula {
namespace test {

TEST(ChunkManagerTest, EmptyChunk) {
    storage::ChunkManager mgr;
    auto descriptors = mgr.chunkData({});
    EXPECT_TRUE(descriptors.empty());
}

TEST(ChunkManagerTest, ChunkSmallData) {
    storage::ChunkManager mgr;
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto descriptors = mgr.chunkData(data);
    EXPECT_GE(descriptors.size(), 1);
}

TEST(ChunkManagerTest, DedupDetection) {
    storage::ChunkConfig config;
    config.dedupStrategy = DedupStrategy::Fixed;
    config.maxChunkSize = 1024;
    storage::ChunkManager mgr(config);

    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto d1 = mgr.findOrCreateChunk(data);
    auto d2 = mgr.findOrCreateChunk(data);
    EXPECT_EQ(d1.hash, d2.hash);
    EXPECT_EQ(mgr.uniqueChunkCount(), 1);
}

TEST(ChunkManagerTest, UniqueChunks) {
    storage::ChunkManager mgr;
    std::vector<uint8_t> data1 = {1, 2, 3};
    std::vector<uint8_t> data2 = {4, 5, 6};

    mgr.findOrCreateChunk(data1);
    mgr.findOrCreateChunk(data2);
    EXPECT_EQ(mgr.uniqueChunkCount(), 2);
}

TEST(ChunkManagerTest, HasChunk) {
    storage::ChunkManager mgr;
    std::vector<uint8_t> data = {1, 2, 3};
    auto desc = mgr.findOrCreateChunk(data);
    EXPECT_TRUE(mgr.hasChunk(desc.hash));
}

TEST(ChunkManagerTest, GetChunk) {
    storage::ChunkManager mgr;
    std::vector<uint8_t> data = {10, 20, 30};
    auto desc = mgr.findOrCreateChunk(data);

    auto result = mgr.getChunk(desc.hash);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->originalSize, data.size());
}

TEST(ChunkManagerTest, RegisterChunk) {
    storage::ChunkManager mgr;
    ChunkDescriptor desc;
    desc.originalSize = 100;
    desc.compressedSize = 50;
    desc.hash[0] = 0xAB;
    mgr.registerChunk(desc);

    EXPECT_TRUE(mgr.hasChunk(desc.hash));
    EXPECT_EQ(mgr.uniqueChunkCount(), 1);
}

TEST(ChunkManagerTest, DedupRatio) {
    storage::ChunkManager mgr;
    EXPECT_EQ(mgr.dedupRatio(), 1.0);

    std::vector<uint8_t> data = {1, 2, 3};
    mgr.findOrCreateChunk(data);
    EXPECT_EQ(mgr.dedupRatio(), 1.0);
}

TEST(ChunkManagerTest, Clear) {
    storage::ChunkManager mgr;
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        mgr.findOrCreateChunk(data);
    }
    EXPECT_EQ(mgr.uniqueChunkCount(), 10);
    mgr.clear();
    EXPECT_EQ(mgr.uniqueChunkCount(), 0);
}

TEST(ChunkManagerTest, SerializeDeserialize) {
    storage::ChunkManager mgr;
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1)};
        mgr.findOrCreateChunk(data);
    }

    auto data = mgr.serialize();
    EXPECT_FALSE(data.empty());

    storage::ChunkManager parsed;
    auto ec = parsed.deserialize(data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(parsed.uniqueChunkCount(), 5);
}

TEST(ChunkManagerTest, DeserializeCorrupt) {
    storage::ChunkManager mgr;
    std::vector<uint8_t> corrupt = {0xFF};
    auto ec = mgr.deserialize(corrupt);
    EXPECT_TRUE(ec);
}

TEST(ChunkManagerTest, ComputeHash) {
    storage::ChunkManager mgr;
    std::vector<uint8_t> data = {1, 2, 3};
    auto hash1 = mgr.computeHash(data);
    auto hash2 = mgr.computeHash(data);
    EXPECT_EQ(hash1, hash2);

    std::vector<uint8_t> data2 = {4, 5, 6};
    auto hash3 = mgr.computeHash(data2);
    EXPECT_NE(hash1, hash3);
}

TEST(ChunkManagerTest, ContentDefinedChunking) {
    storage::ChunkConfig config;
    config.dedupStrategy = DedupStrategy::Content;
    config.minChunkSize = 16;
    config.maxChunkSize = 4096;
    config.avgChunkSize = 128;
    storage::ChunkManager mgr(config);

    std::vector<uint8_t> data(10000, 0);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    auto descriptors = mgr.chunkData(data);
    EXPECT_GT(descriptors.size(), 1);
}

TEST(ChunkManagerTest, TotalDataSize) {
    storage::ChunkManager mgr;
    std::vector<uint8_t> d1(100, 1);
    std::vector<uint8_t> d2(200, 2);

    mgr.findOrCreateChunk(d1);
    mgr.findOrCreateChunk(d2);

    EXPECT_EQ(mgr.totalDataSize(), 300);
}

} // namespace test
} // namespace nebula
