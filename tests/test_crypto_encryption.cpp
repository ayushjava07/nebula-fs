#include <gtest/gtest.h>
#include "nebula/crypto/EncryptionEngine.hpp"

namespace nebula {
namespace test {

TEST(EncryptionTest, GenerateKey) {
    auto key = crypto::EncryptionEngine::generateKey();
    bool nonZero = false;
    for (auto byte : key) {
        if (byte != 0) { nonZero = true; break; }
    }
    EXPECT_TRUE(nonZero);
}

TEST(EncryptionTest, GenerateIV) {
    auto iv1 = crypto::EncryptionEngine::generateIV();
    auto iv2 = crypto::EncryptionEngine::generateIV();
    EXPECT_NE(iv1, iv2);
}

TEST(EncryptionTest, GenerateSalt) {
    auto salt = crypto::EncryptionEngine::generateSalt(16);
    EXPECT_EQ(salt.size(), 16);
}

TEST(EncryptionTest, AESEncryptDecrypt) {
    auto key = crypto::EncryptionEngine::generateKey();
    crypto::EncryptionConfig config;
    config.algorithm = EncryptionAlgorithm::AES256GCM;
    config.key = key;

    crypto::EncryptionEngine engine(config);
    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto encResult = engine.encrypt(plaintext);
    EXPECT_TRUE(encResult.success);
    EXPECT_FALSE(encResult.data.empty());

    auto decResult = engine.decrypt(encResult.data, encResult.iv, encResult.tag);
    EXPECT_TRUE(decResult.success);
    EXPECT_EQ(decResult.data, plaintext);
}

TEST(EncryptionTest, AESLargeData) {
    auto key = crypto::EncryptionEngine::generateKey();
    crypto::EncryptionConfig config;
    config.algorithm = EncryptionAlgorithm::AES256GCM;
    config.key = key;

    crypto::EncryptionEngine engine(config);
    std::vector<uint8_t> plaintext(65536);
    for (size_t i = 0; i < plaintext.size(); ++i) {
        plaintext[i] = static_cast<uint8_t>(i & 0xFF);
    }

    auto encResult = engine.encrypt(plaintext);
    EXPECT_TRUE(encResult.success);

    auto decResult = engine.decrypt(encResult.data, encResult.iv, encResult.tag);
    EXPECT_TRUE(decResult.success);
    EXPECT_EQ(decResult.data, plaintext);
}

TEST(EncryptionTest, WrongKeyDecryptFails) {
    auto key1 = crypto::EncryptionEngine::generateKey();
    auto key2 = crypto::EncryptionEngine::generateKey();

    crypto::EncryptionConfig config1;
    config1.algorithm = EncryptionAlgorithm::AES256GCM;
    config1.key = key1;

    crypto::EncryptionEngine engine1(config1);
    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5};
    auto encResult = engine1.encrypt(plaintext);
    EXPECT_TRUE(encResult.success);

    crypto::EncryptionConfig config2;
    config2.algorithm = EncryptionAlgorithm::AES256GCM;
    config2.key = key2;

    crypto::EncryptionEngine engine2(config2);
    auto decResult = engine2.decrypt(encResult.data, encResult.iv, encResult.tag);
    EXPECT_FALSE(decResult.success);
}

TEST(EncryptionTest, NoEncryptionMode) {
    crypto::EncryptionConfig config;
    config.algorithm = EncryptionAlgorithm::None;

    crypto::EncryptionEngine engine(config);
    std::vector<uint8_t> data = {1, 2, 3};
    auto result = engine.encrypt(data);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data, data);
}

TEST(EncryptionTest, KeyDerivation) {
    auto salt = crypto::EncryptionEngine::generateSalt();
    auto key1 = crypto::EncryptionEngine::deriveKey("password", salt, 1000);
    auto key2 = crypto::EncryptionEngine::deriveKey("password", salt, 1000);
    EXPECT_EQ(key1, key2);

    auto key3 = crypto::EncryptionEngine::deriveKey("different", salt, 1000);
    EXPECT_NE(key1, key3);
}

TEST(EncryptionTest, SetKey) {
    auto key = crypto::EncryptionEngine::generateKey();
    crypto::EncryptionEngine engine;
    engine.setKey(key);
    EXPECT_TRUE(engine.isReady());

    std::vector<uint8_t> data = {1, 2, 3};
    auto result = engine.encrypt(data);
    EXPECT_TRUE(result.success);
}

TEST(EncryptionTest, SetPassword) {
    auto salt = crypto::EncryptionEngine::generateSalt();
    crypto::EncryptionEngine engine;
    engine.setPassword("test_password", salt);
    EXPECT_TRUE(engine.isReady());

    std::vector<uint8_t> data = {1, 2, 3};
    auto result = engine.encrypt(data);
    EXPECT_TRUE(result.success);
}

TEST(EncryptionTest, MoveConstructor) {
    auto key = crypto::EncryptionEngine::generateKey();
    crypto::EncryptionConfig config;
    config.algorithm = EncryptionAlgorithm::AES256GCM;
    config.key = key;

    crypto::EncryptionEngine engine1(config);
    crypto::EncryptionEngine engine2(std::move(engine1));

    std::vector<uint8_t> data = {1, 2, 3};
    auto result = engine2.encrypt(data);
    EXPECT_TRUE(result.success);
}

TEST(EncryptionTest, DeterministicWithSameIV) {
    auto key = crypto::EncryptionEngine::generateKey();
    crypto::EncryptionConfig config;
    config.algorithm = EncryptionAlgorithm::AES256GCM;
    config.key = key;

    crypto::EncryptionEngine engine(config);
    std::vector<uint8_t> data = {1, 2, 3};

    auto iv = crypto::EncryptionEngine::generateIV();
    auto r1 = engine.encrypt(data, iv);
    auto r2 = engine.encrypt(data, iv);

    EXPECT_EQ(r1.data, r2.data);
}

TEST(EncryptionTest, EncryptEmptyData) {
    auto key = crypto::EncryptionEngine::generateKey();
    crypto::EncryptionConfig config;
    config.algorithm = EncryptionAlgorithm::AES256GCM;
    config.key = key;

    crypto::EncryptionEngine engine(config);
    auto result = engine.encrypt({});
    EXPECT_TRUE(result.success);
}

} // namespace test
} // namespace nebula
