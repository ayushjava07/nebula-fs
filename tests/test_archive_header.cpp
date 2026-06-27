#include <gtest/gtest.h>
#include "nebula/archive/ArchiveHeader.hpp"

namespace nebula {
namespace test {

TEST(ArchiveHeaderTest, DefaultHeader) {
    archive::ArchiveHeader header;
    EXPECT_TRUE(header.isValid());
    EXPECT_EQ(header.versionMajor(), 1);
    EXPECT_EQ(header.versionMinor(), 0);
    EXPECT_EQ(header.flags(), 0);
    EXPECT_EQ(header.archiveSize(), 0);
}

TEST(ArchiveHeaderTest, SerializeDeserialize) {
    archive::ArchiveHeader header;
    header.setFlags(0x1234);
    header.setArchiveSize(1000000);
    header.setEntryCount(500);
    header.setMetadataOffset(200);
    header.setMetadataSize(1000);
    header.setDirectoryOffset(1200);
    header.setDirectorySize(500);
    header.updateChecksum();

    auto data = header.serialize();
    EXPECT_EQ(data.size(), archive::ArchiveHeader::headerSize());

    archive::ArchiveHeader parsed;
    auto ec = parsed.parse(data);
    EXPECT_FALSE(ec);
    EXPECT_EQ(parsed.flags(), 0x1234);
    EXPECT_EQ(parsed.archiveSize(), 1000000);
    EXPECT_EQ(parsed.entryCount(), 500);
    EXPECT_EQ(parsed.metadataOffset(), 200);
    EXPECT_EQ(parsed.metadataSize(), 1000);
    EXPECT_EQ(parsed.directoryOffset(), 1200);
    EXPECT_EQ(parsed.directorySize(), 500);
}

TEST(ArchiveHeaderTest, InvalidMagic) {
    archive::ArchiveHeader header;
    std::vector<uint8_t> badData(archive::ArchiveHeader::headerSize(), 0);
    auto ec = header.parse(badData);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, make_error_code(ErrorCode::InvalidMagic));
}

TEST(ArchiveHeaderTest, TruncatedData) {
    archive::ArchiveHeader header;
    std::vector<uint8_t> truncated(10, 0);
    auto ec = header.parse(truncated);
    EXPECT_TRUE(ec);
}

TEST(ArchiveHeaderTest, ChecksumVerification) {
    archive::ArchiveHeader header;
    header.setEntryCount(100);
    header.updateChecksum();

    auto data = header.serialize();
    EXPECT_TRUE(header.verifyChecksum());

    data[100] ^= 0xFF;
    archive::ArchiveHeader parsed;
    auto ec = parsed.parse(data);
    EXPECT_TRUE(ec);
}

TEST(ArchiveHeaderTest, VersionCheck) {
    archive::ArchiveHeader header;
    auto& raw = header.raw();
    raw.versionMajor = 99;
    EXPECT_FALSE(header.isValid());
}

TEST(ArchiveHeaderTest, HeaderSize) {
    EXPECT_EQ(archive::ArchiveHeader::headerSize(), 158);
}

TEST(ArchiveHeaderTest, AllSetters) {
    archive::ArchiveHeader header;
    header.setFlags(0xFFFF);
    header.setArchiveSize(999999999999ULL);
    header.setMetadataOffset(1000);
    header.setDirectoryOffset(2000);
    header.setIndexOffset(3000);
    header.setChunkOffset(4000);
    header.setBlocksOffset(5000);
    header.setJournalOffset(6000);
    header.setEntryCount(9999);
    header.setMetadataSize(100);
    header.setDirectorySize(200);
    header.setIndexSize(300);
    header.setChunkSize(400);
    header.setBlocksSize(500);
    header.setJournalSize(600);

    EXPECT_EQ(header.flags(), 0xFFFF);
    EXPECT_EQ(header.archiveSize(), 999999999999ULL);
    EXPECT_EQ(header.metadataOffset(), 1000);
    EXPECT_EQ(header.directoryOffset(), 2000);
    EXPECT_EQ(header.indexOffset(), 3000);
    EXPECT_EQ(header.chunkOffset(), 4000);
    EXPECT_EQ(header.blocksOffset(), 5000);
    EXPECT_EQ(header.journalOffset(), 6000);
    EXPECT_EQ(header.entryCount(), 9999);
    EXPECT_EQ(header.metadataSize(), 100);
    EXPECT_EQ(header.directorySize(), 200);
}

} // namespace test
} // namespace nebula
