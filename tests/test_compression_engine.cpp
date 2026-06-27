#include <gtest/gtest.h>
#include "nebula/compression/CompressionEngine.hpp"

namespace nebula {
namespace test {

class CompressionEngineTest : public ::testing::Test {
protected:
    std::vector<uint8_t> smallData = {1, 2, 3, 4, 5};
    std::vector<uint8_t> largeData;

    void SetUp() override {
        largeData.resize(100000);
        for (size_t i = 0; i < largeData.size(); ++i) {
            largeData[i] = static_cast<uint8_t>(i & 0xFF);
        }
    }
};

TEST_F(CompressionEngineTest, NoneCompression) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::None});
    auto result = engine.compress(smallData);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data.size(), smallData.size());
    EXPECT_EQ(result.originalSize, smallData.size());
}

TEST_F(CompressionEngineTest, LZ4CompressDecompress) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::LZ4});
    auto compResult = engine.compress(smallData);
    EXPECT_TRUE(compResult.success);
    EXPECT_GT(compResult.compressedSize, 0);

    auto decompResult = engine.decompress(compResult.data, smallData.size());
    EXPECT_TRUE(decompResult.success);
    EXPECT_EQ(decompResult.data.size(), smallData.size());
    for (size_t i = 0; i < smallData.size(); ++i) {
        EXPECT_EQ(decompResult.data[i], smallData[i]);
    }
}

TEST_F(CompressionEngineTest, ZlibCompressDecompress) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::Zlib});
    auto compResult = engine.compress(smallData);
    EXPECT_TRUE(compResult.success);

    auto decompResult = engine.decompress(compResult.data, smallData.size());
    EXPECT_TRUE(decompResult.success);
    EXPECT_EQ(decompResult.data, smallData);
}

TEST_F(CompressionEngineTest, ZstdCompressDecompress) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::Zstd, 3});
    auto compResult = engine.compress(smallData);
    EXPECT_TRUE(compResult.success);

    auto decompResult = engine.decompress(compResult.data, smallData.size());
    EXPECT_TRUE(decompResult.success);
    EXPECT_EQ(decompResult.data, smallData);
}

TEST_F(CompressionEngineTest, LargeDataRoundTrip) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::Zstd, 1});
    auto compResult = engine.compress(largeData);
    EXPECT_TRUE(compResult.success);
    EXPECT_LT(compResult.compressedSize, largeData.size());

    auto decompResult = engine.decompress(compResult.data, largeData.size());
    EXPECT_TRUE(decompResult.success);
    EXPECT_EQ(decompResult.data, largeData);
}

TEST_F(CompressionEngineTest, CompressBlocks) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::Zstd, 1});
    auto blocks = engine.compressBlocks(largeData, 16384);
    EXPECT_GT(blocks.size(), 1);
    for (const auto& block : blocks) {
        EXPECT_TRUE(block.success);
    }
}

TEST_F(CompressionEngineTest, DecompressBlocks) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::Zstd, 1});
    auto blocks = engine.compressBlocks(largeData, 16384);
    auto result = engine.decompressBlocks(blocks, largeData.size());
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data, largeData);
}

TEST_F(CompressionEngineTest, AlgorithmName) {
    compression::CompressionEngine none(
        compression::CompressionConfig{CompressionAlgorithm::None});
    EXPECT_EQ(none.algorithmName(), "none");

    compression::CompressionEngine lz4(
        compression::CompressionConfig{CompressionAlgorithm::LZ4});
    EXPECT_EQ(lz4.algorithmName(), "LZ4");
}

TEST_F(CompressionEngineTest, MaxCompressedSize) {
    auto maxSize = compression::CompressionEngine::maxCompressedSize(1024);
    EXPECT_GT(maxSize, 1024);
}

TEST_F(CompressionEngineTest, EstimateCompressedSize) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::Zstd});
    auto est = engine.estimateCompressedSize(1024);
    EXPECT_GT(est, 0);
}

TEST_F(CompressionEngineTest, Reconfigure) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::LZ4});
    EXPECT_EQ(engine.algorithm(), CompressionAlgorithm::LZ4);

    engine.setConfig(compression::CompressionConfig{CompressionAlgorithm::Zstd});
    EXPECT_EQ(engine.algorithm(), CompressionAlgorithm::Zstd);
}

TEST_F(CompressionEngineTest, MoveConstructor) {
    compression::CompressionEngine engine1(
        compression::CompressionConfig{CompressionAlgorithm::LZ4});
    compression::CompressionEngine engine2(std::move(engine1));
    auto result = engine2.compress(smallData);
    EXPECT_TRUE(result.success);
}

TEST_F(CompressionEngineTest, DecompressCorruptData) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::Zstd});
    std::vector<uint8_t> corruptData = {0, 1, 2, 3, 4, 5};
    auto result = engine.decompress(corruptData, 100);
    EXPECT_FALSE(result.success);
}

TEST_F(CompressionEngineTest, AlgorithmProperty) {
    compression::CompressionEngine engine(
        compression::CompressionConfig{CompressionAlgorithm::Zstd});
    EXPECT_TRUE(engine.isSupported());
    EXPECT_EQ(engine.algorithm(), CompressionAlgorithm::Zstd);
}

} // namespace test
} // namespace nebula
