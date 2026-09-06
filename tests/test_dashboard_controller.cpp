#include <gtest/gtest.h>
#include "nebula/dashboard/DashboardController.hpp"
#include "nebula/archive/ArchiveWriter.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace nebula::dashboard;

TEST(DashboardControllerTest, NonExistentArchiveReturnsError) {
    DashboardController controller;
    auto res = controller.inspectArchive("/nonexistent/path/archive.nbf");
    EXPECT_FALSE(res.success);
    EXPECT_NE(res.errorMessage.find("Archive file not found"), std::string::npos);
}

TEST(DashboardControllerTest, InspectValidArchive) {
    fs::path tempArchive = fs::temp_directory_path() / "test_inspect.nbf";
    fs::remove(tempArchive);

    nebula::archive::WriterConfig config;
    nebula::archive::ArchiveWriter writer(config);
    ASSERT_FALSE(writer.open(tempArchive.string()));

    std::vector<uint8_t> payload1 = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> payload2(256, 0x42);

    ASSERT_FALSE(writer.addBlob(payload1, "/hello.txt"));
    ASSERT_FALSE(writer.addBlob(payload2, "/binary.dat"));
    ASSERT_FALSE(writer.close());

    DashboardController controller;
    auto res = controller.inspectArchive(tempArchive.string());

    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.snapshot.totalEntries, 2);
    EXPECT_EQ(res.sampledBlocks, 2);
    EXPECT_FALSE(res.renderedOutput.empty());
    EXPECT_NE(res.renderedOutput.find("NebulaFS Inspection Dashboard"), std::string::npos);

    fs::remove(tempArchive);
}
