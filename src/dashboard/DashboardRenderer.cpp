#include "nebula/dashboard/DashboardRenderer.hpp"

#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace nebula {
namespace dashboard {

std::string DashboardRenderer::formatBytes(uint64_t bytes) {
    constexpr const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    size_t unitIdx = 0;

    while (size >= 1024.0 && unitIdx < 4) {
        size /= 1024.0;
        unitIdx++;
    }

    std::ostringstream ss;
    if (unitIdx == 0) {
        ss << bytes << " B";
    } else {
        ss << std::fixed << std::setprecision(2) << size << " " << units[unitIdx];
    }
    return ss.str();
}

std::string DashboardRenderer::renderProgressBar(double fraction, size_t width, bool colorize) {
    fraction = std::clamp(fraction, 0.0, 1.0);
    size_t filled = static_cast<size_t>(std::round(fraction * static_cast<double>(width)));
    filled = std::min(filled, width);

    std::ostringstream ss;
    ss << "[";
    if (colorize) {
        ss << ConsoleColor::Green;
    }
    for (size_t i = 0; i < filled; ++i) {
        ss << "=";
    }
    if (colorize) {
        ss << ConsoleColor::Gray;
    }
    for (size_t i = filled; i < width; ++i) {
        ss << "-";
    }
    if (colorize) {
        ss << ConsoleColor::Reset;
    }
    ss << "] " << std::fixed << std::setprecision(1) << (fraction * 100.0) << "%";
    return ss.str();
}

std::string DashboardRenderer::renderEntropyBar(const std::vector<EntropySample>& samples, size_t barWidth) {
    if (samples.empty() || barWidth == 0) {
        return std::string(barWidth, '-');
    }

    std::string result;
    result.reserve(barWidth);

    for (size_t col = 0; col < barWidth; ++col) {
        size_t sampleIdx = (col * samples.size()) / barWidth;
        const auto& sample = samples[sampleIdx];

        char ch = '?';
        switch (sample.classification) {
            case BlockEntropyClass::ZeroFilled:
                ch = '.';
                break;
            case BlockEntropyClass::LowEntropyText:
                ch = 'T';
                break;
            case BlockEntropyClass::StructuredBinary:
                ch = 'B';
                break;
            case BlockEntropyClass::HighEntropyCompressed:
                ch = 'C';
                break;
            case BlockEntropyClass::EncryptedUniform:
                ch = 'E';
                break;
        }
        result.push_back(ch);
    }
    return result;
}

std::string DashboardRenderer::renderHeader(const std::string& archivePath) const {
    std::ostringstream ss;
    ss << "================================================================================\n";
    ss << "  NebulaFS Inspection Dashboard | Target: " << (archivePath.empty() ? "(in-memory)" : archivePath) << "\n";
    ss << "================================================================================\n";
    return ss.str();
}

std::string DashboardRenderer::renderMetricsSummary(const DashboardMetricsSnapshot& s) const {
    std::ostringstream ss;
    ss << "\n--- Storage & Compression Metrics ---\n";
    ss << "  Total Entries:        " << s.totalEntries << "\n";
    ss << "  Uncompressed Size:    " << formatBytes(s.uncompressedBytes) << "\n";
    ss << "  Compressed Size:      " << formatBytes(s.compressedBytes) << "\n";
    ss << "  Compression Ratio:    " << std::fixed << std::setprecision(2) << s.compressionRatio << ":1\n";
    ss << "  Space Savings:        " << std::fixed << std::setprecision(1) << s.spaceSavingsPercent << "%\n";
    ss << "  Savings Gauge:        " << renderProgressBar(s.spaceSavingsPercent / 100.0, 30) << "\n";
    ss << "  Active Blocks:        " << s.activeBlocks << "\n";
    ss << "  Fragmentation Index:  " << std::fixed << std::setprecision(4) << s.fragmentationIndex << "\n";

    if (options_.showCacheGauges) {
        ss << "\n--- Cache Subsystem ---\n";
        ss << "  Cache Hits:           " << s.cacheHits << "\n";
        ss << "  Cache Misses:         " << s.cacheMisses << "\n";
        ss << "  Hit Ratio Gauge:      " << renderProgressBar(s.cacheHitRatio, 30) << "\n";
    }

    return ss.str();
}

std::string DashboardRenderer::renderEntropySection(const std::vector<EntropySample>& samples) const {
    std::ostringstream ss;
    ss << "\n--- Archive Block Entropy Heatmap ---\n";
    size_t barWidth = options_.terminalWidth > 20 ? options_.terminalWidth - 10 : 40;
    ss << "  [" << renderEntropyBar(samples, barWidth) << "]\n";
    ss << "  Legend: [.] Zero  [T] Text  [B] Binary  [C] Compressed  [E] Encrypted\n";
    return ss.str();
}

std::string DashboardRenderer::renderJournalSection(const DashboardMetricsSnapshot& s) const {
    std::ostringstream ss;
    ss << "\n--- Journal & Transaction State ---\n";
    ss << "  Committed Transactions: " << s.journalCommittedTx << "\n";
    ss << "  Pending Transactions:   " << s.journalPendingTx << "\n";
    ss << "  Journal Status:         " << (s.journalPendingTx > 0 ? "WAL DIRTY / ROLL-FORWARD READY" : "CLEAN / SYNCHRONIZED") << "\n";
    return ss.str();
}

std::string DashboardRenderer::renderJson(const std::string& archivePath,
                                         const DashboardMetricsSnapshot& s,
                                         const std::vector<EntropySample>& samples) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"archive\": \"" << archivePath << "\",\n";
    ss << "  \"metrics\": {\n";
    ss << "    \"totalEntries\": " << s.totalEntries << ",\n";
    ss << "    \"uncompressedBytes\": " << s.uncompressedBytes << ",\n";
    ss << "    \"compressedBytes\": " << s.compressedBytes << ",\n";
    ss << "    \"compressionRatio\": " << std::fixed << std::setprecision(4) << s.compressionRatio << ",\n";
    ss << "    \"spaceSavingsPercent\": " << std::fixed << std::setprecision(2) << s.spaceSavingsPercent << ",\n";
    ss << "    \"activeBlocks\": " << s.activeBlocks << ",\n";
    ss << "    \"fragmentationIndex\": " << std::fixed << std::setprecision(4) << s.fragmentationIndex << ",\n";
    ss << "    \"cacheHits\": " << s.cacheHits << ",\n";
    ss << "    \"cacheMisses\": " << s.cacheMisses << ",\n";
    ss << "    \"cacheHitRatio\": " << std::fixed << std::setprecision(4) << s.cacheHitRatio << ",\n";
    ss << "    \"journalCommittedTx\": " << s.journalCommittedTx << ",\n";
    ss << "    \"journalPendingTx\": " << s.journalPendingTx << "\n";
    ss << "  },\n";
    ss << "  \"entropySamplesCount\": " << samples.size() << "\n";
    ss << "}\n";
    return ss.str();
}

std::string DashboardRenderer::render(const std::string& archivePath,
                                      const DashboardMetricsSnapshot& snapshot,
                                      const std::vector<EntropySample>& samples) const {
    if (options_.format == OutputFormat::Json) {
        return renderJson(archivePath, snapshot, samples);
    }

    std::ostringstream ss;
    ss << renderHeader(archivePath);
    ss << renderMetricsSummary(snapshot);

    if (options_.showEntropyHeatmap && !samples.empty()) {
        ss << renderEntropySection(samples);
    }

    if (options_.showJournalStatus) {
        ss << renderJournalSection(snapshot);
    }

    ss << "\n================================================================================\n";
    return ss.str();
}

} // namespace dashboard
} // namespace nebula
