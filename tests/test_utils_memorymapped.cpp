#include <gtest/gtest.h>
#include "nebula/utils/MemoryMappedFile.hpp"
#include <fstream>
#include <filesystem>

namespace nebula {
namespace test {

class MemoryMappedTest : public ::testing::Test {
protected:
    std::string testFilePath;

    void SetUp() override {
        testFilePath = "/tmp/nebula_mmap_test_" + std::to_string(::getpid()) + ".bin";
        std::ofstream file(testFilePath, std::ios::binary);
        for (int i = 0; i < 1000; ++i) {
            file.put(static_cast<char>(i & 0xFF));
        }
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(testFilePath, ec);
    }
};

TEST_F(MemoryMappedTest, OpenAndClose) {
    utils::MemoryMappedFile mmap;
    auto ec = mmap.open(testFilePath);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(mmap.isOpen());
    EXPECT_EQ(mmap.size(), 1000);

    mmap.close();
    EXPECT_FALSE(mmap.isOpen());
}

TEST_F(MemoryMappedTest, OpenNonExistent) {
    utils::MemoryMappedFile mmap;
    auto ec = mmap.open("/nonexistent/file.bin");
    EXPECT_TRUE(ec);
    EXPECT_FALSE(mmap.isOpen());
}

TEST_F(MemoryMappedTest, SpanAccess) {
    utils::MemoryMappedFile mmap(testFilePath);
    ASSERT_TRUE(mmap.isOpen());

    auto span = mmap.span();
    EXPECT_EQ(span.size(), 1000);
    EXPECT_EQ(span[0], 0);
    EXPECT_EQ(span[255], 255);
}

TEST_F(MemoryMappedTest, DataPointer) {
    utils::MemoryMappedFile mmap(testFilePath);
    ASSERT_TRUE(mmap.isOpen());

    auto data = mmap.data();
    EXPECT_NE(data, nullptr);
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[999], static_cast<uint8_t>(999 % 256));
}

TEST_F(MemoryMappedTest, Path) {
    utils::MemoryMappedFile mmap(testFilePath);
    EXPECT_EQ(mmap.path(), testFilePath);
}

TEST_F(MemoryMappedTest, IsValid) {
    utils::MemoryMappedFile mmap;
    EXPECT_FALSE(mmap.isValid());

    mmap.open(testFilePath);
    EXPECT_TRUE(mmap.isValid());
}

TEST_F(MemoryMappedTest, Advise) {
    utils::MemoryMappedFile mmap(testFilePath);
    ASSERT_TRUE(mmap.isOpen());

    EXPECT_NO_THROW(
        mmap.advise(utils::MemoryMappedFile::AccessPattern::Sequential)
    );
    EXPECT_NO_THROW(
        mmap.advise(utils::MemoryMappedFile::AccessPattern::Random)
    );
    EXPECT_NO_THROW(
        mmap.advise(utils::MemoryMappedFile::AccessPattern::WillNeed)
    );
}

TEST_F(MemoryMappedTest, PageSize) {
    auto ps = utils::MemoryMappedFile::pageSize();
    EXPECT_GT(ps, 0);
    EXPECT_EQ(ps % 1024, 0);
}

TEST_F(MemoryMappedTest, MoveConstructor) {
    utils::MemoryMappedFile mmap1(testFilePath);
    ASSERT_TRUE(mmap1.isOpen());

    utils::MemoryMappedFile mmap2(std::move(mmap1));
    EXPECT_TRUE(mmap2.isOpen());
    EXPECT_FALSE(mmap1.isOpen());
    EXPECT_EQ(mmap2.size(), 1000);
}

TEST_F(MemoryMappedTest, Remap) {
    utils::MemoryMappedFile mmap(testFilePath);
    ASSERT_TRUE(mmap.isOpen());

    auto ec = mmap.remap(0, 100);
    EXPECT_FALSE(ec);
    EXPECT_EQ(mmap.size(), 100);
}

TEST_F(MemoryMappedTest, RemapOutOfRange) {
    utils::MemoryMappedFile mmap(testFilePath);
    ASSERT_TRUE(mmap.isOpen());

    auto ec = mmap.remap(0, 10000);
    EXPECT_TRUE(ec);
}

} // namespace test
} // namespace nebula
