#include <gtest/gtest.h>
#include "nebula/parser/Parser.hpp"
#include "nebula/parser/MetadataParser.hpp"
#include "nebula/parser/StreamParser.hpp"

namespace nebula {
namespace test {

class ParserTest : public ::testing::Test {
protected:
    parser::Parser parser;
};

TEST_F(ParserTest, EmptyInput) {
    std::vector<uint8_t> empty;
    auto result = parser.parse(empty);
    EXPECT_TRUE(isError(result));
    EXPECT_EQ(parser.state(), ParserState::Error);
}

TEST_F(ParserTest, TruncatedHeader) {
    std::vector<uint8_t> truncated(50, 0);
    auto result = parser.parse(truncated);
    EXPECT_TRUE(isError(result));
}

TEST_F(ParserTest, ParseHeaderOnly) {
    archive::ArchiveHeader header;
    header.setEntryCount(0);
    header.updateChecksum();
    auto data = header.serialize();

    auto result = parser.parseHeader(data);
    EXPECT_FALSE(isError(result));
    EXPECT_EQ(getValue(result).entryCount(), 0);
}

TEST_F(ParserTest, ParseInvalidHeader) {
    std::vector<uint8_t> badData(176, 0);
    auto result = parser.parseHeader(badData);
    EXPECT_TRUE(isError(result));
}

TEST_F(ParserTest, MetadataParserValid) {
    parser::MetadataParser metaParser;
    metadata::MetadataStore store;
    store.set("key1", "value1");
    store.set("key2", "value2");

    auto serialized = metaParser.serialize(store);
    auto result = metaParser.parse(serialized, serialized.size());
    EXPECT_FALSE(isError(result));

    auto& parsed = getValue(result);
    EXPECT_TRUE(parsed.contains("key1"));
    EXPECT_TRUE(parsed.contains("key2"));
    EXPECT_EQ(parsed.getString("key1"), "value1");
}

TEST_F(ParserTest, MetadataParserCorrupt) {
    parser::MetadataParser metaParser;
    std::vector<uint8_t> corrupt = {0xFF, 0xFF, 0xFF};
    auto result = metaParser.parse(corrupt, corrupt.size());
    EXPECT_TRUE(isError(result));
}

TEST_F(ParserTest, MetadataParserEmpty) {
    parser::MetadataParser metaParser;
    auto result = metaParser.parse({}, 0);
    EXPECT_FALSE(isError(result));
    EXPECT_TRUE(getValue(result).empty());
}

TEST_F(ParserTest, StreamParserBasic) {
    parser::StreamParser streamParser;

    archive::ArchiveHeader header;
    header.setEntryCount(0);
    header.setArchiveSize(archive::ArchiveHeader::headerSize());
    header.updateChecksum();

    auto headerData = header.serialize();
    auto result = streamParser.feed(headerData);
    EXPECT_FALSE(isError(result));
    EXPECT_TRUE(streamParser.isComplete());
}

TEST_F(ParserTest, StreamParserIncremental) {
    parser::StreamParser streamParser;

    archive::ArchiveHeader header;
    header.setEntryCount(0);
    header.setArchiveSize(archive::ArchiveHeader::headerSize());
    header.updateChecksum();

    auto headerData = header.serialize();

    // Feed byte by byte
    for (size_t i = 0; i < headerData.size(); ++i) {
        auto result = streamParser.feed(
            std::span<const uint8_t>(&headerData[i], 1));
        EXPECT_FALSE(isError(result));
    }

    EXPECT_TRUE(streamParser.isComplete());
}

TEST_F(ParserTest, StreamParserReset) {
    parser::StreamParser streamParser;

    archive::ArchiveHeader header;
    header.setEntryCount(0);
    header.updateChecksum();
    auto data = header.serialize();

    streamParser.feed(data);
    EXPECT_TRUE(streamParser.isComplete());

    streamParser.reset();
    EXPECT_FALSE(streamParser.isComplete());
    EXPECT_EQ(streamParser.state(), ParserState::Init);
}

TEST_F(ParserTest, StreamParserProgress) {
    parser::StreamParser streamParser;

    archive::ArchiveHeader header;
    header.setEntryCount(0);
    header.setArchiveSize(1000);
    header.updateChecksum();

    auto data = header.serialize();
    streamParser.feed(data);
    EXPECT_GE(streamParser.progress(), 0.0);
}

TEST_F(ParserTest, ParseMetadataSection) {
    parser::MetadataParser metaParser;
    metadata::MetadataStore store;
    store.set("archive_name", "test_archive");
    store.set("version", "1.0");
    store.set("creator", "NebulaFS");

    auto data = metaParser.serialize(store);
    EXPECT_TRUE(parser::MetadataParser::quickValidate(data));

    auto result = metaParser.parse(data, data.size());
    EXPECT_FALSE(isError(result));

    auto& parsed = getValue(result);
    EXPECT_EQ(parsed.getString("archive_name"), "test_archive");
    EXPECT_EQ(parsed.getString("version"), "1.0");
    EXPECT_EQ(parsed.getString("creator"), "NebulaFS");
}

TEST_F(ParserTest, ParseMetadataValidate) {
    parser::MetadataParser metaParser;
    metadata::MetadataStore store;
    store.set("key", "value");
    auto data = metaParser.serialize(store);

    auto ec = metaParser.validate(data, data.size());
    EXPECT_FALSE(ec);

    ec = metaParser.validate({}, 0);
    EXPECT_FALSE(ec);
}

TEST_F(ParserTest, ParserStateTransitions) {
    parser::Parser p;
    EXPECT_EQ(p.state(), ParserState::Init);

    archive::ArchiveHeader header;
    header.setEntryCount(0);
    header.updateChecksum();
    auto data = header.serialize();

    auto result = p.parse(data);
    EXPECT_EQ(p.state(), ParserState::Complete);
    EXPECT_FALSE(isError(result));
}

TEST_F(ParserTest, ParserWarnings) {
    parser::Parser p(parser::ParserConfig{true, false, true});
    std::vector<uint8_t> data(176 + 10, 0);
    auto result = p.parse(data);
    EXPECT_TRUE(isError(result));
}

} // namespace test
} // namespace nebula
