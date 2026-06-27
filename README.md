# NebulaFS

A lightweight embedded binary archive library. Think tar or zip but with compression, encryption, dedup, crash recovery, and proper random access — all in a single self-contained C++20 library.

I started this because I needed something between "dump everything in a tarball" and "spin up a database." Most binary archive formats are either too simple (tar, cpio) or too tied to a specific use case. NebulaFS tries to hit the middle ground: store thousands of named entries in one file, index them for fast lookup, optionally compress and encrypt, and survive a crash mid-write.

## What it does

- Packs files, blobs, or raw data into a single `.nbf` archive
- Compression: LZ4 (fast), Zlib (balance), Zstd (high ratio)
- Encryption: AES-256-GCM with key derivation
- Content-defined chunking and deduplication
- Crash-safe journaling with recovery
- Random access by path (directory tree) or ID (B-tree + hash table)
- Streaming parser for incremental processing
- Optional checksums everywhere

## Code organization

```
nebula-fs/
├── CMakeLists.txt           # root build, fetches deps
├── .clusterfuzzlite/        # fuzzing CI integration
│   ├── build.sh
│   └── project.yaml
├── fuzz/                    # libFuzzer harnesses + seeds
│   ├── archive_parser_fuzzer.cpp
│   ├── compression_fuzzer.cpp
│   ├── index_fuzzer.cpp
│   ├── journal_fuzzer.cpp
│   ├── metadata_fuzzer.cpp
│   ├── corpus/              # seed files per harness
│   └── dictionary.txt
├── include/nebula/          # public headers
│   ├── archive/             # ArchiveWriter, ArchiveReader
│   ├── compression/         # CompressionEngine, blocks
│   ├── crypto/              # encryption, hashing
│   ├── filesystem/          # DirectoryTree, JournalManager, Recovery
│   ├── index/               # IndexManager, BTree, HashTable
│   ├── metadata/            # MetadataStore, EntryMetadata
│   ├── parser/              # Parser, StreamParser, MetadataParser
│   ├── storage/             # ChunkManager, BlockStorage
│   └── utils/               # Buffer, VarInt, Checksum, MemoryMappedFile
├── src/                     # implementation (mirrors include/ layout)
└── tests/                   # 23 test suites, ~294 tests
```

Each module under `src/` has a corresponding header in `include/nebula/`. The archive layer (`ArchiveWriter`/`ArchiveReader`) is the high-level entry point; everything else (compression, indexing, journaling) lives in separate modules.

## Building

```bash
# dependencies (ubuntu/debian)
sudo apt install build-essential cmake libzstd-dev liblz4-dev \
                 zlib1g-dev libssl-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# run tests
./build/tests/nebula_tests
```

You need CMake 3.22+, a C++20 compiler (g++-13 or clang-14+), and the dev packages for zstd, lz4, zlib, and openssl.

## Fuzzing

```bash
cmake -B build_fuzz \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang \
    -DNEBULA_BUILD_FUZZ=ON

cmake --build build_fuzz -j$(nproc)

# run a specific harness
./build_fuzz/fuzz/archive_parser_fuzzer -max_len=4096 fuzz/corpus/archive_parser_fuzzer/
```

The CMake flag `-DNEBULA_BUILD_FUZZ=ON` enables the 5 libFuzzer harnesses. Each one exercises a different module — archive parsing, compression, index deserialization, journal recovery, and metadata parsing.

## Quick example

```cpp
#include <nebula/archive/ArchiveWriter.hpp>
#include <nebula/archive/ArchiveReader.hpp>

// Write
nebula::archive::ArchiveWriter writer(config);
writer.open("archive.nbf");
writer.addBlob(data, "/entry.bin");
writer.close();

// Read
nebula::archive::ArchiveReader reader;
reader.open("archive.nbf");
auto entries = reader.listEntries();
auto blob = reader.extractEntry("/entry.bin");
```

## Why not just use tar/zip/...

Different tradeoffs. Tar has no index — you scan the whole file to find something. Zip has an index but weak recovery and no built-in dedup. NebulaFS gives you indexed random access, crash-safe journaling, and content-defined chunking in one format. If you don't need those, tar or zip are simpler and more portable.

## What it's not

- Not a filesystem (you can't mount it — though that's on the roadmap)
- Not a streaming format (the whole archive needs to be readable, it's not a tape format)
- Not POSIX-compatible (ownership/permissions are stored but minimal)
