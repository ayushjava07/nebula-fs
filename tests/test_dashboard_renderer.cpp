#include <gtest/gtest.h>
#include "nebula/dashboard/DashboardRenderer.hpp"

using namespace nebula::dashboard;

TEST(DashboardRendererTest, FormatBytes) {
    EXPECT_EQ(DashboardRenderer::formatBytes(500), "500 B");
    EXPECT_EQ(DashboardRenderer::formatBytes(1024), "1.00 KB");
    EXPECT_EQ(DashboardRenderer::formatBytes(1024 * 1024 * 5), "5.00 MB");
    EXPECT_EQ(DashboardRenderer::formatBytes(1024ULL * 1024 * 1024 * 2), "2.00 GB");
}

TEST(DashboardRendererTest, ProgressBarRendering) {
    auto bar0 = DashboardRenderer::renderProgressBar(0.0, 10, false);
    EXPECT_EQ(bar0, "[----------] 0.0%");

    auto bar50 = DashboardRenderer::renderProgressBar(0.5, 10, false);
    EXPECT_EQ(bar50, "[=====-----] 50.0%");

    auto bar100 = DashboardRenderer::renderProgressBar(1.0, 10, false);
    EXPECT_EQ(bar100, "[==========] 100.0%");
}

TEST(DashboardRendererTest, EntropyBarRendering) {
    std::vector<EntropySample> samples;
    samples.push_back({0, 100, 0.0, BlockEntropyClass::ZeroFilled});
    samples.push_back({100, 100, 3.0, BlockEntropyClass::LowEntropyText});
    samples.push_back({200, 100, 6.0, BlockEntropyClass::StructuredBinary});
    samples.push_back({300, 100, 7.5, BlockEntropyClass::HighEntropyCompressed});
    samples.push_back({400, 100, 7.95, BlockEntropyClass::EncryptedUniform});

    std::string bar = DashboardRenderer::renderEntropyBar(samples, 5);
    EXPECT_EQ(bar, ".TBCE");
}

TEST(DashboardRendererTest, FullPlainTextRender) {
    DashboardMetricsSnapshot s;
    s.totalEntries = 42;
    s.uncompressedBytes = 100000;
    s.compressedBytes = 50000;
    s.compressionRatio = 2.0;
    s.spaceSavingsPercent = 50.0;
    s.journalCommittedTx = 10;
    s.journalPendingTx = 0;
    s.cacheHits = 80;
    s.cacheMisses = 20;
    s.cacheHitRatio = 0.8;

    std::vector<EntropySample> samples = {
        {0, 512, 0.0, BlockEntropyClass::ZeroFilled},
        {512, 512, 4.0, BlockEntropyClass::LowEntropyText}
    };

    RendererOptions opts;
    opts.format = OutputFormat::PlainText;
    DashboardRenderer renderer(opts);

    std::string output = renderer.render("my_archive.nbf", s, samples);
    EXPECT_NE(output.find("NebulaFS Inspection Dashboard"), std::string::npos);
    EXPECT_NE(output.find("Total Entries:        42"), std::string::npos);
    EXPECT_NE(output.find("Compression Ratio:    2.00:1"), std::string::npos);
    EXPECT_NE(output.find("CLEAN / SYNCHRONIZED"), std::string::npos);
}

TEST(DashboardRendererTest, JsonRender) {
    DashboardMetricsSnapshot s;
    s.totalEntries = 5;
    s.uncompressedBytes = 2000;
    s.compressedBytes = 1000;
    s.compressionRatio = 2.0;

    std::vector<EntropySample> samples;

    RendererOptions opts;
    opts.format = OutputFormat::Json;
    DashboardRenderer renderer(opts);

    std::string json = renderer.render("backup.nbf", s, samples);
    EXPECT_NE(json.find("\"archive\": \"backup.nbf\""), std::string::npos);
    EXPECT_NE(json.find("\"totalEntries\": 5"), std::string::npos);
    EXPECT_NE(json.find("\"compressionRatio\": 2.0000"), std::string::npos);
}
