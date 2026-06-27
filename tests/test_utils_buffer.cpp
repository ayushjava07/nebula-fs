#include <gtest/gtest.h>
#include "nebula/utils/Buffer.hpp"

namespace nebula {
namespace test {

TEST(BufferTest, DefaultConstructor) {
    utils::Buffer buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0);
}

TEST(BufferTest, ConstructorWithCapacity) {
    utils::Buffer buf(1024);
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, ConstructorWithData) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    utils::Buffer buf(data, 5);
    EXPECT_EQ(buf.size(), 5);
}

TEST(BufferTest, ConstructorWithSpan) {
    std::vector<uint8_t> vec = {10, 20, 30};
    utils::Buffer buf{std::span<const uint8_t>(vec)};
    EXPECT_EQ(buf.size(), 3);
}

TEST(BufferTest, WriteAndReadByte) {
    utils::Buffer buf;
    buf.write(42);
    EXPECT_EQ(buf.size(), 1);
    EXPECT_EQ(buf.readByte(0), 42);
}

TEST(BufferTest, WriteAndReadData) {
    utils::Buffer buf;
    uint8_t data[] = {1, 2, 3, 4, 5};
    buf.write(data, 5);
    EXPECT_EQ(buf.size(), 5);
    uint8_t readData[5] = {};
    buf.read(0, readData, 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(readData[i], data[i]);
    }
}

TEST(BufferTest, WriteSpan) {
    utils::Buffer buf;
    std::vector<uint8_t> data = {100, 200, 255};
    buf.write(std::span<const uint8_t>(data));
    EXPECT_EQ(buf.size(), 3);
}

TEST(BufferTest, WriteAt) {
    utils::Buffer buf(16);
    uint8_t data[] = {1, 2, 3};
    buf.write(data, 3);
    uint8_t newData[] = {10, 20};
    buf.writeAt(1, newData, 2);
    EXPECT_EQ(buf.readByte(0), 1);
    EXPECT_EQ(buf.readByte(1), 10);
    EXPECT_EQ(buf.readByte(2), 20);
}

TEST(BufferTest, ReadOutOfRange) {
    utils::Buffer buf;
    EXPECT_THROW(buf.readByte(0), std::out_of_range);
}

TEST(BufferTest, WriteLE) {
    utils::Buffer buf;
    buf.writeLE<uint32_t>(0x12345678);
    EXPECT_EQ(buf.size(), 4);
    EXPECT_EQ(buf.readByte(0), 0x78);
    EXPECT_EQ(buf.readByte(1), 0x56);
    EXPECT_EQ(buf.readByte(2), 0x34);
    EXPECT_EQ(buf.readByte(3), 0x12);
}

TEST(BufferTest, ReadLE) {
    utils::Buffer buf;
    buf.writeLE<uint32_t>(0xAABBCCDD);
    auto val = buf.readLE<uint32_t>(0);
    EXPECT_EQ(val, 0xAABBCCDD);
}

TEST(BufferTest, WriteVarInt) {
    utils::Buffer buf;
    buf.writeVarInt(0);
    EXPECT_EQ(buf.size(), 1);
    size_t adv = 0;
    EXPECT_EQ(buf.readVarInt(0, adv), 0);
    EXPECT_EQ(adv, 1);
}

TEST(BufferTest, WriteVarIntLarge) {
    utils::Buffer buf;
    buf.writeVarInt(0xFFFFFFFFFFFFFFFFULL);
    EXPECT_GT(buf.size(), 1);
    size_t adv = 0;
    EXPECT_EQ(buf.readVarInt(0, adv), 0xFFFFFFFFFFFFFFFFULL);
}

TEST(BufferTest, View) {
    utils::Buffer buf;
    uint8_t data[] = {1, 2, 3, 4, 5};
    buf.write(data, 5);
    auto v = buf.view(1, 3);
    EXPECT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], 2);
    EXPECT_EQ(v[2], 4);
}

TEST(BufferTest, Clear) {
    utils::Buffer buf;
    buf.write(1);
    buf.write(2);
    buf.clear();
    EXPECT_TRUE(buf.empty());
}

TEST(BufferTest, Reserve) {
    utils::Buffer buf;
    buf.reserve(1000);
    EXPECT_GE(buf.size(), 0);
}

TEST(BufferTest, Slice) {
    utils::Buffer buf;
    uint8_t data[] = {1, 2, 3, 4, 5};
    buf.write(data, 5);
    auto sliced = buf.slice(1, 3);
    EXPECT_EQ(sliced.size(), 3);
    EXPECT_EQ(sliced.readByte(0), 2);
}

TEST(BufferTest, Append) {
    utils::Buffer buf1, buf2;
    buf1.write(uint8_t{1});
    buf2.write(uint8_t{2});
    buf1.append(buf2);
    EXPECT_EQ(buf1.size(), 2);
    EXPECT_EQ(buf1.readByte(1), 2);
}

TEST(BufferTest, Equality) {
    utils::Buffer buf1, buf2;
    buf1.write(uint8_t{1});
    buf2.write(uint8_t{1});
    EXPECT_TRUE(buf1 == buf2);
    buf2.write(uint8_t{2});
    EXPECT_FALSE(buf1 == buf2);
}

TEST(BufferTest, ToHexString) {
    utils::Buffer buf;
    buf.write(uint8_t{0xAB});
    buf.write(uint8_t{0xCD});
    EXPECT_EQ(buf.toHexString(), "abcd");
}

TEST(BufferTest, ToString) {
    utils::Buffer buf;
    std::string text = "hello";
    buf.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    EXPECT_EQ(buf.toString(), "hello");
}

TEST(BufferTest, MoveConstructor) {
    utils::Buffer buf1;
    buf1.write(uint8_t{42});
    utils::Buffer buf2(std::move(buf1));
    EXPECT_EQ(buf2.readByte(0), 42);
    EXPECT_TRUE(buf1.empty());
}

TEST(BufferTest, CopyConstructor) {
    utils::Buffer buf1;
    buf1.write(uint8_t{99});
    utils::Buffer buf2(buf1);
    EXPECT_EQ(buf2.readByte(0), 99);
    EXPECT_EQ(buf1.readByte(0), 99);
}

TEST(BufferTest, Swap) {
    utils::Buffer buf1, buf2;
    buf1.write(uint8_t{1});
    buf2.write(uint8_t{2});
    buf1.swap(buf2);
    EXPECT_EQ(buf1.readByte(0), 2);
    EXPECT_EQ(buf2.readByte(0), 1);
}

TEST(BufferTest, ShrinkToFit) {
    utils::Buffer buf(10000);
    buf.write(uint8_t{1});
    buf.shrinkToFit();
}

} // namespace test
} // namespace nebula
