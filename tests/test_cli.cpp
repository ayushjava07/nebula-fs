#include <gtest/gtest.h>
#include "nebula/cli/CommandLine.hpp"

using namespace nebula::cli;

TEST(CommandLineTest, ParseEmptyArgsShowsHelp) {
    CommandLineHandler handler;
    const char* argv[] = {"nebula"};
    auto opts = handler.parse(1, argv);
    EXPECT_EQ(opts.command, CommandType::Help);

    auto res = handler.execute(opts);
    EXPECT_EQ(res.exitCode, 0);
    EXPECT_TRUE(res.success);
    EXPECT_FALSE(res.message.empty());
}

TEST(CommandLineTest, ParseCreateCommand) {
    CommandLineHandler handler;
    const char* argv[] = {"nebula", "create", "-a", "test.nbf", "-c", "zstd", "file1.txt", "file2.txt"};
    auto opts = handler.parse(8, argv);

    EXPECT_EQ(opts.command, CommandType::Create);
    EXPECT_EQ(opts.archivePath, "test.nbf");
    EXPECT_EQ(opts.compressionType, "zstd");
    ASSERT_EQ(opts.inputFiles.size(), 2);
    EXPECT_EQ(opts.inputFiles[0], "file1.txt");
    EXPECT_EQ(opts.inputFiles[1], "file2.txt");
}

TEST(CommandLineTest, ParseExtractCommand) {
    CommandLineHandler handler;
    const char* argv[] = {"nebula", "extract", "-a", "archive.nbf", "-d", "/tmp/extracted"};
    auto opts = handler.parse(6, argv);

    EXPECT_EQ(opts.command, CommandType::Extract);
    EXPECT_EQ(opts.archivePath, "archive.nbf");
    EXPECT_EQ(opts.targetDirectory, "/tmp/extracted");
}

TEST(CommandLineTest, ParseListCommand) {
    CommandLineHandler handler;
    const char* argv[] = {"nebula", "list", "archive.nbf"};
    auto opts = handler.parse(3, argv);

    EXPECT_EQ(opts.command, CommandType::List);
    EXPECT_EQ(opts.archivePath, "archive.nbf");
}

TEST(CommandLineTest, ParseUnknownCommandReturnsError) {
    CommandLineHandler handler;
    const char* argv[] = {"nebula", "nonexistent-cmd"};
    auto opts = handler.parse(2, argv);

    EXPECT_EQ(opts.command, CommandType::Unknown);
    auto res = handler.execute(opts);
    EXPECT_NE(res.exitCode, 0);
    EXPECT_FALSE(res.success);
}

TEST(CommandLineTest, BenchmarkExecution) {
    CommandLineHandler handler;
    const char* argv[] = {"nebula", "benchmark"};
    auto opts = handler.parse(2, argv);

    EXPECT_EQ(opts.command, CommandType::Benchmark);
    auto res = handler.execute(opts);
    EXPECT_EQ(res.exitCode, 0);
    EXPECT_TRUE(res.success);
    EXPECT_GT(res.processedBytes, 0);
}

TEST(CommandLineTest, ParseInspectCommand) {
    CommandLineHandler handler;
    const char* argv[] = {"nebula", "inspect", "-a", "target.nbf"};
    auto opts = handler.parse(4, argv);

    EXPECT_EQ(opts.command, CommandType::Inspect);
    EXPECT_EQ(opts.archivePath, "target.nbf");

    // Missing file returns exitCode != 0
    auto res = handler.execute(opts);
    EXPECT_NE(res.exitCode, 0);
    EXPECT_FALSE(res.success);
}
