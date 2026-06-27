#include <gtest/gtest.h>
#include "nebula/storage/BlockStorage.hpp"

namespace nebula {
namespace test {

TEST(BlockStorageTest, DeserializeBlockInvalid) {
    storage::BlockStorage bs;
    std::vector<uint8_t> invalidData = {0, 0, 0, 0};
    auto result = bs.deserializeBlock(invalidData, 0);
    EXPECT_TRUE(isError(result));
}

TEST(BlockStorageTest, VerifyBlockInvalid) {
    storage::BlockStorage bs;
    std::vector<uint8_t> badData = {0, 0, 0, 0};
    EXPECT_FALSE(bs.verifyBlock(badData));
}

TEST(BlockStorageTest, BlockAlignment) {
    EXPECT_EQ(storage::BlockStorage::blockAlignment(), 4);
}

TEST(BlockStorageTest, ReadBlockNoneAlgo) {
    storage::BlockStorage bs;
    ChunkDescriptor desc;
    desc.compression = CompressionAlgorithm::None;
    desc.originalSize = 10;

    std::vector<uint8_t> output;
    auto ec = bs.readBlock(desc, output);
    EXPECT_FALSE(ec);
    EXPECT_EQ(output.size(), 10);
}

TEST(BlockStorageTest, ReadRawBlockOutOfRange) {
    storage::BlockStorage bs;
    ChunkDescriptor desc;
    desc.offset = 1000;
    desc.compressedSize = 100;

    std::vector<uint8_t> section(500);
    auto data = bs.readRawBlock(section, desc);
    EXPECT_TRUE(data.empty());
}

} // namespace test
} // namespace nebula
