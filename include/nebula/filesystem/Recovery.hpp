#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include "JournalManager.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <system_error>
#include <functional>

namespace nebula {
namespace filesystem {

/// Recovery mode strategy.
enum class RecoveryStrategy : uint8_t {
    Automatic   = 0x00,  ///< Auto-apply all journal entries
    Manual      = 0x01,  ///< Return entries for manual review
    BestEffort  = 0x02   ///< Apply what can be applied, skip corrupt entries
};

/// Recovery configuration.
struct RecoveryConfig {
    RecoveryStrategy strategy  = RecoveryStrategy::Automatic;
    bool validateChecksums     = true;
    bool createBackup          = false;  ///< Backup archive before recovery
    std::string backupPath;              ///< Path for backup
    ProgressCallback progressCb = nullptr;
};

/// Result of a recovery operation.
struct RecoveryResult {
    bool success           = false;
    size_t entriesReplayed = 0;
    size_t entriesSkipped  = 0;
    size_t entriesCorrupt  = 0;
    std::vector<ParseError> errors;
    std::string summary;
};

/// Handles archive recovery from journal logs.
///
/// Analyzes the journal to determine the state of the archive
/// and replays operations to bring it to a consistent state.
class Recovery {
public:
    explicit Recovery(RecoveryConfig config = {});
    ~Recovery() noexcept = default;

    /// Move-only
    Recovery(Recovery&& other) noexcept;
    Recovery& operator=(Recovery&& other) noexcept;
    Recovery(const Recovery&) = delete;
    Recovery& operator=(const Recovery&) = delete;

    /// Analyze the journal and determine recovery needs.
    [[nodiscard]] RecoveryResult analyze(const JournalManager& journal);

    /// Perform recovery.
    [[nodiscard]] RecoveryResult recover(JournalManager& journal,
                                           std::function<std::error_code(const JournalEntry&)> applyEntry);

    /// Quick check if recovery is needed.
    [[nodiscard]] bool needsRecovery(const JournalManager& journal) const noexcept;

    /// Estimate recovery time.
    [[nodiscard]] std::chrono::seconds estimateRecoveryTime(const JournalManager& journal) const;

private:
    RecoveryConfig config_;

    [[nodiscard]] bool isCheckpointConsistent(const std::deque<JournalEntry>& entries) const;
    [[nodiscard]] std::vector<JournalEntry> findIncompleteTransactions(
        const std::deque<JournalEntry>& entries) const;
};

} // namespace filesystem
} // namespace nebula
