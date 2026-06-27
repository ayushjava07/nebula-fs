#include <gtest/gtest.h>
#include "nebula/crypto/HashEngine.hpp"

namespace nebula {
namespace test {

TEST(HashEngineTest, SHA256Basic) {
    crypto::HashEngine engine;
    engine.update(std::vector<uint8_t>{1, 2, 3});
    auto hash = engine.finalize();
    EXPECT_EQ(hash.size(), 32);
}

TEST(HashEngineTest, SHA256Deterministic) {
    auto h1 = crypto::HashEngine::hash(std::vector<uint8_t>{1, 2, 3}, HashAlgorithm::SHA256);
    auto h2 = crypto::HashEngine::hash(std::vector<uint8_t>{1, 2, 3}, HashAlgorithm::SHA256);
    EXPECT_EQ(h1, h2);
}

TEST(HashEngineTest, SHA256Different) {
    auto h1 = crypto::HashEngine::hash(std::vector<uint8_t>{1, 2, 3}, HashAlgorithm::SHA256);
    auto h2 = crypto::HashEngine::hash(std::vector<uint8_t>{4, 5, 6}, HashAlgorithm::SHA256);
    EXPECT_NE(h1, h2);
}

TEST(HashEngineTest, Reset) {
    crypto::HashEngine engine;
    engine.update(std::vector<uint8_t>{1, 2, 3});
    auto h1 = engine.finalize();

    engine.reset();
    engine.update(std::vector<uint8_t>{1, 2, 3});
    auto h2 = engine.finalize();
    EXPECT_EQ(h1, h2);
}

TEST(HashEngineTest, Verify) {
    auto data = std::vector<uint8_t>{10, 20, 30};
    auto hash = crypto::HashEngine::hash(data);
    EXPECT_TRUE(crypto::HashEngine::verify(data, hash));

    data[0] = 0;
    EXPECT_FALSE(crypto::HashEngine::verify(data, hash));
}

TEST(HashEngineTest, OutputSize) {
    crypto::HashEngine sha256(HashAlgorithm::SHA256);
    EXPECT_EQ(sha256.outputSize(), 32);
}

TEST(HashEngineTest, MoveConstructor) {
    crypto::HashEngine engine1;
    engine1.update(std::vector<uint8_t>{1, 2, 3});
    auto h1 = engine1.finalize();

    crypto::HashEngine engine2(HashAlgorithm::SHA256);
    engine2.update(std::vector<uint8_t>{1, 2, 3});
    auto h2 = engine2.finalize();
    EXPECT_EQ(h1, h2);
}

TEST(HashEngineTest, MultiBufferHash) {
    std::vector<uint8_t> a = {1, 2, 3};
    std::vector<uint8_t> b = {4, 5, 6};
    std::array<std::span<const uint8_t>, 2> bufs = {a, b};

    auto combined = crypto::HashEngine::hash(bufs);

    crypto::HashEngine engine;
    engine.update(a);
    engine.update(b);
    EXPECT_EQ(combined, engine.finalize());
}

TEST(HashEngineTest, HashFileNotFound) {
    auto hash = crypto::HashEngine::hashFile("/nonexistent/file");
    EXPECT_EQ(hash, ChecksumValue{});
}

TEST(HashEngineTest, AlgorithmProperty) {
    crypto::HashEngine engine(HashAlgorithm::SHA256);
    EXPECT_EQ(engine.algorithm(), HashAlgorithm::SHA256);
}

} // namespace test
} // namespace nebula
