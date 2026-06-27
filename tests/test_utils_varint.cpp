#include <gtest/gtest.h>
#include "nebula/utils/VarInt.hpp"

namespace nebula {
namespace test {

TEST(VarIntTest, EncodeSingleByte) {
    uint8_t buf[10] = {};
    auto n = utils::VarInt::encode(0, buf, 10);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(buf[0], 0);
}

TEST(VarIntTest, EncodeSmallValue) {
    uint8_t buf[10] = {};
    auto n = utils::VarInt::encode(127, buf, 10);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(buf[0], 127);
}

TEST(VarIntTest, EncodeTwoBytes) {
    uint8_t buf[10] = {};
    auto n = utils::VarInt::encode(128, buf, 10);
    EXPECT_EQ(n, 2);
    EXPECT_EQ(buf[0], 0x80);
    EXPECT_EQ(buf[1], 0x01);
}

TEST(VarIntTest, EncodeLargeValue) {
    uint8_t buf[10] = {};
    auto n = utils::VarInt::encode(0xFFFFFFFFFFFFFFFFULL, buf, 10);
    EXPECT_EQ(n, 10);
    EXPECT_EQ(buf[9], 0x01);
}

TEST(VarIntTest, DecodeSingleByte) {
    uint8_t buf[] = {0};
    auto result = utils::VarInt::decode(buf, 1);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.value, 0);
    EXPECT_EQ(result.consumed, 1);
}

TEST(VarIntTest, DecodeSmallValue) {
    uint8_t buf[] = {127};
    auto result = utils::VarInt::decode(buf, 1);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.value, 127);
}

TEST(VarIntTest, DecodeTwoBytes) {
    uint8_t buf[] = {0x80, 0x01};
    auto result = utils::VarInt::decode(buf, 2);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.value, 128);
    EXPECT_EQ(result.consumed, 2);
}

TEST(VarIntTest, DecodeFromVector) {
    std::vector<uint8_t> buf = {0x80, 0x02};
    auto result = utils::VarInt::decode(std::span<const uint8_t>(buf));
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.value, 256);
}

TEST(VarIntTest, EncodeDecodeRoundTrip) {
    std::vector<uint64_t> testValues = {
        0, 1, 127, 128, 255, 256, 65535, 65536,
        0xFFFFFFFF, 0x100000000, 0x7FFFFFFFFFFFFFFF,
        0xFFFFFFFFFFFFFFFFULL
    };

    for (auto val : testValues) {
        uint8_t buf[10] = {};
        auto n = utils::VarInt::encode(val, buf, 10);
        EXPECT_GT(n, 0);
        auto result = utils::VarInt::decode(buf, n);
        EXPECT_TRUE(result.valid);
        EXPECT_EQ(result.value, val);
        EXPECT_EQ(result.consumed, n);
    }
}

TEST(VarIntTest, EncodedSize) {
    EXPECT_EQ(utils::VarInt::encodedSize(0), 1);
    EXPECT_EQ(utils::VarInt::encodedSize(127), 1);
    EXPECT_EQ(utils::VarInt::encodedSize(128), 2);
    EXPECT_EQ(utils::VarInt::encodedSize(16383), 2);
    EXPECT_EQ(utils::VarInt::encodedSize(16384), 3);
}

TEST(VarIntTest, EncodeToVector) {
    std::vector<uint8_t> vec;
    utils::VarInt::encode(42, vec);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0], 42);
}

TEST(VarIntTest, ZigZag) {
    EXPECT_EQ(utils::VarInt::zigzagEncode(0), 0);
    EXPECT_EQ(utils::VarInt::zigzagEncode(-1), 1);
    EXPECT_EQ(utils::VarInt::zigzagEncode(1), 2);
    EXPECT_EQ(utils::VarInt::zigzagEncode(-2), 3);

    EXPECT_EQ(utils::VarInt::zigzagDecode(0), 0);
    EXPECT_EQ(utils::VarInt::zigzagDecode(1), -1);
    EXPECT_EQ(utils::VarInt::zigzagDecode(2), 1);
    EXPECT_EQ(utils::VarInt::zigzagDecode(3), -2);
}

TEST(VarIntTest, EncodeDecodeSigned) {
    uint8_t buf[10] = {};
    auto n = utils::VarInt::encodeSigned(-42, buf, 10);
    auto result = utils::VarInt::decodeSigned(buf, n);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(static_cast<int64_t>(result.value), -42);
}

TEST(VarIntTest, DecodeInvalid) {
    uint8_t buf[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    auto result = utils::VarInt::decode(buf, 10);
    EXPECT_FALSE(result.valid);
}

TEST(VarIntTest, EncodeBufferTooSmall) {
    uint8_t buf[1] = {};
    auto n = utils::VarInt::encode(0xFFFFFFFF, buf, 1);
    EXPECT_EQ(n, 1);
}

TEST(VarIntTest, DecodeSpan) {
    std::array<uint8_t, 2> arr = {0x80, 0x01};
    auto result = utils::VarInt::decode(std::span<const uint8_t>(arr));
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.value, 128);
}

} // namespace test
} // namespace nebula
