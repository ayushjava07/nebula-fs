# NebulaFS Enhancement Plan

Based on the findings from `AUDIT.md`, this plan outlines the technical roadmap to elevate NebulaFS to an industrial-grade benchmark source meeting the target profile:
- **Target LOC**: 32,000–40,000 first-party production lines (current: 8,070; target gap: ~24,000 LOC).
- **Target Commit Count**: ≥ 150 commits (current: 16; target gap: +134 commits).
- **Quality Gates**: Per-commit gate validation with `Gate:` trailers and regression prevention.
- **Benchmark Defect Portfolio**: 25–30 confirmed independent defects packaged with Sand-style bundles.

---

## 1. Domain-Appropriate Subsystems to Close the Growth Gap

Every proposed addition naturally extends the embedded binary archive and object database domain:

### Subsystem 1: Operator CLI Tooling (`cmd/nebula`)
- Complete command-line interface supporting operations:
  - `create`: Multi-threaded archive compression and packing.
  - `extract`: Selective and directory-tree unpacking with permissions restoration.
  - `list`: Tree and tabular listing with filters, glob patterns, and human-readable sizes.
  - `verify`: Full cryptographic and block-level checksum validation.
  - `repair`: Reconstruction of damaged archives using transaction journals.
  - `bench`: End-to-end read/write throughput and deduplication benchmarks.

### Subsystem 2: Read-Only Archive Inspection Dashboard & TUI (`nebula::dashboard`)
- ANSI color terminal interface and server-rendered embedded HTTP status view.
- Visualizes:
  - Archive header layout, section offsets, and fragmentation percentage.
  - Compression ratio by algorithm (LZ4 vs Zlib vs Zstd).
  - Deduplication savings and chunk frequency histograms.
  - Provides screenshottable evidence for benchmark defect symptoms.

### Subsystem 3: Pluggable Storage Backend Layer (`nebula::storage::backend`)
- Abstraction separating logical archive operations from physical persistence:
  - `MemoryStorageBackend`: Thread-safe RAM-only container for high-speed ephemeral testing.
  - `FileStorageBackend`: Robust buffered and memory-mapped file operations with fallback.
  - `DirectIOStorageBackend`: Direct block device I/O avoiding OS page-cache pollution.

### Subsystem 4: Secondary Inverted Index (`nebula::search`)
- Inverted text and metadata indexing engine:
  - Tokenizer and lexer for entry names and metadata keys/values.
  - Posting lists and inverted index serialization in a dedicated `.nbf` section.
  - Fast multi-term search queries across millions of entries without full archive scans.

### Subsystem 5: Incremental Snapshot & Delta Archive Engine (`nebula::snapshot`)
- Differential container creation:
  - Reference existing parent archives and emit delta-only `.nbf` archives.
  - Chunk reconciliation and content-defined deduplication across archive boundaries.

### Subsystem 6: Remote Archive Streaming & Replication Protocol (`nebula::replication`)
- Streaming binary wire protocol over TLS:
  - Incremental chunk push and pull synchronization.
  - Resumable transfers with byte-level progress reporting and heartbeat validation.

### Subsystem 7: Multi-Tiered Cache & Explicit Invalidation (`nebula::cache`)
- Two-level caching hierarchy (L1 Memory Buffer + L2 Temp Disk Cache):
  - Eviction policies: LRU, LFU, and ARC (Adaptive Replacement Cache).
  - Explicit invalidation hooks when entries or chunks are superseded in journaled writes.

---

## 2. Commit Sizing & Per-Commit Gate Discipline

All commits will follow the strict atomic commit rules:
- **Maximum diff size**: ≤ 180 LOC per commit (average 60–120 LOC).
- **Atomic separation**:
  1. Component interface and header declarations.
  2. Component implementation and error handling.
  3. Unit and boundary tests.
  4. Integration tests and sanitizers.
- **Commit Trailer**: Every commit message must conclude with:
  ```
  Gate: build=pass lint=pass tests=pass race=pass
  ```
- **Commit Log**: Every commit will be appended to the `## Commit Log` section below.

---

## 3. Defect Category Distribution (30 Target Defects)

Given C++20's direct memory access and concurrency model, all 12 categories are fully feasible:

| Category | Target Count | Detection Strategy |
|---|---|---|
| Type-safety mistakes | 2 | Clang compiler warnings, ASan, UBSan |
| Incorrect state transitions | 3 | Table-driven state machine unit tests |
| Resource-management problems | 3 | ASan leak sanitizer, explicit RAII audits |
| Concurrency/race conditions | 4 | ThreadSanitizer (TSan), concurrent stress tests |
| Stale-cache behavior | 2 | Cache invalidation integration tests |
| Boundary-condition errors | 3 | Boundary tests, LEB128 fuzzing |
| Incorrect error propagation | 2 | Error code assertions, recovery tests |
| Serialization inconsistencies | 3 | Binary round-trip property tests |
| Lifecycle bugs | 3 | Destructor sequencing and move-safety tests |
| Configuration mistakes | 2 | Flag/env configuration precedence tests |
| Validation gaps | 3 | Negative fuzzing and OOB offset checks |
| Memory/resource leaks | 3 | ASan / Valgrind leak detection |
| **Total** | **30** | |

---

## 4. Phase Execution Timeline

- **Phase 0**: Audit completed (`AUDIT.md`).
- **Phase 1**: Enhancement Plan completed (`ENHANCEMENT_PLAN.md`).
- **Phase 2**: Growth Gap Closure (CLI, Dashboard, Pluggable Backends, Search Index, Snapshots).
- **Phase 3**: Hardening Pass (RAII audit, OOB checks, constant-time cryptography).
- **Phase 4**: Test Suite Completion (Concurrency, boundary, Property-based invariant tests).
- **Phase 5**: Clean-baseline verification and Golden Tag (`v2.0.0-golden`).
- **Phase 6**: Defect Catalog (`internal-bench/defects.yaml`) and isolated injections.
- **Phase 7**: Per-defect packaging into Sand-style task bundles.
- **Phase 8**: Documentation finalization.

---

## Commit Log

<!-- Appended to after every commit starting in Phase 2 -->
<!-- Format: <short-hash> | phase <N> | gate: PASS|FIXED-FROM-PREVIOUS | <one-line summary> -->
