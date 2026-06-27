#include <gtest/gtest.h>
#include "nebula/archive/ArchiveWriter.hpp"
#include "nebula/archive/ArchiveReader.hpp"
#include "nebula/parser/Parser.hpp"
#include <fstream>
#include <filesystem>

namespace nebula {
namespace test {

class IntegrationTest : public ::testing::Test {
protected:
    std::string archivePath;

    void SetUp() override {
        archivePath = "/tmp/nebula_test_" + std::to_string(::getpid()) + ".nbf";
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(archivePath, ec);
    }
};

TEST_F(IntegrationTest, WriteAndReadEmptyArchive) {
    {
        archive::ArchiveWriter writer;
        auto ec = writer.open(archivePath);
        EXPECT_FALSE(ec);
        ec = writer.close();
        EXPECT_FALSE(ec);
    }

    {
        archive::ArchiveReader reader;
        auto ec = reader.open(archivePath);
        EXPECT_FALSE(ec);
        EXPECT_TRUE(reader.isOpen());
        EXPECT_EQ(reader.entryCount(), 0);
    }
}

TEST_F(IntegrationTest, WriteAndReadBlob) {
    std::vector<uint8_t> testData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    {
        archive::ArchiveWriter writer;
        auto ec = writer.open(archivePath);
        EXPECT_FALSE(ec);
        ec = writer.addBlob(testData, "/test.blob");
        EXPECT_FALSE(ec);
        ec = writer.close();
        EXPECT_FALSE(ec);
    }

    {
        archive::ArchiveReader reader;
        auto ec = reader.open(archivePath);
        EXPECT_FALSE(ec);

        auto entries = reader.listEntries();
        EXPECT_FALSE(isError(entries));
        EXPECT_EQ(getValue(entries).size(), 1);

        auto data = reader.extractEntry("/test.blob");
        EXPECT_FALSE(isError(data));
        EXPECT_EQ(getValue(data), testData);
    }
}

TEST_F(IntegrationTest, WriteAndReadMultipleFiles) {
    {
        archive::ArchiveWriter writer;
        auto ec = writer.open(archivePath);
        EXPECT_FALSE(ec);

        writer.addBlob(std::vector<uint8_t>{1, 2, 3}, "/file1.bin");
        writer.addBlob(std::vector<uint8_t>{4, 5, 6}, "/file2.bin");
        writer.addBlob(std::vector<uint8_t>{7, 8, 9}, "/file3.bin");
        ec = writer.close();
        EXPECT_FALSE(ec);
    }

    {
        archive::ArchiveReader reader;
        auto ec = reader.open(archivePath);
        EXPECT_FALSE(ec);
        EXPECT_EQ(reader.entryCount(), 3);
    }
}

TEST_F(IntegrationTest, WriteWithMetadata) {
    {
        archive::ArchiveWriter writer;
        writer.setMetadata("author", "NebulaFS");
        writer.setMetadata("version", "1.0");
        auto ec = writer.open(archivePath);
        EXPECT_FALSE(ec);
        writer.addBlob(std::vector<uint8_t>{1, 2, 3}, "/data.bin");
        ec = writer.close();
        EXPECT_FALSE(ec);
    }

    {
        archive::ArchiveReader reader;
        auto ec = reader.open(archivePath);
        EXPECT_FALSE(ec);

        auto author = reader.getMetadata("author");
        EXPECT_TRUE(author.has_value());
        EXPECT_EQ(*author, "NebulaFS");
    }
}

TEST_F(IntegrationTest, ParserIntegration) {
    archive::ArchiveWriter writer;
    writer.setMetadata("test", "value");
    auto ec = writer.open(archivePath);
    EXPECT_FALSE(ec);
    writer.addBlob(std::vector<uint8_t>{1, 2, 3}, "/test.bin");
    ec = writer.close();
    EXPECT_FALSE(ec);

    parser::Parser p;
    auto result = p.parseFile(archivePath);
    EXPECT_FALSE(isError(result));
    EXPECT_TRUE(getValue(result).valid);
}

TEST_F(IntegrationTest, LargeBlobRoundTrip) {
    std::vector<uint8_t> largeData(100000);
    for (size_t i = 0; i < largeData.size(); ++i) {
        largeData[i] = static_cast<uint8_t>(i & 0xFF);
    }

    {
        archive::ArchiveWriter writer;
        auto ec = writer.open(archivePath);
        EXPECT_FALSE(ec);
        ec = writer.addBlob(largeData, "/large.bin");
        EXPECT_FALSE(ec);
        ec = writer.close();
        EXPECT_FALSE(ec);
    }

    {
        archive::ArchiveReader reader;
        auto ec = reader.open(archivePath);
        EXPECT_FALSE(ec);

        auto data = reader.extractEntry("/large.bin");
        EXPECT_FALSE(isError(data));
        EXPECT_EQ(getValue(data).size(), largeData.size());
        EXPECT_EQ(getValue(data), largeData);
    }
}

TEST_F(IntegrationTest, WriteAndListDirectory) {
    archive::ArchiveWriter writer;
    writer.open(archivePath);
    writer.addDirectory("/mydir");
    writer.addBlob(std::vector<uint8_t>{1}, "/mydir/file1.txt");
    writer.addBlob(std::vector<uint8_t>{2}, "/mydir/file2.txt");
    writer.close();

    archive::ArchiveReader reader;
    reader.open(archivePath);
    auto entries = reader.listEntries();
    EXPECT_FALSE(isError(entries));
    EXPECT_EQ(getValue(entries).size(), 3);
}

} // namespace test
} // namespace nebula
