#include "nebula/dashboard/DashboardController.hpp"
#include "nebula/archive/ArchiveReader.hpp"

#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace nebula {
namespace dashboard {

InspectionResult DashboardController::inspectArchive(const std::string& archivePath) {
    InspectionResult result;
    collector_.reset();

    if (!fs::exists(archivePath)) {
        result.success = false;
        result.errorMessage = "Archive file not found: " + archivePath;
        return result;
    }

    archive::ReaderConfig config;
    config.verifyChecksums = false; // Fast inspection pass
    archive::ArchiveReader reader(config);

    if (auto ec = reader.open(archivePath)) {
        result.success = false;
        result.errorMessage = "Failed to open archive: " + ec.message();
        return result;
    }

    auto entriesResult = reader.listEntries();
    if (std::holds_alternative<ParseError>(entriesResult)) {
        const auto& err = std::get<ParseError>(entriesResult);
        result.success = false;
        result.errorMessage = "Failed to list archive entries: " + err.message;
        return result;
    }

    const auto& entries = std::get<std::vector<ArchiveEntry>>(entriesResult);

    uint64_t totalOriginal = 0;
    uint64_t totalStored = 0;
    std::vector<EntropySample> samples;
    samples.reserve(entries.size());

    uint64_t currentOffset = 0;
    for (const auto& entry : entries) {
        totalOriginal += entry.originalSize;
        totalStored += entry.storedSize;

        // Sample entry data for entropy
        auto extractRes = entry.path.empty() ? reader.extractEntry(entry.id) : reader.extractEntry(entry.path);
        if (std::holds_alternative<std::vector<uint8_t>>(extractRes)) {
            const auto& data = std::get<std::vector<uint8_t>>(extractRes);
            auto sample = DashboardMetricsCollector::sampleBlockEntropy(currentOffset, data);
            samples.push_back(sample);
            collector_.recordCacheAccess(true);
        } else {
            collector_.recordCacheAccess(false);
        }
        currentOffset += entry.storedSize;
    }

    if (reader.needsRecovery()) {
        collector_.recordJournalTx(false);
    } else {
        collector_.recordJournalTx(true);
    }

    collector_.updateStorageStats(entries.size(), totalOriginal, totalStored, entries.size());

    auto snapshot = collector_.snapshot();
    result.snapshot = snapshot;
    result.sampledBlocks = samples.size();
    result.renderedOutput = renderer_.render(archivePath, snapshot, samples);
    result.success = true;

    return result;
}

} // namespace dashboard
} // namespace nebula
