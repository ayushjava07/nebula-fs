#pragma once

#include "../Config.hpp"
#include "../Types.hpp"
#include "../BinaryFormat.hpp"
#include "../utils/Checksum.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <system_error>
#include <deque>

namespace nebula {
namespace filesystem {

/// Journaling mode.
enum class JournalMode : uint8_t {
    Disabled   = 0x00,
    Memory     = 0x01,  ///< Journal in memory only
    File       = 0x02,  ///< Journal to separate file
    Embedded   = 0x03   ///< Journal embedded in archive
};

/// Configuration for the journal manager.
struct JournalConfig {
    JournalMode mode        = JournalMode::Embedded;
    size_t maxEntrySize     = kMaxJournalEntrySize;
    size_t maxJournalSize   = 64 * 1024 * 1024;  ///< 64 MiB max journal
    bool enableChecksums    = true;
    std::string journalPath;  ///< Path for file-mode journal
};

/// Manages the transaction journal for crash recovery.
///
/// The journal records all modifications before they are applied,
/// enabling recovery from crashes or incomplete writes.
/// Uses a write-ahead logging (WAL) approach.
class JournalManager {
public:
    explicit JournalManager(JournalConfig config = {});
    ~JournalManager() noexcept;

    /// Move-only
    JournalManager(JournalManager&& other) noexcept;
    JournalManager& operator=(JournalManager&& other) noexcept;
    JournalManager(const JournalManager&) = delete;
    JournalManager& operator=(const JournalManager&) = delete;

    /// Begin a new checkpoint.
    [[nodiscard]] std::error_code beginCheckpoint();

    /// End the current checkpoint.
    [[nodiscard]] std::error_code endCheckpoint();

    /// Log a journal entry.
    [[nodiscard]] std::error_code log(JournalEntry entry);

    /// Log a create entry operation.
    [[nodiscard]] std::error_code logCreate(EntryID id, std::span<const uint8_t> data);

    /// Log an update entry operation.
    [[nodiscard]] std::error_code logUpdate(EntryID id, std::span<const uint8_t> data);

    /// Log a delete entry operation.
    [[nodiscard]] std::error_code logDelete(EntryID id);

    /// Log a write block operation.
    [[nodiscard]] std::error_code logWriteBlock(const ChunkDescriptor& chunk);

    /// Commit the current transaction.
    [[nodiscard]] std::error_code commit();

    /// Abort the current transaction.
    [[nodiscard]] std::error_code abort();

    /// Recover from journal (replay all entries).
    [[nodiscard]] std::error_code recover();

    /// Replay a single journal entry.
    [[nodiscard]] std::error_code replayEntry(const JournalEntry& entry);

    /// Check if recovery is needed.
    [[nodiscard]] bool needsRecovery() const noexcept;

    /// Check if there are uncommitted entries.
    [[nodiscard]] bool hasUncommitted() const noexcept;

    /// Get journal entries.
    [[nodiscard]] const std::deque<JournalEntry>& entries() const noexcept { return entries_; }

    /// Get the number of entries.
    [[nodiscard]] size_t entryCount() const noexcept { return entries_.size(); }

    /// Clear the journal.
    void clear() noexcept;

    /// Flush the journal to disk (for file mode).
    [[nodiscard]] std::error_code flush();

    /// Serialize journal to binary.
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialize journal from binary.
    [[nodiscard]] std::error_code deserialize(std::span<const uint8_t> data);

    /// Verify journal integrity.
    [[nodiscard]] bool verifyIntegrity() const;

    /// Get current sequence number.
    [[nodiscard]] uint64_t currentSequence() const noexcept { return sequence_; }

private:
    JournalConfig config_;
    std::deque<JournalEntry> entries_;
    uint64_t sequence_ = 0;
    uint64_t totalJournalSize_ = 0;
    bool inCheckpoint_ = false;
    bool needsRecovery_ = false;
    std::unique_ptr<utils::ChecksumEngine> checksum_;
    int journalFd_ = -1;  ///< For file-mode journal

    JournalEntry createEntry(JournalEntryType type, std::vector<uint8_t> data);
    [[nodiscard]] std::error_code writeToFile(const JournalEntry& entry);
    [[nodiscard]] bool isJournalFull() const noexcept;
};

} // namespace filesystem
} // namespace nebula
