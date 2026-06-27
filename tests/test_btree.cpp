#include <gtest/gtest.h>
#include "nebula/index/BTree.hpp"

namespace nebula {
namespace test {

using BTreeType = index::BTree<uint64_t, IndexEntry>;

TEST(BTreeTest, InsertAndFind) {
    BTreeType tree(4);
    IndexEntry entry{1, 100, 50, {}};
    tree.insert(1, entry);

    auto result = tree.find(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->entryId, 1);
    EXPECT_EQ(result->offset, 100);
    EXPECT_EQ(result->size, 50);
}

TEST(BTreeTest, FindNonExistent) {
    BTreeType tree;
    auto result = tree.find(42);
    EXPECT_FALSE(result.has_value());
}

TEST(BTreeTest, Contains) {
    BTreeType tree;
    tree.insert(1, {1, 0, 0, {}});
    EXPECT_TRUE(tree.contains(1));
    EXPECT_FALSE(tree.contains(2));
}

TEST(BTreeTest, MultipleInserts) {
    BTreeType tree(4);
    for (uint64_t i = 0; i < 100; ++i) {
        tree.insert(i, {i, i * 100, 64, {}});
    }

    for (uint64_t i = 0; i < 100; ++i) {
        auto result = tree.find(i);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->entryId, i);
        EXPECT_EQ(result->offset, i * 100);
    }
}

TEST(BTreeTest, Remove) {
    BTreeType tree(4);
    tree.insert(1, {1, 0, 0, {}});
    tree.insert(2, {2, 0, 0, {}});
    tree.insert(3, {3, 0, 0, {}});

    EXPECT_TRUE(tree.remove(2));
    EXPECT_FALSE(tree.contains(2));
    EXPECT_TRUE(tree.contains(1));
    EXPECT_TRUE(tree.contains(3));
    EXPECT_EQ(tree.size(), 2);
}

TEST(BTreeTest, RemoveNonExistent) {
    BTreeType tree;
    EXPECT_FALSE(tree.remove(42));
}

TEST(BTreeTest, Clear) {
    BTreeType tree(4);
    for (uint64_t i = 0; i < 50; ++i) {
        tree.insert(i, {i, 0, 0, {}});
    }
    EXPECT_EQ(tree.size(), 50);

    tree.clear();
    EXPECT_TRUE(tree.empty());
    EXPECT_EQ(tree.size(), 0);
}

TEST(BTreeTest, Size) {
    BTreeType tree;
    EXPECT_TRUE(tree.empty());
    EXPECT_EQ(tree.size(), 0);

    tree.insert(1, {1, 0, 0, {}});
    EXPECT_FALSE(tree.empty());
    EXPECT_EQ(tree.size(), 1);

    tree.insert(2, {2, 0, 0, {}});
    EXPECT_EQ(tree.size(), 2);
}

TEST(BTreeTest, Order4Tree) {
    BTreeType tree(4);
    for (uint64_t i = 0; i < 10; ++i) {
        tree.insert(i, {i, 0, 0, {}});
    }
    EXPECT_EQ(tree.size(), 10);
    for (uint64_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(tree.contains(i));
    }
}

TEST(BTreeTest, ReverseInsertOrder) {
    BTreeType tree(8);
    for (uint64_t i = 100; i > 0; --i) {
        tree.insert(i, {i, 0, 0, {}});
    }
    EXPECT_EQ(tree.size(), 100);
    for (uint64_t i = 1; i <= 100; ++i) {
        EXPECT_TRUE(tree.contains(i));
    }
}

TEST(BTreeTest, LargeTree) {
    BTreeType tree(128);
    constexpr size_t count = 10000;
    for (uint64_t i = 0; i < count; ++i) {
        tree.insert(i, {i, 0, 0, {}});
    }
    EXPECT_EQ(tree.size(), count);
    for (uint64_t i = 0; i < count; ++i) {
        EXPECT_TRUE(tree.contains(i));
    }
}

TEST(BTreeTest, DuplicateInsert) {
    BTreeType tree;
    tree.insert(1, {1, 100, 50, {}});
    tree.insert(1, {1, 200, 75, {}});

    auto result = tree.find(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(tree.size(), 2);
}

TEST(BTreeTest, Iterator) {
    BTreeType tree(4);
    for (uint64_t i = 0; i < 10; ++i) {
        tree.insert(i, {i, 0, 0, {}});
    }

    size_t count = 0;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 10);
}

TEST(BTreeTest, SerializeDeserialize) {
    BTreeType tree(8);
    for (uint64_t i = 0; i < 50; ++i) {
        tree.insert(i, {i, i * 100, 64, {}});
    }

    auto data = tree.serialize();
    EXPECT_FALSE(data.empty());

    BTreeType parsed;
    auto ec = parsed.deserialize(data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(parsed.size(), 50);

    for (uint64_t i = 0; i < 50; ++i) {
        auto result = parsed.find(i);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->offset, i * 100);
    }
}

} // namespace test
} // namespace nebula
