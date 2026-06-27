# NebulaFS Architecture

## Overview

NebulaFS is a lightweight embedded binary object database and archive format.
It stores thousands of files inside a single binary container with support for
compression, encryption, deduplication, and journaling.

## Binary Format

The NebulaFS archive format (NBF) is a structured binary container with the
following sections:

```
+-------------------+  0x00
| Magic (4 bytes)   |  "NBF\x01"
+-------------------+
| Header (176 bytes)|  ArchiveHeader struct
+-------------------+
| Metadata (var)    |  Key-value metadata
+-------------------+
| Directory Tree    |  Hierarchical directory structure
+-------------------+
| Index Table       |  B-tree and hash table indices
+-------------------+
| Chunk Table       |  Chunk descriptors for deduplication
+-------------------+
| Compressed Blocks |  Actual data blocks (compressed/encrypted)
+-------------------+
| Journal (opt.)    |  Transaction journal for crash recovery
+-------------------+
```

### Header Format

The header (176 bytes packed) contains:
- Magic bytes (4): `NBF\x01`
- Version (4): major and minor version numbers
- Flags (2): encryption, journaling, dedup, etc.
- Archive size (8): total archive size in bytes
- Section offsets (6x8): pointers to each major section
- Section sizes (6x8): sizes of each section
- Entry count (8): number of entries
- Header checksum (4): CRC32 of header (excluding magic)
- Archive checksum (32): SHA-256 of entire archive

### Variable-Length Integers

All integer fields in sections use unsigned LEB128-like encoding where
each byte uses 7 bits for data with the MSB as a continuation flag.

## Parser Pipeline

The parser walks through sections sequentially:

```
Input Data
    |
    v
[Header] --> Validate magic, version, checksums
    |
    v
[Metadata] --> Parse key-value pairs with varint lengths
    |
    v
[Directory Tree] --> Reconstruct hierarchical node tree
    |
    v
[Index Table] --> Load B-tree entries and hash table
    |
    v
[Chunk Table] --> Load chunk descriptors
    |
    v
[Compressed Blocks] --> Parse block headers and data
    |
    v
[Object Reconstruction] --> Assemble entries from blocks
```

The parser is designed for graceful degradation:
- Non-fatal errors produce warnings without aborting
- Corrupt sections can be skipped if recoverable
- Rich error objects capture location, severity, and context
- Streaming parser supports incremental processing

## Class Diagram

```
+-------------------+       +--------------------+
|   ArchiveWriter   |       |   ArchiveReader    |
+-------------------+       +--------------------+
| - Compression     |       | - Parser           |
| - Encryption      |       | - IndexManager     |
| - ChunkManager    |       | - MetadataStore    |
| - IndexManager    |       | - DirectoryTree    |
| - MetadataStore   |       | - ChunkManager     |
| - DirectoryTree   |       | - Compression      |
| - JournalManager  |       | - Decryption       |
+-------------------+       +--------------------+
         |                           |
         v                           v
+-------------------+       +--------------------+
|  Parser / Stream  |       |   Parser / Stream  |
|     Parser        |<------|     Parser         |
+-------------------+       +--------------------+
         |                           ^
         v                           |
+-------------------+               |
|  BinaryFormat     |---------------+
|  structs +        |
|  serialization    |
+-------------------+

+-------------------+  +------------------+  +------------------+
| CompressionEngine |  | EncryptionEngine |  | ChecksumEngine   |
+-------------------+  +------------------+  +------------------+
| LZ4, Zlib, Zstd   |  | AES-256-GCM      |  | CRC32, SHA-256   |
| Block compression |  | Key derivation   |  | Streaming hash   |
+-------------------+  +------------------+  +------------------+

+-------------------+  +------------------+  +------------------+
|   IndexManager    |  |   ChunkManager   |  |  JournalManager  |
+-------------------+  +------------------+  +------------------+
| B-Tree index      |  | CDC chunking     |  | Write-ahead log  |
| Hash table cache  |  | Dedup detection  |  | Checkpointing    |
| Path resolution   |  | Hash addressing  |  | Recovery         |
+-------------------+  +------------------+  +------------------+

+-------------------+  +------------------+
|  MetadataStore    |  |   DirectoryTree  |
+-------------------+  +------------------+
| Key-value store   |  | Nested hierarchy|
| Binary values     |  | Path resolution |
+-------------------+  +------------------+

+-------------------+  +------------------+
|    Buffer         |  | MemoryMappedFile |
+-------------------+  +------------------+
| Dynamic storage   |  | Zero-copy I/O   |
| Serialization     |  | Large file supp.|
+-------------------+  +------------------+
```

## Module Responsibilities

### archive/
- **ArchiveWriter**: High-level archive creation orchestrator
- **ArchiveReader**: High-level archive reading and extraction
- **ArchiveHeader**: Header serialization, validation, checksumming

### parser/
- **Parser**: Full archive parser (all sections)
- **StreamParser**: Incremental streaming parser
- **MetadataParser**: Specialized metadata section parser

### compression/
- **CompressionEngine**: Multi-algorithm compress/decompress
- **CompressionBlock**: Individual block management with lazy decompression

### crypto/
- **EncryptionEngine**: AES-256-GCM encryption with key derivation
- **HashEngine**: SHA-256 and BLAKE3 hashing

### storage/
- **ChunkManager**: Content-defined chunking and deduplication
- **BlockStorage**: Physical block read/write with verification

### index/
- **IndexManager**: Entry index with ID/path lookup
- **BTree**: B-tree index template for ordered access
- **HashTable**: Robin Hood hash table for fast path lookup

### metadata/
- **MetadataStore**: Archive-level key-value metadata
- **EntryMetadata**: Per-entry extended attributes and tags

### filesystem/
- **DirectoryTree**: Hierarchical directory structure
- **FileResolver**: Path resolution and glob matching
- **JournalManager**: Write-ahead logging for crash recovery
- **Recovery**: Journal analysis and recovery execution

### network/
- **ArchiveServer**: Simple TCP server for remote archive access

### utils/
- **Buffer**: Dynamic byte buffer for serialization
- **VarInt**: Variable-length integer encoding
- **ChecksumEngine**: CRC32 and SHA-256 computation
- **MemoryMappedFile**: Memory-mapped file I/O

## Data Flow

### Writing an Archive:

1. User calls ArchiveWriter::open()
2. Header is initialized with default values
3. Journal begins a checkpoint transaction
4. Each entry is:
   a. Compressed (if enabled)
   b. Encrypted (if enabled)
   c. Chunked and deduplicated
   d. Indexed in B-tree and hash table
   e. Inserted into directory tree
   f. Written to journal
5. On close():
   a. Metadata section serialized
   b. Directory tree serialized
   c. Index table serialized
   d. Chunk table serialized
   e. Blocks section written
   f. Journal finalized
   g. Header updated with offsets/sizes
   h. Header checksums computed

### Reading an Archive:

1. Parser walks through all sections
2. Header is validated (magic, version, checksums)
3. Metadata section is parsed
4. Directory tree is reconstructed
5. Index table is loaded
6. Chunk table is loaded
7. On entry extraction:
   a. Index lookup finds entry offset/size
   b. Data is read from blocks section
   c. Verified against checksum
   d. Decrypted (if needed)
   e. Decompressed (if needed)

## Error Handling Strategy

- **Result<T> type**: All parse operations return Result<T> variant
- **ParseError**: Rich error objects with severity, location, and context
- **Graceful degradation**: Non-critical errors produce warnings
- **Streaming safety**: StreamParser handles truncated data gracefully
- **Fuzzing resilience**: All parsers tested with libFuzzer

## Future Extension Points

1. **Multi-volume archives**: Extend header with volume tracking
2. **Plugins**: Add compression/encryption plugin interface
3. **Remote backends**: Extend network module for S3, HTTP
4. **Incremental indexing**: Append-only index updates
5. **Snapshot support**: Versioned archive snapshots
6. **Content-defined replication**: CDC-based sync protocol
7. **Wide-column metadata**: Extend metadata store for complex schemas
8. **Filesystem FUSE**: Mount archives as filesystem
9. **Concurrent access**: Reader-writer locks for multi-threaded access
10. **Dedup across archives**: Global deduplication database

## Performance Considerations

- **Memory-mapped I/O**: Large archives read via mmap for zero-copy
- **Lazy decompression**: Blocks decompressed on first access
- **Configurable block size**: Trade-off between compression ratio and random access
- **B-tree caching**: Frequently accessed nodes kept in memory
- **Robin Hood hashing**: Optimized hash table with good cache locality
- **Content-defined chunking**: Better dedup for files with insertions/deletions
