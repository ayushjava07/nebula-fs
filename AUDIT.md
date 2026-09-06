# NebulaFS Repository Audit

This audit was conducted in accordance with **Phase 0** of the *Enhancement Prompt — Existing Repository → Industrial/Benchmark Level*.

---

## 1. Executive Summary & Repo Identity

- **Repository**: `NebulaFS` (`github.com/ayushjava07/nebula-fs`)
- **Domain**: Embedded binary object database and high-performance archive container format (`.nbf`), featuring content-defined chunking, deduplication, compression (LZ4/Zlib/Zstd), authenticated encryption (AES-256-GCM), crash-safe journaling, and B-Tree/HashTable random-access indices.
- **Primary Language**: C++20 (`-std=c++20`) with C fallback dependencies.
- **Build System**: CMake 3.16+ generating Makefiles/Ninja.
- **Testing Framework**: GoogleTest v1.14.0 (via `FetchContent`).
- **Fuzzing Infrastructure**: LibFuzzer / ClusterFuzzLite with per-harness seed corpora.

---

## 2. Line of Code (LOC) Analysis

Measured using `cloc v2.10`, excluding third-party bundled dependencies, subprojects, and version control metadata:

### Production Source Code (`include/` & `src/`)
| Component | Files | Blank Lines | Comment Lines | Code Lines (LOC) |
|---|---|---|---|---|
| C++ Implementation (`src/**/*.cpp`) | 33 | 1,147 | 97 | 5,611 |
| C/C++ Headers (`include/nebula/**/*.hpp`) | 37 | 798 | 675 | 2,459 |
| **Total First-Party Production Source** | **70** | **1,945** | **772** | **8,070** |

### Test & Fuzz Code (`tests/` & `fuzz/`)
| Component | Files | Blank Lines | Comment Lines | Code Lines (LOC) |
|---|---|---|---|---|
| Unit & Integration Tests (`tests/*.cpp`) | 24 | 557 | 45 | 3,283 |
| Fuzz Harnesses & Seeds (`fuzz/*.cpp`) | 6 | 68 | 5 | 378 |
| **Total Test & Fuzz Source** | **30** | **625** | **50** | **3,661** |

### Bundled Third-Party & Subprojects
- **`third_party/`**: Bundled fallback implementations for `lz4`, `zstd`, `zlib`, and `sha256` (pure C).
- **`tempest/`**: Submodule/subproject tracking distributed workflow engine components (17,060 Go LOC).

---

## 3. Git History Quality & Commit Metrics

- **Current Commit Count**: `16 commits` (`git log --oneline | wc -l`)
- **Development Time Span**: June 27, 2026 to September 6, 2026 (~2.5 months).
- **Commit History Character**:
  - Initial scaffolding commits establishing zero-network CMake builds and bundled deps.
  - Bugfix and hardening commits (`Fix clang aggregate init`, `Fix OOM in IndexManager::deserialize`).
  - Defect injection PR merge: `Merge pull request #1 from ayushjava07/more-hard-bugs` (`b5e3db3 Add 20 hard memory-safety bugs`).
  - Commits are meaningful but heavily batched; history does not yet reflect the target 150+ incremental commit progression.

---

## 4. Subsystem Inventory & Architectural Boundaries

NebulaFS is organized into 10 cohesive core subsystems across `include/nebula/` and `src/`:

| Subsystem | Namespace | Core Classes / Files | Responsibilities |
|---|---|---|---|
| **Archive Container** | `nebula::archive` | `ArchiveHeader`, `ArchiveReader`, `ArchiveWriter` | High-level archive creation, file packing, container validation, header serialization |
| **Compression Engine** | `nebula::compression` | `CompressionEngine`, `CompressionBlock` | Pluggable compression drivers (LZ4, Zlib, Zstd), block-level compression & framing |
| **Cryptographic Engine** | `nebula::crypto` | `EncryptionEngine`, `HashEngine` | AES-256-GCM authenticated encryption, SHA-256 hashing, HMAC signatures |
| **Filesystem & Recovery**| `nebula::filesystem` | `DirectoryTree`, `FileResolver`, `JournalManager`, `Recovery`, `BTree` | Virtual hierarchy, path resolution, write-ahead journaling, crash-safe transaction recovery |
| **Indexing Engine** | `nebula::index` | `IndexManager`, `BTree`, `HashTable`, `SessionManager` | O(1) hash table lookup by entry ID, range scans via B-Tree, index table persistence |
| **Metadata Management** | `nebula::metadata` | `MetadataStore`, `EntryMetadata` | Key-value attribute tagging, timestamps, permissions, custom user metadata |
| **Network & Server** | `nebula::network` | `ArchiveServer`, `ProtocolRegistry`, `ThreadPool` | Binary wire protocol for remote client queries, connection pool, worker threads |
| **Parser Pipeline** | `nebula::parser` | `Parser`, `StreamParser`, `MetadataParser` | Zero-copy stream parsing, incremental section validation, corrupt payload detection |
| **Storage & Deduplication**| `nebula::storage` | `BlockStorage`, `ChunkManager`, `CacheManager` | Content-defined chunking (Rabin/rolling hash), chunk deduplication table, block store |
| **Utilities & Primitives** | `nebula::utils` | `Buffer`, `VarInt`, `Checksum`, `MemoryMappedFile`, `Allocator` | LEB128 encoding, memory-mapped I/O, CRC32/Adler32, aligned buffer allocations |

---

## 5. Test Suite Inventory & Category Matrix

The repository contains **24 test suites** comprising **295 test cases** in `tests/`:

| Category | Status | Existing Coverage |
|---|---|---|
| **Unit Tests** | Strong | Comprehensive tests for `Buffer`, `VarInt`, `Checksum`, `MemoryMappedFile`, `Compression`, `Crypto`, `BTree`, `HashTable`, `DirectoryTree`. |
| **Integration Tests** | Moderate | `test_integration.cpp` exercises full archive lifecycle (write → read → parse → extract). |
| **Concurrency Tests** | Thin | Basic thread pool tests in `test_network_server.cpp`; lacks concurrent read/write and lock-free cache stress tests. |
| **Persistence / Durability** | Moderate | `test_journal_manager.cpp` and `test_recovery.cpp` test WAL replay; lacks crash-simulation under write load. |
| **Boundary / Negative Tests**| Moderate | Negative parsing tests present; needs boundary tests on 0-byte entries, 4GB+ archives, and corrupt headers. |
| **API / CLI Tests** | Missing | No standalone CLI executable currently exists (only library and server). |
| **Fuzzing Coverage** | Strong | 5 LibFuzzer harnesses in `fuzz/` covering parser, index, compression, journal, and metadata. |
| **Property-Based Invariants**| Missing | No property-based invariant generation testing (e.g. RapidCheck). |

---

## 6. Build, Test, Lint, and Static Analysis Commands

These exact commands are detected and verified for the repository:

### 1. Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEBULA_BUILD_TESTS=ON
cmake --build build -j$(sysctl -n hw.ncpu || nproc)
```

### 2. Run Test Suite
```bash
./build/tests/nebula_tests
# Or via ctest:
ctest --test-dir build --output-on-failure
```

### 3. Sanitizer Builds
```bash
# AddressSanitizer (ASan) & UndefinedBehaviorSanitizer (UBSan):
cmake -B build_asan -DNEBULA_ENABLE_ASAN=ON -DNEBULA_ENABLE_UBSAN=ON -DNEBULA_BUILD_TESTS=ON
cmake --build build_asan -j
./build_asan/tests/nebula_tests

# ThreadSanitizer (TSan):
cmake -B build_tsan -DNEBULA_ENABLE_TSAN=ON -DNEBULA_BUILD_TESTS=ON
cmake --build build_tsan -j
./build_tsan/tests/nebula_tests
```

### 4. Static Analysis & Linting
```bash
# clang-tidy:
clang-tidy $(find src -name '*.cpp') -header-filter=include/** -- -std=c++20

# clang-format check:
clang-format --dry-run --Werror $(find include src tests -name '*.hpp' -o -name '*.cpp')
```

### 5. Fuzzing
```bash
cmake -B build_fuzz -DNEBULA_BUILD_FUZZ=ON -DFUZZER_ENGINE_FLAG="-fsanitize=fuzzer,address"
cmake --build build_fuzz -j
./build_fuzz/fuzz/archive_parser_fuzzer fuzz/corpus/archive_parser_fuzzer/ -max_total_time=30
```

---

## 7. Gap Analysis Against Benchmark Target Profile

| Dimension | Current State | Target Profile | Gap To Close |
|---|---|---|---|
| **Production LOC** | 8,070 lines | 32,000–40,000 lines | **+23,930 to +31,930 LOC** needed |
| **Commit Count** | 16 commits | ≥ 150 commits | **+134 commits** needed |
| **Subsystems** | 10 core subsystems | Enterprise-grade suite | Add CLI tool, TUI/web inspection, remote replication, incremental snapshotting, inverted text index |
| **Test Categories** | 4 of 8 categories solid | ≥ 5 categories covered across every subsystem | Add CLI tests, high-concurrency race tests, property invariant tests, crash simulation |
| **Defect Manifest** | 11 patch files in root | 25–30 independent verified defects in `internal-bench/defects.yaml` | Structure and catalog all defects with Sand-style F2P/P2P test tagging |

---

## 8. C++20 Stack Defect Feasibility

Unlike managed garbage-collected languages, the C++20 stack of NebulaFS can authentically produce and test **all 12 target defect categories**:

1. **Type-safety mistakes**: Pointer conversions, bad `reinterpret_cast`, signed/unsigned conversion underflows.
2. **Incorrect state transitions**: Out-of-order journal transaction commits, invalid decompression states.
3. **Resource-management problems**: Unclosed file descriptors, unmapped `mmap` regions, double-closes.
4. **Concurrency/race conditions**: Data races on chunk cache and thread pool queues (`TSan`-detectable).
5. **Stale-cache behavior**: Cache eviction omissions on chunk updates or rollbacks.
6. **Boundary-condition errors**: Off-by-one in LEB128 VarInt decoding, block alignment bounds.
7. **Incorrect error propagation**: Swallowed return codes, unhandled corrupted chunk checksums.
8. **Serialization inconsistencies**: Endianness packing bugs, header field misalignment.
9. **Lifecycle bugs**: Destructor order errors, use after move in stream pipelines.
10. **Configuration mistakes**: Buffer size overrides, compression level clipping.
11. **Validation gaps**: Missing bounds checks on archive section offset pointers (OOB vulnerabilities).
12. **Memory/resource leaks**: Memory leaks on exception throws (`ASan`/LeakSanitizer detectable).

---

**Audit Sign-off**: Phase 0 complete. Ready to proceed to Phase 1 (Authoring `ENHANCEMENT_PLAN.md`).
