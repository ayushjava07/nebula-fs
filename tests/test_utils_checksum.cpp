#include <gtest/gtest.h>
#include "nebula/utils/Checksum.hpp"

namespace nebula {
namespace test {

TEST(ChecksumTest, CRC32Basic) {
    uint8_t data[] = "hello";
    auto crc = utils::ChecksumEngine::crc32(data, 5);
    EXPECT_NE(crc, 0);
}

TEST(ChecksumTest, CRC32Span) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto crc = utils::ChecksumEngine::crc32(std::span<const uint8_t>(data));
    EXPECT_NE(crc, 0);
}

TEST(ChecksumTest, CRC32Deterministic) {
    uint8_t data[] = "test data";
    auto crc1 = utils::ChecksumEngine::crc32(data, 9);
    auto crc2 = utils::ChecksumEngine::crc32(data, 9);
    EXPECT_EQ(crc1, crc2);
}

TEST(ChecksumTest, SHA256Basic) {
    std::vector<uint8_t> data = {1, 2, 3};
    auto hash = utils::ChecksumEngine::compute(data, HashAlgorithm::SHA256);
    EXPECT_EQ(hash.size(), 32);
    EXPECT_NE(hash, ChecksumValue{});
}

TEST(ChecksumTest, SHA256Deterministic) {
    std::vector<uint8_t> data = {1, 2, 3};
    auto hash1 = utils::ChecksumEngine::compute(data, HashAlgorithm::SHA256);
    auto hash2 = utils::ChecksumEngine::compute(data, HashAlgorithm::SHA256);
    EXPECT_EQ(hash1, hash2);
}

TEST(ChecksumTest, SHA256DifferentInputs) {
    std::vector<uint8_t> data1 = {1, 2, 3};
    std::vector<uint8_t> data2 = {4, 5, 6};
    auto hash1 = utils::ChecksumEngine::compute(data1, HashAlgorithm::SHA256);
    auto hash2 = utils::ChecksumEngine::compute(data2, HashAlgorithm::SHA256);
    EXPECT_NE(hash1, hash2);
}

TEST(ChecksumTest, VerifyHash) {
    std::vector<uint8_t> data = {10, 20, 30};
    auto hash = utils::ChecksumEngine::compute(data, HashAlgorithm::SHA256);
    EXPECT_TRUE(utils::ChecksumEngine::verify(data, hash, HashAlgorithm::SHA256));

    data[0] = 99;
    EXPECT_FALSE(utils::ChecksumEngine::verify(data, hash, HashAlgorithm::SHA256));
}

TEST(ChecksumTest, ToHex) {
    ChecksumValue hash;
    hash[0] = 0xAB;
    hash[1] = 0xCD;
    auto hex = utils::ChecksumEngine::toHex(hash);
    EXPECT_EQ(hex.size(), 64);
    EXPECT_EQ(hex.substr(0, 4), "abcd");
}

TEST(ChecksumTest, FromHex) {
    std::string hex(64, '0');
    hex[0] = 'a'; hex[1] = 'b'; hex[2] = 'c'; hex[3] = 'd';
    auto hash = utils::ChecksumEngine::fromHex(hex);
    EXPECT_EQ(hash[0], 0xAB);
    EXPECT_EQ(hash[1], 0xCD);
}

TEST(ChecksumTest, HexRoundTrip) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto hash = utils::ChecksumEngine::compute(data);
    auto hex = utils::ChecksumEngine::toHex(hash);
    auto parsed = utils::ChecksumEngine::fromHex(hex);
    EXPECT_EQ(hash, parsed);
}

TEST(ChecksumTest, StreamingAPI) {
    utils::ChecksumEngine engine;
    engine.update(std::vector<uint8_t>{1, 2, 3});
    engine.update(std::vector<uint8_t>{4, 5, 6});
    auto hash1 = engine.finalize();

    engine.reset();
    engine.update(std::vector<uint8_t>{1, 2, 3, 4, 5, 6});
    auto hash2 = engine.finalize();

    EXPECT_EQ(hash1, hash2);
}

TEST(ChecksumTest, MultiBufferHash) {
    std::vector<uint8_t> a = {1, 2, 3};
    std::vector<uint8_t> b = {4, 5, 6};
    std::array<std::span<const uint8_t>, 2> bufs = {a, b};
    auto hash = utils::ChecksumEngine::compute(bufs);

    utils::ChecksumEngine engine;
    engine.update(a);
    engine.update(b);
    EXPECT_EQ(hash, engine.finalize());
}

TEST(ChecksumTest, EmptyInput) {
    auto hash = utils::ChecksumEngine::compute(std::span<const uint8_t>(), HashAlgorithm::SHA256);
    EXPECT_NE(hash, ChecksumValue{});
    EXPECT_EQ(hash.size(), 32);
}

TEST(ChecksumTest, StringUpdate) {
    utils::ChecksumEngine engine;
    engine.update(std::string_view("hello"));
    auto hash = engine.finalize();
    EXPECT_NE(hash, ChecksumValue{});
}

TEST(ChecksumTest, CopyConstructor) {
    utils::ChecksumEngine engine1;
    engine1.update(std::vector<uint8_t>{1, 2, 3});

    utils::ChecksumEngine engine2(engine1);
    engine2.update(std::vector<uint8_t>{4, 5, 6});
    engine1.update(std::vector<uint8_t>{4, 5, 6});

    EXPECT_EQ(engine1.finalize(), engine2.finalize());
}

TEST(ChecksumTest, MoveConstructor) {
    utils::ChecksumEngine engine1;
    engine1.update(std::vector<uint8_t>{1, 2, 3});

    utils::ChecksumEngine engine2(std::move(engine1));
    engine2.update(std::vector<uint8_t>{4, 5, 6});

    EXPECT_NE(engine2.finalize(), ChecksumValue{});
}

TEST(ChecksumTest, Reset) {
    utils::ChecksumEngine engine;
    engine.update(std::vector<uint8_t>{1, 2, 3});
    auto hash1 = engine.finalize();

    engine.reset();
    engine.update(std::vector<uint8_t>{1, 2, 3});
    auto hash2 = engine.finalize();

    EXPECT_EQ(hash1, hash2);
}

TEST(ChecksumTest, AlgorithmChange) {
    utils::ChecksumEngine engine(HashAlgorithm::CRC32);
    EXPECT_EQ(engine.algorithm(), HashAlgorithm::CRC32);
}

TEST(ChecksumTest, HashSize) {
    utils::ChecksumEngine sha256(HashAlgorithm::SHA256);
    EXPECT_EQ(sha256.hashSize(), 32);
}

} // namespace test
} // namespace nebula
