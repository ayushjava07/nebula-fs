#include <gtest/gtest.h>
#include "nebula/filesystem/DirectoryTree.hpp"

namespace nebula {
namespace test {

TEST(DirectoryTreeTest, InsertRoot) {
    filesystem::DirectoryTree tree;
    ArchiveEntry entry;
    entry.id = 1;
    entry.type = EntryType::Directory;
    entry.path = "/";

    auto ec = tree.insert(entry);
    EXPECT_FALSE(ec);
    EXPECT_EQ(tree.root().name, "/");
}

TEST(DirectoryTreeTest, InsertFile) {
    filesystem::DirectoryTree tree;
    ArchiveEntry entry;
    entry.id = 1;
    entry.type = EntryType::File;
    entry.path = "/test.txt";

    auto ec = tree.insert(entry);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(tree.exists("/test.txt"));
}

TEST(DirectoryTreeTest, InsertNestedDirectories) {
    filesystem::DirectoryTree tree;
    ArchiveEntry dir;
    dir.id = 1;
    dir.type = EntryType::Directory;
    dir.path = "/dir1/dir2";

    auto ec = tree.insert(dir);
    EXPECT_FALSE(ec);

    EXPECT_TRUE(tree.exists("/dir1"));
    EXPECT_TRUE(tree.exists("/dir1/dir2"));
}

TEST(DirectoryTreeTest, Find) {
    filesystem::DirectoryTree tree;
    ArchiveEntry entry;
    entry.id = 42;
    entry.type = EntryType::File;
    entry.path = "/path/to/file.txt";

    tree.insert(entry);
    auto node = tree.find("/path/to/file.txt");
    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->id, 42);
}

TEST(DirectoryTreeTest, FindNonExistent) {
    filesystem::DirectoryTree tree;
    auto node = tree.find("/nonexistent");
    EXPECT_FALSE(node.has_value());
}

TEST(DirectoryTreeTest, ListDirectory) {
    filesystem::DirectoryTree tree;
    ArchiveEntry f1, f2;
    f1.id = 1; f1.type = EntryType::File; f1.path = "/a.txt";
    f2.id = 2; f2.type = EntryType::File; f2.path = "/b.txt";

    tree.insert(f1);
    tree.insert(f2);

    auto entries = tree.listDirectory("/");
    EXPECT_EQ(entries.size(), 2);
}

TEST(DirectoryTreeTest, GetAllEntries) {
    filesystem::DirectoryTree tree;
    ArchiveEntry f1, f2, d1;
    f1.id = 1; f1.type = EntryType::File; f1.path = "/f1.txt";
    f2.id = 2; f2.type = EntryType::File; f2.path = "/dir/f2.txt";
    d1.id = 3; d1.type = EntryType::Directory; d1.path = "/dir";

    tree.insert(f1);
    tree.insert(d1);
    tree.insert(f2);

    auto all = tree.getAllEntries();
    EXPECT_GE(all.size(), 3);
}

TEST(DirectoryTreeTest, Remove) {
    filesystem::DirectoryTree tree;
    ArchiveEntry entry;
    entry.id = 1;
    entry.type = EntryType::File;
    entry.path = "/file.txt";

    tree.insert(entry);
    EXPECT_TRUE(tree.exists("/file.txt"));

    EXPECT_TRUE(tree.remove("/file.txt"));
    EXPECT_FALSE(tree.exists("/file.txt"));
}

TEST(DirectoryTreeTest, NormalizePath) {
    EXPECT_EQ(filesystem::DirectoryTree::normalizePath("/"), "/");
    EXPECT_EQ(filesystem::DirectoryTree::normalizePath("/a/b/c"), "/a/b/c");
    EXPECT_EQ(filesystem::DirectoryTree::normalizePath("a/b"), "/a/b");
}

TEST(DirectoryTreeTest, SplitPath) {
    auto parts = filesystem::DirectoryTree::splitPath("/a/b/c");
    EXPECT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(DirectoryTreeTest, PathDepth) {
    EXPECT_EQ(filesystem::DirectoryTree::pathDepth("/"), 0);
    EXPECT_EQ(filesystem::DirectoryTree::pathDepth("/a"), 1);
    EXPECT_EQ(filesystem::DirectoryTree::pathDepth("/a/b/c"), 3);
}

TEST(DirectoryTreeTest, SerializeDeserialize) {
    filesystem::DirectoryTree tree;
    ArchiveEntry f1, f2, d1;
    f1.id = 1; f1.type = EntryType::File; f1.path = "/a.txt";
    f2.id = 2; f2.type = EntryType::File; f2.path = "/dir/b.txt";
    d1.id = 3; d1.type = EntryType::Directory; d1.path = "/dir";

    tree.insert(f1);
    tree.insert(d1);
    tree.insert(f2);

    auto data = tree.serialize();
    EXPECT_FALSE(data.empty());

    filesystem::DirectoryTree parsed;
    auto ec = parsed.deserialize(data);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(parsed.exists("/a.txt"));
    EXPECT_TRUE(parsed.exists("/dir"));
    EXPECT_TRUE(parsed.exists("/dir/b.txt"));
}

TEST(DirectoryTreeTest, Clear) {
    filesystem::DirectoryTree tree;
    ArchiveEntry entry;
    entry.id = 1; entry.type = EntryType::File; entry.path = "/test.txt";
    tree.insert(entry);

    tree.clear();
    EXPECT_FALSE(tree.exists("/test.txt"));
    EXPECT_EQ(tree.nodeCount(), 0);
}

TEST(DirectoryTreeTest, InsertNode) {
    filesystem::DirectoryTree tree;
    DirectoryNode node;
    node.name = "/";
    node.id = 1;
    node.parentId = 0;

    auto ec = tree.insertNode(node);
    EXPECT_FALSE(ec);
}

} // namespace test
} // namespace nebula
