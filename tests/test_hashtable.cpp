#include <gtest/gtest.h>
#include "nebula/index/HashTable.hpp"

namespace nebula {
namespace test {

TEST(HashTableTest, InsertAndFind) {
    index::HashTable ht;
    ht.insert("key1", 100);
    ht.insert("key2", 200);

    auto result = ht.find("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 100);

    result = ht.find("key2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 200);
}

TEST(HashTableTest, FindNonExistent) {
    index::HashTable ht;
    auto result = ht.find("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST(HashTableTest, Contains) {
    index::HashTable ht;
    ht.insert("test", 42);
    EXPECT_TRUE(ht.contains("test"));
    EXPECT_FALSE(ht.contains("missing"));
}

TEST(HashTableTest, Remove) {
    index::HashTable ht;
    ht.insert("key", 100);
    EXPECT_TRUE(ht.contains("key"));

    EXPECT_TRUE(ht.remove("key"));
    EXPECT_FALSE(ht.contains("key"));
    EXPECT_FALSE(ht.remove("key"));
}

TEST(HashTableTest, UpdateExisting) {
    index::HashTable ht;
    ht.insert("key", 100);
    ht.insert("key", 200);

    auto result = ht.find("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 200);
}

TEST(HashTableTest, CollisionHandling) {
    index::HashTable ht(16);
    for (int i = 0; i < 100; ++i) {
        ht.insert("key" + std::to_string(i), i);
    }

    for (int i = 0; i < 100; ++i) {
        auto result = ht.find("key" + std::to_string(i));
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, i);
    }
}

TEST(HashTableTest, LoadFactor) {
    index::HashTable ht(16);
    EXPECT_EQ(ht.loadFactor(), 0.0);

    for (int i = 0; i < 8; ++i) {
        ht.insert("key" + std::to_string(i), i);
    }
    EXPECT_GT(ht.loadFactor(), 0.0);
    EXPECT_LT(ht.loadFactor(), 1.0);
}

TEST(HashTableTest, Clear) {
    index::HashTable ht;
    for (int i = 0; i < 10; ++i) {
        ht.insert("key" + std::to_string(i), i);
    }
    EXPECT_EQ(ht.size(), 10);

    ht.clear();
    EXPECT_EQ(ht.size(), 0);
    EXPECT_FALSE(ht.contains("key0"));
}

TEST(HashTableTest, Reserve) {
    index::HashTable ht;
    ht.reserve(1000);
    EXPECT_GE(ht.capacity(), 1000);
}

TEST(HashTableTest, SerializeDeserialize) {
    index::HashTable ht(64);
    for (int i = 0; i < 20; ++i) {
        ht.insert("path/" + std::to_string(i) + ".txt", i);
    }

    auto data = ht.serialize();
    EXPECT_FALSE(data.empty());

    index::HashTable parsed;
    auto ec = parsed.deserialize(data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(parsed.size(), 20);

    for (int i = 0; i < 20; ++i) {
        auto result = parsed.find("path/" + std::to_string(i) + ".txt");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, i);
    }
}

TEST(HashTableTest, DeserializeCorrupt) {
    index::HashTable ht;
    std::vector<uint8_t> corrupt = {0xFF, 0xFF};
    auto ec = ht.deserialize(corrupt);
    EXPECT_TRUE(ec);
}

TEST(HashTableTest, LargeCapacity) {
    index::HashTable ht(65536);
    EXPECT_EQ(ht.capacity(), 65536);
    EXPECT_EQ(ht.size(), 0);
}

TEST(HashTableTest, Iterator) {
    index::HashTable ht;
    for (int i = 0; i < 10; ++i) {
        ht.insert("key" + std::to_string(i), i);
    }

    size_t count = 0;
    for (auto it = ht.begin(); it != ht.end(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 10);
}

} // namespace test
} // namespace nebula
