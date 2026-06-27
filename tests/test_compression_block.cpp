#include <gtest/gtest.h>
#include "nebula/compression/CompressionBlock.hpp"

namespace nebula {
namespace test {

TEST(CompressionBlockTest, DefaultBlock) {
    compression::CompressionBlock block;
    EXPECT_EQ(block.offset, 0);
    EXPECT_EQ(block.compressedSize, 0);
    EXPECT_EQ(block.originalSize, 0);
    EXPECT_FALSE(block.encrypted);
    EXPECT_FALSE(block.isDecompressed);
}

TEST(CompressionBlockTest, NoCompressionDecompress) {
    compression::CompressionBlock block;
    block.algo = CompressionAlgorithm::None;
    block.originalSize = 5;
    block.compressedData = {1, 2, 3, 4, 5};
    block.compressedSize = 5;

    auto data = block.getDecompressedData();
    EXPECT_EQ(data.size(), 5);
    EXPECT_TRUE(block.isDecompressed);
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[4], 5);
}

TEST(CompressionBlockTest, HeaderSize) {
    compression::CompressionBlock block;
    EXPECT_EQ(block.headerSize(), 25);
}

TEST(CompressionBlockTest, VerifyIntegrityNoChecksum) {
    compression::CompressionBlock block;
    block.algo = CompressionAlgorithm::None;
    block.originalSize = 3;
    block.compressedData = {1, 2, 3};
    block.checksum = 0;
    EXPECT_TRUE(block.verifyIntegrity());
}

TEST(CompressionBlockTest, BlockCollection) {
    compression::BlockCollection collection;
    EXPECT_EQ(collection.blockCount(), 0);

    compression::CompressionBlock block;
    block.originalSize = 100;
    block.compressedSize = 50;
    collection.addBlock(std::move(block));

    EXPECT_EQ(collection.blockCount(), 1);

    auto* retrieved = collection.getBlock(0);
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->originalSize, 100);

    EXPECT_EQ(collection.getBlock(5), nullptr);
}

TEST(CompressionBlockTest, BlockCollectionTotalSizes) {
    compression::BlockCollection collection;
    compression::CompressionBlock b1, b2;
    b1.originalSize = 100; b1.compressedSize = 50;
    b2.originalSize = 200; b2.compressedSize = 80;

    collection.addBlock(std::move(b1));
    collection.addBlock(std::move(b2));

    EXPECT_EQ(collection.totalOriginalSize(), 300);
    EXPECT_EQ(collection.totalCompressedSize(), 130);
}

TEST(CompressionBlockTest, BlockCollectionFindBlock) {
    compression::BlockCollection collection;
    compression::CompressionBlock b1, b2;
    b1.originalSize = 100;
    b2.originalSize = 100;

    collection.addBlock(std::move(b1));
    collection.addBlock(std::move(b2));

    EXPECT_NE(collection.findBlock(50), nullptr);
    EXPECT_NE(collection.findBlock(150), nullptr);
    EXPECT_EQ(collection.findBlock(300), nullptr);
}

TEST(CompressionBlockTest, BlockCollectionClear) {
    compression::BlockCollection collection;
    collection.addBlock(compression::CompressionBlock{});
    collection.addBlock(compression::CompressionBlock{});
    EXPECT_EQ(collection.blockCount(), 2);

    collection.clear();
    EXPECT_EQ(collection.blockCount(), 0);
}

TEST(CompressionBlockTest, BlockCollectionReserve) {
    compression::BlockCollection collection;
    collection.reserve(100);
    EXPECT_EQ(collection.blockCount(), 0);
}

TEST(CompressionBlockTest, SyncWord) {
    EXPECT_EQ(compression::CompressionBlock::kSyncWord, 0x424E);
}

} // namespace test
} // namespace nebula
