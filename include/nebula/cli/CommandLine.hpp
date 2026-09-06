#pragma once

#include "nebula/archive/ArchiveReader.hpp"
#include "nebula/archive/ArchiveWriter.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace nebula::cli {

enum class CommandType {
    Create,
    Extract,
    List,
    Verify,
    Repair,
    Benchmark,
    Help,
    Unknown
};

struct CommandLineOptions {
    CommandType command = CommandType::Unknown;
    std::string archivePath;
    std::string targetDirectory;
    std::vector<std::string> inputFiles;
    std::string filterPattern;
    bool verbose = false;
    bool recursive = true;
    bool calculateChecksums = true;
    int compressionLevel = 3;
    std::string compressionType = "lz4";
    std::string encryptionKey;
};

struct CommandResult {
    int exitCode = 0;
    std::string message;
    size_t processedCount = 0;
    uint64_t processedBytes = 0;
    bool success = true;
};

class CommandLineHandler {
public:
    CommandLineHandler() = default;
    ~CommandLineHandler() = default;

    CommandLineOptions parse(int argc, const char* const* argv);
    CommandResult execute(const CommandLineOptions& options);

    std::string getHelpString() const;

private:
    CommandResult handleCreate(const CommandLineOptions& options);
    CommandResult handleExtract(const CommandLineOptions& options);
    CommandResult handleList(const CommandLineOptions& options);
    CommandResult handleVerify(const CommandLineOptions& options);
    CommandResult handleRepair(const CommandLineOptions& options);
    CommandResult handleBenchmark(const CommandLineOptions& options);
};

} // namespace nebula::cli
