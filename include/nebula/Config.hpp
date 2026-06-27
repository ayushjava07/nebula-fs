#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <string_view>

namespace nebula {

/// Maximum supported archive version
constexpr uint16_t kArchiveVersionMajor = 1;
constexpr uint16_t kArchiveVersionMinor = 0;

/// Magic bytes identifying a NebulaFS archive
constexpr std::string_view kArchiveMagic = "NBF\x01";

/// Default compression block size (64 KiB)
constexpr size_t kDefaultBlockSize = 65536;

/// Maximum chunk size for deduplication (1 MiB)
constexpr size_t kMaxChunkSize = 1048576;

/// Minimum chunk size for deduplication (4 KiB)
constexpr size_t kMinChunkSize = 4096;

/// Default hash table size
constexpr size_t kDefaultHashTableSize = 65536;

/// B-tree default order
constexpr size_t kBTreeDefaultOrder = 128;

/// Journal entry size limit (512 KiB)
constexpr size_t kMaxJournalEntrySize = 524288;

/// Maximum number of nested directories
constexpr uint16_t kMaxDirectoryDepth = 255;

/// Maximum filename length
constexpr uint16_t kMaxFileNameLength = 4096;

/// Default buffer size for streaming operations
constexpr size_t kDefaultBufferSize = 16384;

/// Checksum length in bytes (SHA-256)
constexpr size_t kChecksumLength = 32;

/// AES-256-GCM key length
constexpr size_t kAESKeyLength = 32;

/// AES-GCM IV length
constexpr size_t kAESIVLength = 12;

/// AES-GCM tag length
constexpr size_t kAESTagLength = 16;

/// Size of a block address in the chunk table
constexpr size_t kBlockAddressSize = sizeof(uint64_t);

/// Size of a block length field
constexpr size_t kBlockLengthSize = sizeof(uint64_t);

/// Maximum number of entries in a single archive
constexpr uint64_t kMaxArchiveEntries = 1ULL << 48;

/// Default timeout for network operations
constexpr auto kDefaultNetworkTimeout = std::chrono::seconds(30);

/// I/O buffer size for archive reading/writing
constexpr size_t kIOBufferSize = 262144;

/// Number of B-tree nodes to cache
constexpr size_t kBTreeCacheSize = 1024;

/// Maximum size of a single metadata entry
constexpr size_t kMaxMetadataSize = 1048576;

/// Version string for the library
constexpr std::string_view kLibraryVersion = "1.0.0";

} // namespace nebula
