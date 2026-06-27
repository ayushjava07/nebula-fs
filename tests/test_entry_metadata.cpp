#include <gtest/gtest.h>
#include "nebula/metadata/EntryMetadata.hpp"

namespace nebula {
namespace test {

TEST(EntryMetadataTest, SetAndGetAttribute) {
    metadata::EntryMetadata meta;
    meta.setAttribute("author", "test_user");
    meta.setAttribute("encoding", "utf-8");

    EXPECT_EQ(meta.getAttribute("author"), "test_user");
    EXPECT_EQ(meta.getAttribute("encoding"), "utf-8");
}

TEST(EntryMetadataTest, MissingAttribute) {
    metadata::EntryMetadata meta;
    EXPECT_FALSE(meta.getAttribute("nonexistent").has_value());
}

TEST(EntryMetadataTest, HasAttribute) {
    metadata::EntryMetadata meta;
    meta.setAttribute("key", "value");
    EXPECT_TRUE(meta.hasAttribute("key"));
    EXPECT_FALSE(meta.hasAttribute("missing"));
}

TEST(EntryMetadataTest, RemoveAttribute) {
    metadata::EntryMetadata meta;
    meta.setAttribute("key", "value");
    EXPECT_TRUE(meta.removeAttribute("key"));
    EXPECT_FALSE(meta.removeAttribute("key"));
}

TEST(EntryMetadataTest, AttributeNames) {
    metadata::EntryMetadata meta;
    meta.setAttribute("a", "1");
    meta.setAttribute("b", "2");
    auto names = meta.attributeNames();
    EXPECT_EQ(names.size(), 2);
}

TEST(EntryMetadataTest, Tags) {
    metadata::EntryMetadata meta;
    meta.addTag("important");
    meta.addTag("compressed");
    EXPECT_TRUE(meta.hasTag("important"));
    EXPECT_TRUE(meta.hasTag("compressed"));
    EXPECT_FALSE(meta.hasTag("missing"));

    EXPECT_TRUE(meta.removeTag("compressed"));
    EXPECT_FALSE(meta.hasTag("compressed"));
}

TEST(EntryMetadataTest, SetTagsVector) {
    metadata::EntryMetadata meta;
    meta.setTags({"tag1", "tag2", "tag3"});
    EXPECT_EQ(meta.tags().size(), 3);
}

TEST(EntryMetadataTest, DuplicateTag) {
    metadata::EntryMetadata meta;
    meta.addTag("tag");
    meta.addTag("tag");
    EXPECT_EQ(meta.tags().size(), 1);
}

TEST(EntryMetadataTest, ContentType) {
    metadata::EntryMetadata meta;
    meta.setContentType("text/plain");
    EXPECT_EQ(meta.contentType(), "text/plain");
}

TEST(EntryMetadataTest, SourcePath) {
    metadata::EntryMetadata meta;
    meta.setSourcePath("/original/path.txt");
    EXPECT_EQ(meta.sourcePath(), "/original/path.txt");
}

TEST(EntryMetadataTest, Notes) {
    metadata::EntryMetadata meta;
    meta.setNotes("This is a test entry");
    EXPECT_EQ(meta.notes(), "This is a test entry");
}

TEST(EntryMetadataTest, Clear) {
    metadata::EntryMetadata meta;
    meta.setAttribute("key", "value");
    meta.addTag("tag");
    meta.setContentType("text/plain");

    meta.clear();
    EXPECT_EQ(meta.attributeCount(), 0);
    EXPECT_TRUE(meta.tags().empty());
    EXPECT_TRUE(meta.contentType().empty());
}

TEST(EntryMetadataTest, SerializeDeserialize) {
    metadata::EntryMetadata meta;
    meta.setAttribute("author", "test");
    meta.setAttribute("encoding", "binary");
    meta.addTag("backup");
    meta.setContentType("application/octet-stream");

    auto data = meta.serialize();
    EXPECT_FALSE(data.empty());

    metadata::EntryMetadata parsed;
    auto ec = parsed.deserialize(data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(parsed.getAttribute("author"), "test");
    EXPECT_TRUE(parsed.hasTag("backup"));
    EXPECT_EQ(parsed.contentType(), "application/octet-stream");
}

TEST(EntryMetadataTest, BinaryAttribute) {
    metadata::EntryMetadata meta;
    std::vector<uint8_t> binData = {0xDE, 0xAD, 0xBE, 0xEF};
    meta.setAttribute("binary_key", binData);
    EXPECT_TRUE(meta.hasAttribute("binary_key"));

    auto result = meta.getAttribute("binary_key");
    ASSERT_TRUE(result.has_value());
}

} // namespace test
} // namespace nebula
