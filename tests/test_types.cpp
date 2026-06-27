#include <gtest/gtest.h>
#include "nebula/Types.hpp"
#include "nebula/Config.hpp"
#include "nebula/Error.hpp"

namespace nebula {
namespace test {

TEST(TypesTest, PermissionsToUnixMode) {
    Permissions p;
    p.ownerRead = true; p.ownerWrite = true; p.ownerExec = true;
    p.groupRead = true; p.groupWrite = false; p.groupExec = false;
    p.otherRead = true; p.otherWrite = false; p.otherExec = false;
    EXPECT_EQ(p.toUnixMode(), 0744u);
}

TEST(TypesTest, PermissionsFromUnixMode) {
    auto p = Permissions::fromUnixMode(0644);
    EXPECT_TRUE(p.ownerRead);
    EXPECT_TRUE(p.ownerWrite);
    EXPECT_FALSE(p.ownerExec);
    EXPECT_TRUE(p.groupRead);
    EXPECT_FALSE(p.groupWrite);
    EXPECT_TRUE(p.otherRead);
}

TEST(TypesTest, PermissionsRoundTrip) {
    for (uint16_t mode : {0000, 0644, 0755, 0777, 0600, 0700}) {
        auto p = Permissions::fromUnixMode(mode);
        EXPECT_EQ(p.toUnixMode(), mode);
    }
}

TEST(TypesTest, TimestampNow) {
    auto ts = Timestamp::now();
    EXPECT_GT(ts.seconds, 0);
    auto tp = ts.toTimePoint();
    auto ts2 = Timestamp::fromTimePoint(tp);
    EXPECT_EQ(ts.seconds, ts2.seconds);
}

TEST(TypesTest, TimestampRoundTrip) {
    Timestamp ts;
    ts.seconds = 1234567890;
    ts.nanos = 123456789;
    auto tp = ts.toTimePoint();
    auto ts2 = Timestamp::fromTimePoint(tp);
    EXPECT_EQ(ts.seconds, ts2.seconds);
    EXPECT_EQ(ts.nanos, ts2.nanos);
}

TEST(TypesTest, EntryFlagsOperators) {
    auto flags = EntryFlags::Compressed | EntryFlags::Encrypted;
    EXPECT_TRUE(hasFlag(flags, EntryFlags::Compressed));
    EXPECT_TRUE(hasFlag(flags, EntryFlags::Encrypted));
    EXPECT_FALSE(hasFlag(flags, EntryFlags::Deduplicated));
}

TEST(TypesTest, UUIDComparison) {
    UUID a{1, 2};
    UUID b{1, 2};
    UUID c{2, 1};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_LT(a, c);
}

TEST(TypesTest, ParseErrorToString) {
    ParseError err;
    err.severity = ErrorSeverity::Fatal;
    err.state = ParserState::Header;
    err.offset = 42;
    err.message = "test error";
    auto str = err.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("FATAL"), std::string::npos);
    EXPECT_NE(str.find("test error"), std::string::npos);
    EXPECT_NE(str.find("42"), std::string::npos);
}

TEST(ErrorTest, ErrorCodeToErrorCode) {
    auto ec = make_error_code(ErrorCode::InvalidMagic);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.value(), 1);
    EXPECT_EQ(ec.category().name(), std::string_view("NebulaFS"));
}

TEST(ErrorTest, ErrorCodeMessages) {
    EXPECT_EQ(NebulaErrorCategory::get().message(0), "success");
    EXPECT_EQ(NebulaErrorCategory::get().message(1), "invalid magic bytes");
    EXPECT_EQ(NebulaErrorCategory::get().message(20), "not a NebulaFS archive");
    EXPECT_EQ(NebulaErrorCategory::get().message(99), "unknown error");
}

TEST(ErrorTest, ToParseError) {
    auto err = toParseError(ErrorCode::CorruptHeader, ParserState::Header, 100, "bad header");
    EXPECT_EQ(err.state, ParserState::Header);
    EXPECT_EQ(err.offset, 100);
    EXPECT_EQ(err.severity, ErrorSeverity::Fatal);
    EXPECT_EQ(err.message, "bad header");
}

TEST(ConfigTest, Constants) {
    EXPECT_EQ(kArchiveVersionMajor, 1);
    EXPECT_EQ(kArchiveVersionMinor, 0);
    EXPECT_EQ(kDefaultBlockSize, 65536);
    EXPECT_EQ(kChecksumLength, 32);
    EXPECT_EQ(kAESKeyLength, 32);
    EXPECT_EQ(kAESIVLength, 12);
    EXPECT_EQ(kAESTagLength, 16);
    EXPECT_GT(kMaxArchiveEntries, 1000000);
}

} // namespace test
} // namespace nebula
