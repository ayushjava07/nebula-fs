#include <gtest/gtest.h>
#include "nebula/metadata/MetadataStore.hpp"

namespace nebula {
namespace test {

TEST(MetadataStoreTest, SetAndGet) {
    metadata::MetadataStore store;
    store.set("key1", "value1");
    store.set("key2", "value2");

    EXPECT_EQ(store.getString("key1"), "value1");
    EXPECT_EQ(store.getString("key2"), "value2");
}

TEST(MetadataStoreTest, GetNonExistent) {
    metadata::MetadataStore store;
    EXPECT_FALSE(store.getString("nonexistent").has_value());
}

TEST(MetadataStoreTest, Contains) {
    metadata::MetadataStore store;
    store.set("key", "value");
    EXPECT_TRUE(store.contains("key"));
    EXPECT_FALSE(store.contains("missing"));
}

TEST(MetadataStoreTest, Remove) {
    metadata::MetadataStore store;
    store.set("key", "value");
    EXPECT_TRUE(store.contains("key"));

    EXPECT_TRUE(store.remove("key"));
    EXPECT_FALSE(store.remove("key"));
}

TEST(MetadataStoreTest, Keys) {
    metadata::MetadataStore store;
    store.set("a", "1");
    store.set("b", "2");
    store.set("c", "3");

    auto keys = store.keys();
    EXPECT_EQ(keys.size(), 3);
}

TEST(MetadataStoreTest, Size) {
    metadata::MetadataStore store;
    EXPECT_TRUE(store.empty());
    EXPECT_EQ(store.size(), 0);

    store.set("key", "value");
    EXPECT_FALSE(store.empty());
    EXPECT_EQ(store.size(), 1);
}

TEST(MetadataStoreTest, Clear) {
    metadata::MetadataStore store;
    store.set("key1", "value1");
    store.set("key2", "value2");
    store.clear();

    EXPECT_TRUE(store.empty());
    EXPECT_EQ(store.size(), 0);
}

TEST(MetadataStoreTest, BinaryValues) {
    metadata::MetadataStore store;
    std::vector<uint8_t> binData = {0, 1, 2, 3, 4, 5};
    store.set("binary", binData);

    auto result = store.getBinary("binary");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, binData);
}

TEST(MetadataStoreTest, SerializeDeserialize) {
    metadata::MetadataStore store;
    store.set("name", "test_archive");
    store.set("version", "1.0.0");
    store.set("author", "NebulaFS");

    auto data = store.serialize();
    EXPECT_FALSE(data.empty());

    metadata::MetadataStore parsed;
    auto ec = parsed.deserialize(data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(parsed.size(), 3);
    EXPECT_EQ(parsed.getString("name"), "test_archive");
    EXPECT_EQ(parsed.getString("version"), "1.0.0");
    EXPECT_EQ(parsed.getString("author"), "NebulaFS");
}

TEST(MetadataStoreTest, Validate) {
    metadata::MetadataStore store;
    EXPECT_TRUE(store.validate());

    store.set("key", "value");
    EXPECT_TRUE(store.validate());
}

TEST(MetadataStoreTest, Merge) {
    metadata::MetadataStore store1;
    store1.set("a", "1");
    store1.set("b", "2");

    metadata::MetadataStore store2;
    store2.set("c", "3");

    store1.merge(store2);
    EXPECT_EQ(store1.size(), 3);
    EXPECT_TRUE(store1.contains("c"));
}

TEST(MetadataStoreTest, OverwriteExisting) {
    metadata::MetadataStore store;
    store.set("key", "old_value");
    store.set("key", "new_value");

    EXPECT_EQ(store.getString("key"), "new_value");
}

TEST(MetadataStoreTest, CopyConstructor) {
    metadata::MetadataStore store1;
    store1.set("key", "value");

    metadata::MetadataStore store2(store1);
    EXPECT_EQ(store2.getString("key"), "value");
}

TEST(MetadataStoreTest, MoveConstructor) {
    metadata::MetadataStore store1;
    store1.set("key", "value");

    metadata::MetadataStore store2(std::move(store1));
    EXPECT_EQ(store2.getString("key"), "value");
}

} // namespace test
} // namespace nebula
