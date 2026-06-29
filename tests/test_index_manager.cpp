#include <gtest/gtest.h>
#include "nebula/index/IndexManager.hpp"

namespace nebula {
namespace test {

TEST(IndexManagerTest, InsertAndFind) {
    index::IndexManager mgr;
    ArchiveEntry entry;
    entry.id = 1;
    entry.path = "/test.txt";
    entry.offset = 100;
    entry.storedSize = 50;

    mgr.insert(entry);
    EXPECT_EQ(mgr.size(), 1);
    EXPECT_TRUE(mgr.contains(1));
    EXPECT_TRUE(mgr.containsPath("/test.txt"));
}

TEST(IndexManagerTest, FindById) {
    index::IndexManager mgr;
    ArchiveEntry entry;
    entry.id = 42;
    entry.offset = 200;
    entry.storedSize = 1024;
    mgr.insert(entry);

    auto result = mgr.findById(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->entryId, 42);
    EXPECT_EQ(result->offset, 200);
    EXPECT_EQ(result->size, 1024);
}

TEST(IndexManagerTest, FindByPath) {
    index::IndexManager mgr;
    ArchiveEntry entry;
    entry.id = 5;
    entry.path = "/dir/file.bin";
    entry.offset = 500;
    mgr.insert(entry);

    auto result = mgr.findByPath("/dir/file.bin");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->entryId, 5);
}

TEST(IndexManagerTest, NotFound) {
    index::IndexManager mgr;
    EXPECT_FALSE(mgr.findById(999).has_value());
    EXPECT_FALSE(mgr.findByPath("/nonexistent").has_value());
}

TEST(IndexManagerTest, Remove) {
    index::IndexManager mgr;
    ArchiveEntry entry;
    entry.id = 1;
    entry.path = "/test.txt";
    mgr.insert(entry);

    EXPECT_TRUE(mgr.remove(1));
    EXPECT_FALSE(mgr.contains(1));
    EXPECT_FALSE(mgr.remove(1));
}

TEST(IndexManagerTest, Clear) {
    index::IndexManager mgr;
    for (int i = 0; i < 10; ++i) {
        ArchiveEntry entry;
        entry.id = static_cast<EntryID>(i);
        entry.path = "/file" + std::to_string(i);
        mgr.insert(entry);
    }
    EXPECT_EQ(mgr.size(), 10);
    mgr.clear();
    EXPECT_EQ(mgr.size(), 0);
}

TEST(IndexManagerTest, FindByType) {
    index::IndexManager mgr;
    ArchiveEntry file1, file2, dir1;
    file1.id = 1; file1.type = EntryType::File;
    file2.id = 2; file2.type = EntryType::File;
    dir1.id = 3; dir1.type = EntryType::Directory;

    mgr.insert(file1);
    mgr.insert(file2);
    mgr.insert(dir1);

    auto files = mgr.findByType(EntryType::File);
    EXPECT_EQ(files.size(), 2);

    auto dirs = mgr.findByType(EntryType::Directory);
    EXPECT_EQ(dirs.size(), 1);
}

TEST(IndexManagerTest, FindByPrefix) {
    index::IndexManager mgr;
    ArchiveEntry e1, e2, e3;
    e1.id = 1; e1.path = "/a/file1.txt";
    e2.id = 2; e2.path = "/a/file2.txt";
    e3.id = 3; e3.path = "/b/file3.txt";

    mgr.insert(e1);
    mgr.insert(e2);
    mgr.insert(e3);

    auto results = mgr.findByPrefix("/a");
    EXPECT_EQ(results.size(), 2);
}

TEST(IndexManagerTest, SerializeDeserialize) {
    index::IndexManager mgr;
    for (int i = 0; i < 5; ++i) {
        ArchiveEntry entry;
        entry.id = static_cast<EntryID>(i);
        entry.offset = static_cast<Offset>(i * 100);
        entry.storedSize = 64;
        mgr.insert(entry);
    }

    auto data = mgr.serialize();
    EXPECT_FALSE(data.empty());

    index::IndexManager parsed;
    auto ec = parsed.deserialize(data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(parsed.size(), 5);
    EXPECT_TRUE(parsed.contains(0));
    EXPECT_TRUE(parsed.contains(4));
}

TEST(IndexManagerTest, DeserializeCorrupt) {
    index::IndexManager mgr;
    std::vector<uint8_t> corrupt = {0xFF, 0xFF, 0xFF};
    auto ec = mgr.deserialize(corrupt);
    EXPECT_TRUE(ec);
}

TEST(IndexManagerTest, DeserializeRejectsExcessiveCount) {
    // VarInt-encoded count = 0x20000000 (536,870,912 entries)
    // With min 35 bytes/entry = ~18.8 GB needed, but input is tiny.
    std::vector<uint8_t> data = {0x80, 0x80, 0x80, 0x80, 0x02};
    index::IndexManager mgr;
    auto ec = mgr.deserialize(data);
    EXPECT_TRUE(ec);
}

TEST(IndexManagerTest, Validate) {
    index::IndexManager mgr;
    EXPECT_TRUE(mgr.validate());

    ArchiveEntry entry;
    entry.id = 1;
    mgr.insert(entry);
    EXPECT_TRUE(mgr.validate());
}

TEST(IndexManagerTest, BatchInsert) {
    index::IndexManager mgr;
    std::vector<ArchiveEntry> entries;
    for (int i = 0; i < 100; ++i) {
        ArchiveEntry entry;
        entry.id = static_cast<EntryID>(i);
        entry.path = "/file" + std::to_string(i);
        entries.push_back(entry);
    }

    mgr.insert(entries);
    EXPECT_EQ(mgr.size(), 100);
}

TEST(IndexManagerTest, PathMapConsistency) {
    index::IndexManager mgr;
    ArchiveEntry entry;
    entry.id = 1;
    entry.path = "/test.txt";
    mgr.insert(entry);

    EXPECT_EQ(mgr.pathMap().size(), 1);
    EXPECT_EQ(mgr.pathMap().at("/test.txt"), 1);
}

} // namespace test
} // namespace nebula
