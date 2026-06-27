#include "nebula/filesystem/Recovery.hpp"
#include <algorithm>
#include <sstream>

namespace nebula {
namespace filesystem {

Recovery::Recovery(RecoveryConfig config) : config_(config) {}

Recovery::Recovery(Recovery&& other) noexcept : config_(other.config_) {}

Recovery& Recovery::operator=(Recovery&& other) noexcept {
    if (this != &other) {
        config_ = other.config_;
    }
    return *this;
}

RecoveryResult Recovery::analyze(const JournalManager& journal) {
    RecoveryResult result;
    const auto& entries = journal.entries();

    if (entries.empty()) {
        result.success = true;
        result.summary = "No journal entries to analyze";
        return result;
    }

    result.success = isCheckpointConsistent(entries);

    auto incomplete = findIncompleteTransactions(entries);
    result.entriesSkipped = incomplete.size();

    for (const auto& entry : entries) {
        if (entry.type == JournalEntryType::BeginCheckpoint ||
            entry.type == JournalEntryType::EndCheckpoint) {
            continue;
        }
        ++result.entriesReplayed;
    }

    std::ostringstream oss;
    oss << "Analysis complete: " << result.entriesReplayed << " entries, "
        << result.entriesSkipped << " skipped, "
        << result.entriesCorrupt << " corrupt. "
        << (result.success ? "Consistent" : "Inconsistent");
    result.summary = oss.str();

    return result;
}

RecoveryResult Recovery::recover(JournalManager& journal,
                                   std::function<std::error_code(const JournalEntry&)> applyEntry) {
    RecoveryResult result;

    auto analysis = analyze(journal);
    result.entriesSkipped = analysis.entriesSkipped;
    result.entriesCorrupt = analysis.entriesCorrupt;

    const auto& entries = journal.entries();
    bool inActiveTransaction = false;

    for (const auto& entry : entries) {
        if (config_.validateChecksums && !journal.verifyIntegrity()) {
            ++result.entriesCorrupt;
            if (config_.strategy == RecoveryStrategy::BestEffort) {
                continue;
            } else if (config_.strategy == RecoveryStrategy::Manual) {
                result.errors.push_back(
                    toParseError(ErrorCode::JournalCorrupt, ParserState::Init,
                                entry.sequence, "corrupt journal entry"));
                continue;
            } else {
                result.success = false;
                result.summary = "Recovery failed: corrupt journal";
                return result;
            }
        }

        if (entry.type == JournalEntryType::BeginCheckpoint) {
            inActiveTransaction = true;
            continue;
        }

        if (entry.type == JournalEntryType::EndCheckpoint ||
            entry.type == JournalEntryType::Commit) {
            inActiveTransaction = false;
            continue;
        }

        if (entry.type == JournalEntryType::Abort) {
            inActiveTransaction = false;
            continue;
        }

        if (inActiveTransaction) {
            auto ec = applyEntry(entry);
            if (ec) {
                if (config_.strategy == RecoveryStrategy::BestEffort) {
                    ++result.entriesSkipped;
                    continue;
                }
                result.errors.push_back(
                    toParseError(ErrorCode::IOError, ParserState::Init,
                                entry.sequence, ec.message()));
                ++result.entriesCorrupt;
                continue;
            }
        }

        ++result.entriesReplayed;
    }

    if (config_.strategy == RecoveryStrategy::Automatic) {
        journal.clear();
    }

    result.success = true;
    std::ostringstream oss;
    oss << "Recovery complete: " << result.entriesReplayed << " replayed, "
        << result.entriesSkipped << " skipped, "
        << result.entriesCorrupt << " corrupt";
    result.summary = oss.str();

    return result;
}

bool Recovery::needsRecovery(const JournalManager& journal) const noexcept {
    return journal.needsRecovery() || journal.hasUncommitted();
}

std::chrono::seconds Recovery::estimateRecoveryTime(const JournalManager& journal) const {
    size_t entryCount = journal.entryCount();
    if (entryCount == 0) return std::chrono::seconds(0);
    constexpr size_t entriesPerSecond = 10000;
    return std::chrono::seconds(entryCount / entriesPerSecond + 1);
}

bool Recovery::isCheckpointConsistent(const std::deque<JournalEntry>& entries) const {
    int checkpointDepth = 0;
    bool hasCommit = false;

    for (const auto& entry : entries) {
        switch (entry.type) {
            case JournalEntryType::BeginCheckpoint:
                ++checkpointDepth;
                break;
            case JournalEntryType::EndCheckpoint:
                --checkpointDepth;
                break;
            case JournalEntryType::Commit:
                hasCommit = true;
                break;
            case JournalEntryType::Abort:
                hasCommit = true;
                break;
            default:
                break;
        }
    }

    return checkpointDepth == 0 && hasCommit;
}

std::vector<JournalEntry> Recovery::findIncompleteTransactions(
    const std::deque<JournalEntry>& entries) const {
    std::vector<JournalEntry> incomplete;
    int depth = 0;

    for (const auto& entry : entries) {
        if (entry.type == JournalEntryType::BeginCheckpoint) {
            ++depth;
        } else if (entry.type == JournalEntryType::EndCheckpoint ||
                   entry.type == JournalEntryType::Commit ||
                   entry.type == JournalEntryType::Abort) {
            if (depth > 0) --depth;
        } else if (depth > 0) {
            incomplete.push_back(entry);
        }
    }

    return incomplete;
}

} // namespace filesystem
} // namespace nebula
