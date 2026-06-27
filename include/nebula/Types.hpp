#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <optional>
#include <variant>
#include <memory>
#include <chrono>
#include <functional>

namespace nebula {

/// Unique identifier for an entry within an archive
using EntryID = uint64_t;

/// Size type for archive offsets
using Offset = uint64_t;

/// Size type for lengths
using Length = uint64_t;

/// Checksum value (SHA-256)
using ChecksumValue = std::array<uint8_t, 32>;

/// 128-bit UUID
struct UUID {
    uint64_t high;
    uint64_t low;

    bool operator==(const UUID& other) const noexcept {
        return high == other.high && low == other.low;
    }
    bool operator!=(const UUID& other) const noexcept {
        return !(*this == other);
    }
    bool operator<(const UUID& other) const noexcept {
        return high < other.high || (high == other.high && low < other.low);
    }
};

/// Access permissions (Unix-style)
struct Permissions {
    bool ownerRead   : 1 = true;
    bool ownerWrite  : 1 = true;
    bool ownerExec   : 1 = false;
    bool groupRead   : 1 = true;
    bool groupWrite  : 1 = false;
    bool groupExec   : 1 = false;
    bool otherRead   : 1 = true;
    bool otherWrite  : 1 = false;
    bool otherExec   : 1 = false;

    [[nodiscard]] uint16_t toUnixMode() const noexcept;
    static Permissions fromUnixMode(uint16_t mode) noexcept;
};

inline uint16_t Permissions::toUnixMode() const noexcept {
    uint16_t mode = 0;
    if (ownerRead)  mode |= 0400;
    if (ownerWrite) mode |= 0200;
    if (ownerExec)  mode |= 0100;
    if (groupRead)  mode |= 0040;
    if (groupWrite) mode |= 0020;
    if (groupExec)  mode |= 0010;
    if (otherRead)  mode |= 0004;
    if (otherWrite) mode |= 0002;
    if (otherExec)  mode |= 0001;
    return mode;
}

inline Permissions Permissions::fromUnixMode(uint16_t mode) noexcept {
    Permissions p;
    p.ownerRead  = (mode & 0400) != 0;
    p.ownerWrite = (mode & 0200) != 0;
    p.ownerExec  = (mode & 0100) != 0;
    p.groupRead  = (mode & 0040) != 0;
    p.groupWrite = (mode & 0020) != 0;
    p.groupExec  = (mode & 0010) != 0;
    p.otherRead  = (mode & 0004) != 0;
    p.otherWrite = (mode & 0002) != 0;
    p.otherExec  = (mode & 0001) != 0;
    return p;
}

/// Timestamp with nanosecond precision
struct Timestamp {
    int64_t seconds;     ///< Seconds since Unix epoch
    uint32_t nanos = 0;  ///< Nanosecond fraction

    [[nodiscard]] static Timestamp now() noexcept;
    [[nodiscard]] std::chrono::system_clock::time_point toTimePoint() const noexcept;
    static Timestamp fromTimePoint(std::chrono::system_clock::time_point tp) noexcept;
};

inline Timestamp Timestamp::now() noexcept {
    auto tp = std::chrono::system_clock::now();
    return fromTimePoint(tp);
}

inline std::chrono::system_clock::time_point Timestamp::toTimePoint() const noexcept {
    auto dur = std::chrono::seconds(seconds) + std::chrono::nanoseconds(nanos);
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(dur));
}

inline Timestamp Timestamp::fromTimePoint(std::chrono::system_clock::time_point tp) noexcept {
    auto dur = tp.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(dur - secs);
    return Timestamp{static_cast<int64_t>(secs.count()), static_cast<uint32_t>(nanos.count())};
}

/// Entry types stored in the archive
enum class EntryType : uint8_t {
    File       = 0x01,
    Directory  = 0x02,
    Symlink    = 0x03,
    Hardlink   = 0x04,
    Block      = 0x05,  ///< Binary blob
    Reserved   = 0xFF
};

/// Compression algorithm identifiers
enum class CompressionAlgorithm : uint8_t {
    None   = 0x00,
    LZ4    = 0x01,
    Zlib   = 0x02,
    Zstd   = 0x03
};

/// Encryption algorithm identifiers
enum class EncryptionAlgorithm : uint8_t {
    None          = 0x00,
    AES256GCM     = 0x01,
    ChaCha20Poly  = 0x02
};

/// Hash algorithm for checksums
enum class HashAlgorithm : uint8_t {
    CRC32   = 0x00,
    SHA256  = 0x01,
    Blake3  = 0x02
};

/// Deduplication strategy
enum class DedupStrategy : uint8_t {
    None     = 0x00,
    Fixed    = 0x01,  ///< Fixed-size chunking
    Content  = 0x02   ///< Content-defined chunking (CDC)
};

/// Archive open mode
enum class OpenMode : uint8_t {
    Read        = 0x01,
    Write       = 0x02,
    Append      = 0x03,
    Recovery    = 0x04
};

/// Parser state machine states
enum class ParserState : uint8_t {
    Init            = 0x00,
    Header          = 0x01,
    Metadata        = 0x02,
    DirectoryTree   = 0x03,
    IndexTable      = 0x04,
    ChunkTable      = 0x05,
    CompressedBlocks= 0x06,
    ObjectRecon     = 0x07,
    Complete        = 0x08,
    Error           = 0xFF
};

/// Severity of parse errors
enum class ErrorSeverity : uint8_t {
    Warning     = 0x00,  ///< Non-fatal, can continue
    Recoverable = 0x01,  ///< Section can be skipped
    Fatal       = 0x02   ///< Archive is corrupt
};

/// Journal entry types
enum class JournalEntryType : uint8_t {
    BeginCheckpoint  = 0x01,
    EndCheckpoint    = 0x02,
    CreateEntry      = 0x03,
    UpdateEntry      = 0x04,
    DeleteEntry      = 0x05,
    RenameEntry      = 0x06,
    WriteBlock       = 0x07,
    Commit           = 0x08,
    Abort            = 0x09
};

/// Flags for archive entries
enum class EntryFlags : uint32_t {
    None          = 0x00000000,
    Compressed    = 0x00000001,
    Encrypted     = 0x00000002,
    Deduplicated  = 0x00000004,
    Sparse        = 0x00000008,
    Immutable     = 0x00000010,
    Hidden        = 0x00000020,
    System        = 0x00000040,
    Archived      = 0x00000080,
    Temporary     = 0x00000100,
    OffsetFlag    = 0x00000200
};

constexpr EntryFlags operator|(EntryFlags a, EntryFlags b) noexcept {
    return static_cast<EntryFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

constexpr EntryFlags operator&(EntryFlags a, EntryFlags b) noexcept {
    return static_cast<EntryFlags>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
}

constexpr bool hasFlag(EntryFlags flags, EntryFlags test) noexcept {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
}

/// A single entry in the archive
struct ArchiveEntry {
    EntryID       id            = 0;
    EntryType     type          = EntryType::File;
    EntryFlags    flags         = EntryFlags::None;
    std::string   name;
    std::string   path;
    Offset        offset        = 0;
    Length        originalSize  = 0;
    Length        storedSize    = 0;
    Timestamp     createdAt;
    Timestamp     modifiedAt;
    Permissions   permissions;
    uint32_t      ownerUid      = 0;
    uint32_t      ownerGid      = 0;
    ChecksumValue checksum{};
    uint32_t      refCount      = 1;

    /// The compression algorithm used (if compressed)
    CompressionAlgorithm compression = CompressionAlgorithm::None;
    /// The encryption algorithm used (if encrypted)
    EncryptionAlgorithm encryption = EncryptionAlgorithm::None;
};

/// Rich error information returned by parsers
struct ParseError {
    ErrorSeverity severity     = ErrorSeverity::Fatal;
    ParserState   state        = ParserState::Init;
    uint64_t      offset       = 0;    ///< Byte offset where error occurred
    std::string   message;
    int           systemError  = 0;    ///< errno if applicable
    std::optional<uint64_t> expectedSize;
    std::optional<uint64_t> actualSize;

    [[nodiscard]] std::string toString() const;
};

inline std::string ParseError::toString() const {
    std::ostringstream oss;
    oss << "ParseError [";
    switch (severity) {
        case ErrorSeverity::Warning:   oss << "WARNING"; break;
        case ErrorSeverity::Recoverable: oss << "RECOVERABLE"; break;
        case ErrorSeverity::Fatal:     oss << "FATAL"; break;
    }
    oss << "] at state=" << static_cast<int>(state)
        << " offset=" << offset;
    if (!message.empty()) oss << " \"" << message << "\"";
    if (systemError) oss << " errno=" << systemError;
    if (expectedSize) oss << " expected=" << *expectedSize;
    if (actualSize) oss << " actual=" << *actualSize;
    return oss.str();
}

/// Result type that can hold either a value or an error
template<typename T>
using Result = std::variant<T, ParseError>;

/// Block descriptor in the chunk table
struct ChunkDescriptor {
    ChecksumValue  hash{};         ///< Content hash (for dedup verification)
    Offset         offset   = 0;   ///< Offset in compressed blocks section
    Length         compressedSize = 0;
    Length         originalSize   = 0;
    CompressionAlgorithm compression = CompressionAlgorithm::None;
    bool           encrypted       = false;
};

/// B-tree node handle
struct IndexEntry {
    EntryID       entryId   = 0;
    Offset        offset    = 0;
    Length        size      = 0;
    ChecksumValue checksum{};
};

/// Directory tree node
struct DirectoryNode {
    std::string                  name;
    EntryID                      id = 0;
    EntryID                      parentId = 0;
    std::vector<DirectoryNode>   children;
    std::vector<EntryID>         files;
    Timestamp                    modifiedAt;
    Permissions                  permissions;
};

/// Journal entry for recovery
struct JournalEntry {
    JournalEntryType type        = JournalEntryType::BeginCheckpoint;
    uint64_t         sequence    = 0;
    Timestamp        timestamp;
    std::vector<uint8_t> data;
    ChecksumValue    checksum{};
};

/// Progress callback for long operations
using ProgressCallback = std::function<void(uint64_t current, uint64_t total, std::string_view stage)>;

} // namespace nebula
