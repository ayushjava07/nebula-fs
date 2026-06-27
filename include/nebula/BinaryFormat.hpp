#pragma once

#include "Types.hpp"
#include "Config.hpp"
#include "Error.hpp"
#include <array>
#include <cstring>
#include <type_traits>

namespace nebula {
namespace format {

#pragma pack(push, 1)

/// NebulaFS Binary Format definitions.
///
/// Overall archive layout:
///
/// +-------------------+ 0x00
/// | Magic (4 bytes)   | "NBF\x01"
/// +-------------------+
/// | Header (var)      | ArchiveHeader struct
/// +-------------------+
/// | Metadata (var)    | Serialized metadata key-value pairs
/// +-------------------+
/// | Directory Tree    | Hierarchical directory structure
/// +-------------------+
/// | Index Table       | B-tree index entries
/// +-------------------+
/// | Chunk Table       | Chunk descriptors
/// +-------------------+
/// | Compressed Blocks | Actual data blocks (may be compressed/encrypted)
/// +-------------------+
/// | Journal           | Transaction journal (optional)
/// +-------------------+

#pragma pack(push, 1)

/// Archive header stored at the beginning of every NebulaFS archive.
struct ArchiveHeader {
    /// Magic identifier: "NBF\x01"
    uint8_t  magic[4] = {'N', 'B', 'F', 0x01};

    /// Major version of the format
    uint16_t versionMajor = kArchiveVersionMajor;

    /// Minor version of the format
    uint16_t versionMinor = kArchiveVersionMinor;

    /// Format flags
    uint16_t flags = 0;

    /// Total size of the archive in bytes
    uint64_t archiveSize = 0;

    /// Offset to the metadata section
    uint64_t metadataOffset = 0;

    /// Offset to the directory tree section
    uint64_t directoryOffset = 0;

    /// Offset to the index table section
    uint64_t indexOffset = 0;

    /// Offset to the chunk table section
    uint64_t chunkOffset = 0;

    /// Offset to the compressed blocks section
    uint64_t blocksOffset = 0;

    /// Offset to the journal section (0 if no journal)
    uint64_t journalOffset = 0;

    /// Number of entries in the archive
    uint64_t entryCount = 0;

    /// Size of the metadata section
    uint64_t metadataSize = 0;

    /// Size of the directory tree section
    uint64_t directorySize = 0;

    /// Size of the index table section
    uint64_t indexSize = 0;

    /// Size of the chunk table section
    uint64_t chunkSize = 0;

    /// Size of the compressed blocks section
    uint64_t blocksSize = 0;

    /// Size of the journal section
    uint64_t journalSize = 0;

    /// CRC32 checksum of the header (starting after checksum field)
    uint32_t headerChecksum = 0;

    /// SHA-256 checksum of the entire archive (0 if not yet finalized)
    uint8_t  archiveChecksum[kChecksumLength] = {};

    /// Check if the magic bytes are valid
    [[nodiscard]] bool isValidMagic() const noexcept {
        return magic[0] == 'N' && magic[1] == 'B' && magic[2] == 'F' && magic[3] == 0x01;
    }

    /// Check if the version is supported
    [[nodiscard]] bool isVersionSupported() const noexcept {
        return versionMajor == kArchiveVersionMajor && versionMinor <= kArchiveVersionMinor;
    }

    /// Compute the total header size
    static constexpr size_t headerSize() noexcept {
        return sizeof(ArchiveHeader);
    }
};

static_assert(sizeof(ArchiveHeader) == 158,
    "ArchiveHeader must be packed to exactly 158 bytes");

/// Metadata entry in the metadata section
struct MetadataEntry {
    /// Length of the key string (varint encoded)
    uint64_t keyLength = 0;

    /// Length of the value data (varint encoded)
    uint64_t valueLength = 0;

    /// Key string follows immediately
    /// Value data follows immediately after key
};

/// Directory tree entry (serialized)
struct DirectoryEntry {
    /// Entry ID (varint encoded)
    uint64_t entryId = 0;

    /// Parent entry ID (varint encoded, 0 = root)
    uint64_t parentId = 0;

    /// Entry type
    EntryType type = EntryType::File;

    /// Entry flags
    uint32_t flags = 0;

    /// Length of the entry name (varint encoded)
    uint64_t nameLength = 0;

    /// Name string follows immediately
};

/// Index entry in the B-tree index table
struct IndexEntryHeader {
    /// Entry ID (varint encoded)
    uint64_t entryId = 0;

    /// Offset in the archive where the entry data begins
    uint64_t offset = 0;

    /// Size of the entry data
    uint64_t size = 0;

    /// SHA-256 checksum of the entry
    uint8_t checksum[kChecksumLength] = {};

    /// Length of the entry path (varint encoded)
    uint64_t pathLength = 0;
    /// Path string follows
};

/// Chunk descriptor in the chunk table
struct ChunkDescriptorHeader {
    /// Offset in the blocks section
    uint64_t offset = 0;

    /// Compressed size
    uint64_t compressedSize = 0;

    /// Original (uncompressed) size
    uint64_t originalSize = 0;

    /// Hash of the uncompressed content
    uint8_t contentHash[kChecksumLength] = {};

    /// Compression algorithm used
    CompressionAlgorithm compression = CompressionAlgorithm::None;

    /// Encryption algorithm used
    EncryptionAlgorithm encryption = EncryptionAlgorithm::None;

    /// Reserved for future use
    uint8_t reserved[6] = {};
};

/// Compressed block header
struct BlockHeader {
    /// Magic bytes for block alignment: "NB"
    uint8_t sync[2] = {'N', 'B'};

    /// Compressed size of this block (0 if uncompressed)
    uint64_t compressedSize = 0;

    /// Original (uncompressed) size
    uint64_t originalSize = 0;

    /// Compression algorithm
    CompressionAlgorithm compression = CompressionAlgorithm::None;

    /// Encryption algorithm
    EncryptionAlgorithm encryption = EncryptionAlgorithm::None;

    /// Whether this block is encrypted
    uint8_t encrypted = 0;

    /// Reserved
    uint8_t reserved[4] = {};
};

static_assert(sizeof(BlockHeader) == 25,
    "BlockHeader must be packed to 25 bytes");

/// Journal entry header
struct JournalEntryHeader {
    /// Magic: "JL"
    uint8_t magic[2] = {'J', 'L'};

    /// Entry type
    JournalEntryType type = JournalEntryType::BeginCheckpoint;

    /// Sequence number (monotonically increasing)
    uint64_t sequence = 0;

    /// Timestamp (seconds since epoch)
    int64_t timestampSeconds = 0;

    /// Timestamp nanoseconds
    uint32_t timestampNanos = 0;

    /// Size of the journal entry data
    uint64_t dataSize = 0;

    /// CRC32 of the entry (header + data)
    uint32_t entryChecksum = 0;
};

#pragma pack(pop)

/// Variable-length integer encoding helpers
struct VarInt {
    /// Maximum bytes needed for a 64-bit value
    static constexpr size_t kMaxBytes = 10;

    /// Encode a uint64_t into a byte buffer.
    /// Returns the number of bytes written.
    static size_t encode(uint64_t value, uint8_t* buf, size_t bufSize) noexcept;

    /// Decode a varint from a byte buffer.
    /// Returns the decoded value and the number of bytes consumed.
    struct DecodeResult { uint64_t value; size_t consumed; bool valid; };
    static DecodeResult decode(const uint8_t* buf, size_t bufSize) noexcept;

    /// Encoded size of a value without writing
    static size_t encodedSize(uint64_t value) noexcept;
};

#pragma pack(pop)

/// Flags for the archive header
enum HeaderFlags : uint16_t {
    HeaderNone           = 0x0000,
    HeaderEncrypted      = 0x0001,  ///< Archive uses encryption
    HeaderJournaled      = 0x0002,  ///< Archive has journal
    HeaderRecovery       = 0x0004,  ///< Archive needs recovery
    HeaderFinalized      = 0x0008,  ///< Archive is finalized
    HeaderSparse         = 0x0010,  ///< Archive uses sparse blocks
    HeaderDedup          = 0x0020,  ///< Archive uses deduplication
    HeaderStreaming      = 0x0040,  ///< Archive is being streamed
    HeaderReadOnly       = 0x0080,  ///< Archive is read-only
    HeaderCompressed     = 0x0100,  ///< Default compression enabled
    HeaderMultiVolume    = 0x0200,  ///< Multi-volume archive
};

/// Section identifiers for parser state
enum class SectionType : uint8_t {
    Invalid         = 0x00,
    Header          = 0x01,
    Metadata        = 0x02,
    DirectoryTree   = 0x03,
    IndexTable      = 0x04,
    ChunkTable      = 0x05,
    CompressedBlocks= 0x06,
    Journal         = 0x07
};

} // namespace format

} // namespace nebula
