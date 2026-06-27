#include <gtest/gtest.h>
#include "nebula/filesystem/FileResolver.hpp"
#include "nebula/index/IndexManager.hpp"

namespace nebula {
namespace test {

TEST(FileResolverTest, ParentPath) {
    EXPECT_EQ(filesystem::FileResolver::parentPath("/"), "/");
    EXPECT_EQ(filesystem::FileResolver::parentPath("/a/b/c.txt"), "/a/b");
    EXPECT_EQ(filesystem::FileResolver::parentPath("/a.txt"), "/");
}

TEST(FileResolverTest, FileName) {
    EXPECT_EQ(filesystem::FileResolver::fileName("/"), "");
    EXPECT_EQ(filesystem::FileResolver::fileName("/a/b/c.txt"), "c.txt");
    EXPECT_EQ(filesystem::FileResolver::fileName("justfile.txt"), "justfile.txt");
}

TEST(FileResolverTest, GlobMatching) {
    index::IndexManager idx;
    ArchiveEntry e1, e2, e3;
    e1.id = 1; e1.path = "/docs/file1.txt";
    e2.id = 2; e2.path = "/docs/file2.txt";
    e3.id = 3; e3.path = "/images/photo.jpg";

    idx.insert(e1);
    idx.insert(e2);
    idx.insert(e3);

    filesystem::DirectoryTree tree;
    tree.insert(e1);
    tree.insert(e2);
    tree.insert(e3);

    filesystem::FileResolver resolver(&tree, &idx);
    auto results = resolver.glob("*.txt");
}

TEST(FileResolverTest, ListAll) {
    index::IndexManager idx;
    filesystem::DirectoryTree tree;

    ArchiveEntry e1, e2;
    e1.id = 1; e1.path = "/a.txt";
    e2.id = 2; e2.path = "/b.txt";

    idx.insert(e1);
    idx.insert(e2);
    tree.insert(e1);
    tree.insert(e2);

    filesystem::FileResolver resolver(&tree, &idx);
    auto all = resolver.listAll();
    EXPECT_EQ(all.size(), 2);
}

TEST(FileResolverTest, ListDirectory) {
    index::IndexManager idx;
    filesystem::DirectoryTree tree;

    ArchiveEntry f1, f2;
    f1.id = 1; f1.type = EntryType::File; f1.path = "/a.txt";
    f2.id = 2; f2.type = EntryType::File; f2.path = "/b.txt";

    idx.insert(f1);
    idx.insert(f2);
    tree.insert(f1);
    tree.insert(f2);

    filesystem::FileResolver resolver(&tree, &idx);
    auto entries = resolver.listDirectory("/");
    EXPECT_EQ(entries.size(), 2);
}

TEST(FileResolverTest, IsDirectory) {
    filesystem::DirectoryTree tree;
    index::IndexManager idx;

    ArchiveEntry dir;
    dir.id = 1; dir.type = EntryType::Directory; dir.path = "/mydir";
    tree.insert(dir);

    filesystem::FileResolver resolver(&tree, &idx);
    EXPECT_TRUE(resolver.isDirectory("/mydir"));
    EXPECT_FALSE(resolver.isDirectory("/nonexistent"));
}

} // namespace test
} // namespace nebula
