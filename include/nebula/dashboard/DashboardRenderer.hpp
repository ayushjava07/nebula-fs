#pragma once

#include "nebula/dashboard/DashboardMetrics.hpp"

#include <string>
#include <vector>
#include <ostream>

namespace nebula {
namespace dashboard {

struct ConsoleColor {
    static constexpr const char* Reset   = "\033[0m";
    static constexpr const char* Bold    = "\033[1m";
    static constexpr const char* Dim     = "\033[2m";
    static constexpr const char* Red     = "\033[31m";
    static constexpr const char* Green   = "\033[32m";
    static constexpr const char* Yellow  = "\033[33m";
    static constexpr const char* Blue    = "\033[34m";
    static constexpr const char* Magenta = "\033[35m";
    static constexpr const char* Cyan    = "\033[36m";
    static constexpr const char* Gray    = "\033[90m";
};

enum class OutputFormat {
    PlainText,
    AnsiColor,
    Json
};

struct RendererOptions {
    OutputFormat format{OutputFormat::PlainText};
    size_t terminalWidth{80};
    bool showEntropyHeatmap{true};
    bool showJournalStatus{true};
    bool showCacheGauges{true};
};

class DashboardRenderer {
public:
    explicit DashboardRenderer(RendererOptions options = {}) : options_(options) {}

    std::string render(const std::string& archivePath,
                       const DashboardMetricsSnapshot& snapshot,
                       const std::vector<EntropySample>& samples) const;

    static std::string renderProgressBar(double fraction, size_t width, bool colorize = false);
    static std::string renderEntropyBar(const std::vector<EntropySample>& samples, size_t barWidth);
    static std::string formatBytes(uint64_t bytes);

    void setOptions(const RendererOptions& options) { options_ = options; }
    [[nodiscard]] const RendererOptions& options() const noexcept { return options_; }

private:
    RendererOptions options_;

    std::string renderHeader(const std::string& archivePath) const;
    std::string renderMetricsSummary(const DashboardMetricsSnapshot& s) const;
    std::string renderEntropySection(const std::vector<EntropySample>& samples) const;
    std::string renderJournalSection(const DashboardMetricsSnapshot& s) const;
    std::string renderJson(const std::string& archivePath,
                           const DashboardMetricsSnapshot& snapshot,
                           const std::vector<EntropySample>& samples) const;
};

} // namespace dashboard
} // namespace nebula
