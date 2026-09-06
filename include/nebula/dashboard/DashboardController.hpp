#pragma once

#include "nebula/dashboard/DashboardMetrics.hpp"
#include "nebula/dashboard/DashboardRenderer.hpp"
#include "nebula/Types.hpp"

#include <string>
#include <functional>

namespace nebula {
namespace dashboard {

struct InspectionResult {
    bool success{false};
    std::string renderedOutput;
    std::string errorMessage;
    DashboardMetricsSnapshot snapshot;
    size_t sampledBlocks{0};
};

class DashboardController {
public:
    DashboardController() = default;
    explicit DashboardController(RendererOptions options) : renderer_(options) {}

    InspectionResult inspectArchive(const std::string& archivePath);

    void setRendererOptions(const RendererOptions& options) {
        renderer_.setOptions(options);
    }

    [[nodiscard]] const DashboardRenderer& renderer() const noexcept {
        return renderer_;
    }

private:
    DashboardRenderer renderer_;
    DashboardMetricsCollector collector_;
};

} // namespace dashboard
} // namespace nebula
