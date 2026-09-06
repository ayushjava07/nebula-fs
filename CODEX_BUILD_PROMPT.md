# Codex Build Prompt — Synthetic Benchmark Repository (Enhanced Edition)

## How to use this file

This enhanced prompt is designed for autonomous coding agents (such as Codex, Claude, or Antigravity) to build a production-grade, highly realistic synthetic benchmark repository with a massive, organically grown Git history (**280–350+ commits**) and **32,000–40,000 lines of clean Go code**.

### Key Enhancements Made in this Version:
1. **Massive Commit History Enforcement (280–350+ Commits)**:
   - Eliminated the agent "batching syndrome" by imposing a **strict maximum ceiling of 150–200 LOC per commit** and an **atomic commit rule** (separate types, implementations, tests, fixtures, and docs into dedicated commits).
   - Provided a **Granular Commit Manifest** detailing the exact commit sequence across all 13 phases with strict numeric gates at every phase checkpoint.
   - Added a **Git Topology & Realistic Development Protocol**: feature branch simulation (`feature/...`), short-lived PR merges with `--no-ff`, multi-persona author attribution (4 simulated team members), and a deterministic commit timestamp distribution script covering a 7-month development arc.
2. **Full Concrete Architecture Blueprint (16 Subsystems)**:
   - Pinned product name to **`Tempest`** (`github.com/tempest-io/tempest`), a high-throughput, fault-tolerant distributed workflow orchestration engine.
   - Concrete package layout, PostgreSQL DDL schemas (`leases`, `runs`, `tasks`, `event_outbox`, `audit_events`), and formal state machine transition matrices.
3. **Expanded 36-Defect Manifest (Phase 10)**:
   - Completely specified all 36 candidates across the 12 categories with explicit Defect IDs (`DEF-001` to `DEF-036`), target subsystems, concrete failure mechanisms, primary detection tools (`go test -race`, `goleak`, `staticcheck`, `rapid`), target `[F2P]` regression tests, and `[P2P]` baseline tests.
4. **Sand-Style Benchmark Packaging Engine (Phase 11)**:
   - Defined the exact task bundle structure (`tasks/DEF-XXX/`) including `instructions.md`, `problem.md`, `solution.patch`, `broken.patch`, metadata, and visual/terminal evidence.
   - Included automated validation scripts to test and verify all defect packages both before and after patching.
5. **Robust Resumption & Failure Recovery**:
   - Explicit instructions on how an agent can resume from any interrupted phase using `PLAN.md` and Git status without duplicating work or losing commit cadence.

Paste everything from the `---` below into Codex / your autonomous agent as-is.

---

## Role

You are the lead systems engineer building **Tempest** (`github.com/tempest-io/tempest`), an original, enterprise-grade distributed workflow orchestration platform written from scratch in Go.

This repository will serve as the source material for high-difficulty software engineering benchmark tasks for AI coding agents. Therefore, it **must look and feel like an organically grown, multi-contributor production repository** with an extensive, believable commit history (**minimum 280 commits**, target **300–350 commits**), spanning 32,000–40,000 lines of Go code (excluding generated code and vendors).

Work autonomously through the phases in order. Never batch multiple subsystems or broad refactors into single monolithic commits. Follow the explicit commit cadence, phase gates, and defect methodologies specified below.

---

## System Architecture & Technical Specifications

### 1. Product Identity & Domain
- **Product Name**: **Tempest**
- **Module Path**: `github.com/tempest-io/tempest`
- **Binary Name**: `tempest` (`cmd/tempest/main.go`)
- **Core Domain**: Distributed workflow orchestration engine supporting DAG-based task execution, distributed leases, crash-safe state recovery, pluggable executors, and event-driven notifications.

### 2. Pinned Dependencies (`go.mod`)
Use Go 1.22+ with fixed, standard dependencies:
- `github.com/google/uuid v1.6.0` (ID generation)
- `github.com/lib/pq v1.10.9` (PostgreSQL driver)
- `google.golang.org/grpc v1.64.0` & `google.golang.org/protobuf v1.34.1` (gRPC surface)
- `github.com/prometheus/client_golang v1.19.1` (Metrics)
- `go.uber.org/goleak v1.3.0` (Goroutine leak detection in tests)
- `pgregory.net/rapid v1.1.0` (Property-based invariant testing)
- `golang.org/x/sync v0.7.0` (Concurrency primitives)

### 3. Complete Subsystem Map (16 Required Subsystems)
Every subsystem has a designated package under `pkg/` (public API) or `internal/` (private logic):

| # | Subsystem | Package Path | Primary Responsibilities |
|---|---|---|---|
| 1 | Workflow & State Machine Engine | `pkg/types`, `internal/statemachine`, `internal/workflow` | DAG definition, run lifecycle, state transitions, step dependency resolution |
| 2 | Scheduler & Worker Pool | `internal/scheduler` | Priority queue, goroutine worker pool, lease heartbeating, work stealing |
| 3 | Persistence Layer | `internal/persistence/{memstore,pgstore,conformance}` | `Store` interface, ACID transactional isolation, PostgreSQL DDL migrations, lease locks |
| 4 | In-Memory Cache | `internal/cache`, `internal/lru` | Generic LRU + TTL cache, tenant-partitioned keys, explicit mutation invalidation |
| 5 | HTTP REST API | `internal/api`, `internal/api/v1` | Versioned REST endpoints, RFC 7807 problem details, pagination, rate limiting |
| 6 | gRPC API Surface | `internal/grpcapi` | Proto definitions, gRPC services mirroring workflow execution, auth interceptors |
| 7 | Operator CLI | `internal/cli` | Subcommands (`server`, `run`, `workflow`, `admin`, `migrate`), table/JSON output |
| 8 | Pluggable Task Handlers | `internal/plugin`, `internal/plugin/handlers` | `TaskHandler` interface, registry, built-ins: `http`, `shell`, `transform`, `wait` |
| 9 | Retry & Backoff Subsystem | `internal/retry`, `internal/backoff` | Exponential backoff, jitter algorithms, retry budget, context cancellation |
| 10 | Webhook & Event Dispatch | `internal/webhook`, `internal/events` | Outbox pattern, HMAC-SHA256 signatures, asynchronous deliverer pool |
| 11 | Observability & Metrics | `internal/metrics`, `internal/tracing` | Prometheus counters/histograms, `/debug/metrics`, `/debug/pprof`, trace contexts |
| 12 | Tiered Configuration Layer | `internal/config`, `internal/flagutil` | Priority merge: CLI flags > Env (`TEMPEST_*`) > Config file > Defaults |
| 13 | Authentication & RBAC | `internal/auth` | API token authentication, role hierarchy (`reader`, `operator`, `admin`), action matrix |
| 14 | Background Maintenance | `internal/worker` | Expiry worker, run compaction worker, orphaned lease reaper |
| 15 | HTML Status Dashboard | `internal/dashboard` | Zero-JS server-rendered templates, live queue statistics, run health indicators |
| 16 | Migrations & Fixtures | `internal/migration` | Sequential SQL migrations, transactional rollbacks, seed fixtures |

---

## Git History Discipline & Commit Architecture

### 1. Hard Commit Sizing & Atomicity Rules
- **Total Commit Target**: **280 to 350 commits** across the entire build.
- **Strict LOC Ceiling**: No commit may exceed **180 lines of code changed** (added/modified), with the sole exception of the initial Phase 0 scaffold and raw SQL migration scripts (max 300 LOC).
- **Average Commit Sizing**: 60 to 120 lines changed.
- **The Atomic Separation Rule**:
  - Never commit an interface and its implementation in the same commit.
  - Never commit a feature and its full test suite in the same commit.
  - Always follow this micro-commit sequence for every new component:
    1. `feat(<subsystem>): define interface and domain types`
    2. `feat(<subsystem>): implement core logic stub and validation`
    3. `feat(<subsystem>): complete execution engine and error handling`
    4. `test(<subsystem>): add unit and boundary tests`
    5. `test(<subsystem>): add concurrency / race-detector / leak tests`

### 2. Multi-Persona Team Simulation
Simulate a realistic 4-engineer distributed engineering team by varying `GIT_AUTHOR_NAME` and `GIT_AUTHOR_EMAIL`:
- **Lead Architect**: `Alex Vance <alex.vance@tempest.io>` (Domain types, state machine, core scheduler, design docs)
- **Infrastructure Engineer**: `Devon Reed <devon.reed@tempest.io>` (Persistence, pgstore, migrations, lease manager, worker pool)
- **API & Platform Engineer**: `Sora Chen <sora.chen@tempest.io>` (HTTP API, gRPC, CLI, plugins, webhooks, dashboard)
- **Reliability & QA Engineer**: `Marcus Brody <marcus.brody@tempest.io>` (Metrics, retry/backoff, fuzz tests, race testing, goleak audits)

### 3. Realistic Timestamp Arc (7 Months)
Spread all commits across a believable 7-month development timeline (e.g., from `2025-10-01` to `2026-05-01`).
- Commits must be concentrated on **weekdays** between 09:15 and 18:45 local time.
- Introduce realistic development bursts (sprints of 4–8 commits in one day during feature phases) followed by 1–2 day quiet periods.
- Export `GIT_AUTHOR_DATE` and `GIT_COMMITTER_DATE` before each commit:
  ```bash
  export GIT_AUTHOR_DATE="2025-11-12T14:22:10"
  export GIT_COMMITTER_DATE="2025-11-12T14:22:10"
  ```

### 4. Branching Topology & PR Merges
To replicate an authentic production repository:
- Develop major subsystems on short-lived feature branches:
  ```bash
  git checkout -b feature/pgstore-lease-locking
  # ... 4 to 6 micro commits ...
  git checkout main
  git merge --no-ff feature/pgstore-lease-locking -m "Merge pull request #14 from feature/pgstore-lease-locking"
  git branch -d feature/pgstore-lease-locking
  ```
- Incorporate roughly 15–20 merge commits across the development phases.

### 5. Conventional Commit Standard
All commit messages must use the imperative mood and follow Conventional Commits:
```
<type>(<scope>): <short imperative summary>

<detailed rationale explaining the engineering decisions, edge cases handled, or invariants enforced>

[Optional Ref: #ISSUE-ID]
```
Allowed types: `feat`, `fix`, `test`, `refactor`, `perf`, `docs`, `chore`, `ci`.

---

## Phase Plan with Strict Numeric Commit & LOC Gates

Maintain a running `PLAN.md` at the repo root. At the conclusion of every phase, record: current phase, running total LOC (via `cloc`), running commit count (`git log --oneline | wc -l`), and verify all gates before proceeding.

| Phase | Subsystems & Content | Commit Budget | Gate Checklist Before Advancing |
|---|---|---|---|
| **0** | Repo scaffold, `go.mod`, `DESIGN.md`, base error taxonomy, base domain types, CI workflow, Makefile | 12–15 commits | `DESIGN.md` complete; `go.mod` pinned; Phase commits ≥ 12; `go build ./...` clean |
| **1** | Core domain state machine, `Store` interface, in-memory store, PostgreSQL schema migrations, `pgstore`, transaction coordinator | 30–35 commits | Cumulative commits ≥ 45; State machine covers all legal + 5 illegal transitions; `pgstore` conformance tests pass |
| **2** | Priority scheduler queue, worker pool, lease heartbeating, exponential backoff, jitter, retry policies, cancellation hierarchies | 25–30 commits | Cumulative commits ≥ 75; Concurrency tests pass with `-race`; zero leaks with `goleak` |
| **3** | HTTP REST server, routing mux, API v1 DTOs, RFC 7807 error handler, pagination, rate limiting, gRPC server & protobuf definitions | 25–30 commits | Cumulative commits ≥ 105; All HTTP/gRPC endpoints have positive + negative boundary tests |
| **4** | Tiered config loader (flag/env/file), CLI framework, operator subcommands (`server`, `run`, `workflow`, `admin`), output formatters | 20–25 commits | Cumulative commits ≥ 130; Precedence matrix (flag > env > file > default) verified by unit tests |
| **5** | Plugin engine, `TaskHandler` registry, 4 built-in handlers (`http`, `shell`, `transform`, `wait`), event outbox, HMAC webhook deliverer | 20–25 commits | Cumulative commits ≥ 155; Event encode/decode round-trip tests pass; Webhook retry backoff verified |
| **6** | In-memory LRU+TTL cache, invalidation hooks, Prometheus metrics, background maintenance reapers, server-rendered HTML dashboard | 25–30 commits | Cumulative commits ≥ 185; Cache invalidation integration tests clean; Dashboard renders without external assets |
| **7** | Hardening pass: API token auth, RBAC permissions matrix, input validation audit, resource-cleanup audit, staticcheck cleanup | 20–25 commits | Cumulative commits ≥ 210; Zero `TODO`/`FIXME` stubs; `go vet` and `staticcheck` 100% clean |
| **8** | Comprehensive test suite expansion: table-driven edge tests, rapid invariant property tests, fuzz targets, `[F2P]`/`[P2P]` tagging | 30–35 commits | Cumulative commits ≥ 245; Production LOC: 32,000–40,000; Fuzz targets run crash-free; All tests tagged |
| **9** | **Golden Baseline Verification**: run full race, leak, static analysis, benchmark suite; tag golden commit | 8–10 commits | Cumulative commits ≥ 255; 100% green on `go test -race ./...`; Tag `v1.0.0-golden` |
| **10** | **Defect Injection**: Inject 36 catalogued defects as isolated, revertible commits on top of golden tag | 36 commits | Cumulative commits ≥ 290; Each defect commit cleanly revertible; Manifest matches 36 entries |
| **11** | **Sand-Style Benchmark Packaging**: Generate task bundles (`instructions.md`, patches, test specs, evidence transcripts) | 30–35 commits | Cumulative commits ≥ 320; 36 task packages built in `benchmarks/tasks/` |
| **12** | Documentation finalization (`README`, `CONTRIBUTING`, `CHANGELOG`, internal benchmark notes) | 10–15 commits | Cumulative commits ≥ 335; Final DoD checklist satisfied |

---

## Granular Commit Blueprint (Reference Sequence)

Follow this granular micro-commit roadmap to systematically reach the 300+ commit target:

### Phase 0: Project Inception & Foundation (Commits 1–14)
1. `chore(repo): initialize git repository and define .gitignore`
2. `chore(build): configure go.mod with Go 1.22 and pin core dependencies`
3. `docs(design): draft architecture design document and subsystem boundaries in DESIGN.md`
4. `chore(ci): set up GitHub Actions CI workflow for test, race, and lint`
5. `chore(build): create Makefile with test, lint, fuzz, and coverage targets`
6. `feat(errors): define Classified error taxonomy and domain error codes`
7. `test(errors): implement unit tests for error wrapping and classification`
8. `feat(types): define core workflow and task status enum types`
9. `feat(types): define Run, StepRun, and TaskDefinition struct schemas`
10. `feat(types): implement JSON serialization helpers and validation methods for types`
11. `test(types): add table-driven unit tests for domain type serialization`
12. `feat(validation): introduce string, identifier, and namespace validators`
13. `test(validation): add boundary tests for validation rules`
14. `docs(plan): initialize PLAN.md tracking Phase 0 completion and LOC metrics`

### Phase 1: State Machine & Dual Persistence Layer (Commits 15–48)
15. `feat(statemachine): define ExecutionState transitions and legal event matrix`
16. `feat(statemachine): implement StateMachine engine with transition listeners`
17. `feat(statemachine): add invariant validation preventing illegal transition paths`
18. `test(statemachine): add comprehensive table tests for all legal state transitions`
19. `test(statemachine): add negative tests asserting errors on illegal state transitions`
20. `feat(persistence): define Store interface for workflow definitions, runs, and leases`
21. `feat(persistence): define TxStore interface supporting atomic multi-entity transactions`
22. `feat(memstore): scaffold thread-safe in-memory store with sync.RWMutex`
23. `feat(memstore): implement workflow definition CRUD in memstore`
24. `feat(memstore): implement Run and StepRun lifecycle persistence in memstore`
25. `feat(memstore): implement lease locking with expiration tracking in memstore`
26. `feat(memstore): implement transactional rollback and commit staging in memstore`
27. `test(memstore): add comprehensive unit tests for memstore CRUD operations`
28. `test(memstore): add concurrent read/write race condition tests for memstore`
29. `feat(migration): create schema migration engine supporting versioned SQL scripts`
30. `feat(migration): add migration 001_initial_schema.sql defining workflows, runs, and tasks`
31. `feat(migration): add migration 002_leases_and_outbox.sql for distributed locks and events`
32. `feat(migration): add migration 003_audit_log_and_indices.sql for query optimization`
33. `test(migration): add unit tests for migration applier and rollback mechanisms`
34. `feat(pgstore): scaffold PostgreSQL Store implementation using database/sql`
35. `feat(pgstore): implement workflow definition storage queries and scanner`
36. `feat(pgstore): implement Run and StepRun CRUD with optimistic concurrency controls`
37. `feat(pgstore): implement distributed lease acquisition with SELECT FOR UPDATE SKIP LOCKED`
38. `feat(pgstore): implement lease heartbeating and renewal query logic`
39. `feat(pgstore): implement transactional outbox event storage`
40. `test(persistence): implement shared Store conformance test suite`
41. `test(persistence): execute conformance suite against memstore`
42. `feat(persistence): add deterministic in-memory clock provider for time-travel testing`
43. `test(persistence): add lease timeout and automatic expiration test cases`
44. `refactor(persistence): extract common SQL query builders and error mappers`
45. `test(statemachine): add rapid property-based test checking state machine reachability`
46. `chore(fixtures): add seed fixtures for complex workflow DAG definitions`
47. `test(persistence): add benchmark tests comparing memstore vs query builders`
48. `docs(plan): update PLAN.md with Phase 1 metrics and commit milestones`

### Phase 2: Priority Scheduler, Worker Pool & Retry Engine (Commits 49–78)
49. `feat(scheduler): define PriorityQueue for scheduled and runnable task steps`
50. `test(scheduler): add unit tests for task priority ordering and FIFO tie-breaking`
51. `feat(scheduler): implement WorkerPool managing bounded goroutine workers`
52. `feat(scheduler): implement Worker task assignment and graceful drain channels`
53. `test(scheduler): add concurrency test verifying WorkerPool bound enforcement`
54. `feat(backoff): implement ExponentialBackoff with configurable min/max duration`
55. `feat(backoff): add FullJitter and EqualJitter randomization algorithms`
56. `test(backoff): add unit tests verifying backoff duration bounds and jitter variance`
57. `feat(retry): define RetryPolicy struct with retryable error classifier`
58. `feat(retry): implement retry execution loop with context cancellation propagation`
59. `test(retry): add unit tests verifying maximum retry attempts and timeout aborts`
60. `feat(scheduler): implement LeaseMaintainer background goroutine for active runs`
61. `test(scheduler): add test asserting lease renewal during long-running tasks`
62. `feat(scheduler): implement Scheduler loop polling runnable tasks from Store`
63. `feat(scheduler): add work-stealing algorithm across partitioned queue buckets`
64. `test(scheduler): add integration test for DAG step execution order`
65. `feat(scheduler): implement task timeout watcher cancelling expired execution contexts`
66. `test(scheduler): add negative test verifying task cancellation on context deadline`
67. `feat(workflow): implement DAG dependency resolver checking upstream step completion`
68. `test(workflow): add unit tests detecting cycles and invalid dependencies in DAGs`
69. `feat(scheduler): implement graceful shutdown manager terminating workers cleanly`
70. `test(scheduler): add race detector stress test running 100 concurrent workflows`
71. `test(scheduler): add goleak verification in TestMain for scheduler package`
72. `feat(ratepool): implement token bucket rate limiter for worker task dispatch`
73. `test(ratepool): add unit test for rate limiter token consumption and refilling`
74. `feat(throttle): implement concurrency throttle per workflow namespace`
75. `test(throttle): add unit tests for namespace-isolated throttle limits`
76. `refactor(scheduler): optimize queue lock contention using atomic status counters`
77. `chore(lint): clean up internal scheduler package comments and exported symbols`
78. `docs(plan): record Phase 2 completion in PLAN.md`

### Phase 3: HTTP REST API & gRPC Interfaces (Commits 79–108)
79. `feat(api): scaffold HTTP Server with standard http.ServeMux and middleware chain`
80. `feat(api): implement RFC 7807 Problem Details error response writer`
81. `feat(api): implement structured request logger middleware with correlation IDs`
82. `feat(api/v1): define REST API request and response DTO schemas`
83. `feat(api/v1): implement POST /v1/workflows handler for workflow definition creation`
84. `feat(api/v1): implement GET /v1/workflows and GET /v1/workflows/{id} handlers`
85. `feat(api/v1): implement POST /v1/runs handler submitting new execution runs`
86. `feat(api/v1): implement GET /v1/runs/{id} and GET /v1/runs/{id}/steps handlers`
87. `feat(api/v1): implement POST /v1/runs/{id}/cancel handler for running executions`
88. `feat(api): implement cursor-based pagination middleware for list endpoints`
89. `test(api): add unit tests for pagination cursor encoding and decoding`
90. `feat(api): implement per-IP and per-token rate limiting HTTP middleware`
91. `test(api): add negative test asserting HTTP 429 Too Many Requests on rate limit breach`
92. `test(api): add boundary tests for invalid JSON bodies and malformed payload requests`
93. `test(api): add end-to-end integration test submitting and querying a workflow run`
94. `feat(grpcapi): define TempestService protobuf specification in api/proto/tempest.proto`
95. `chore(grpcapi): generate Go protobuf and gRPC server stubs`
96. `feat(grpcapi): implement gRPC Server wrapping core Store and Scheduler services`
97. `feat(grpcapi): implement CreateWorkflow and GetWorkflow RPC endpoints`
98. `feat(grpcapi): implement SubmitRun and GetRunStatus RPC endpoints`
99. `feat(grpcapi): implement CancelRun RPC endpoint with gRPC status error mapping`
100. `feat(grpcapi): implement gRPC unary interceptor for request logging and panics`
101. `feat(grpcapi): implement gRPC auth interceptor validating bearer metadata`
102. `test(grpcapi): add in-process gRPC client test suite for all RPC methods`
103. `test(grpcapi): add test verifying gRPC code INVALID_ARGUMENT on bad inputs`
104. `test(grpcapi): add test verifying gRPC code NOT_FOUND on non-existent runs`
105. `refactor(api): unify HTTP and gRPC error classification mappings`
106. `test(api): add goleak leak verification for HTTP server shutdown test`
107. `test(grpcapi): add goleak leak verification for gRPC server stop test`
108. `docs(plan): update PLAN.md with Phase 3 checkpoints and metrics`

### Phase 4: Tiered Configuration & Operator CLI (Commits 109–133)
109. `feat(config): define Config schema struct for server, db, scheduler, and auth`
110. `feat(config): implement default configuration provider with safe production defaults`
111. `feat(config): implement YAML/JSON file configuration loader`
112. `feat(config): implement environment variable loader overriding config with TEMPEST_*`
113. `feat(flagutil): implement custom CLI flag parser supporting duration and string slices`
114. `feat(config): implement 4-tier precedence resolver (Flag > Env > File > Default)`
115. `test(config): add unit tests verifying flag overriding environment variable`
116. `test(config): add unit tests verifying env overriding configuration file`
117. `feat(config): add sensitive credential masking for logged configuration structs`
118. `feat(cli): scaffold CLI command hierarchy using standard flag package`
119. `feat(cli): implement 'tempest server' command starting HTTP, gRPC, and scheduler`
120. `feat(cli): implement 'tempest run submit' command with file payload input`
121. `feat(cli): implement 'tempest run inspect' command querying live run status`
122. `feat(cli): implement 'tempest run cancel' command stopping an active execution`
123. `feat(cli): implement 'tempest workflow register' and 'tempest workflow list'`
124. `feat(cli): implement 'tempest admin migrate' command executing DB schema migrations`
125. `feat(cli): implement 'tempest admin compact' triggering historical run pruning`
126. `feat(cli): implement table, JSON, and YAML output formatters for CLI results`
127. `test(cli): add unit tests for table formatting and column alignment`
128. `test(cli): add CLI argument parsing error tests and unknown subcommand help tests`
129. `feat(cli): implement bash and zsh shell autocompletion script generators`
130. `feat(shutdown): implement OS signal listener handling SIGINT and SIGTERM gracefully`
131. `test(shutdown): add integration test verifying clean shutdown on signal trigger`
132. `chore(docs): write man-page style CLI documentation in docs/cli.md`
133. `docs(plan): record Phase 4 status in PLAN.md`

### Phase 5: Plugin Architecture & Event Subsystems (Commits 134–158)
134. `feat(plugin): define TaskHandler interface and ExecutionContext contracts`
135. `feat(plugin): implement thread-safe PluginRegistry for registering custom handlers`
136. `feat(plugin/handlers): implement HttpTaskHandler executing outbound HTTP requests`
137. `feat(plugin/handlers): add configurable timeout and retry support to HttpTaskHandler`
138. `test(plugin/handlers): add unit tests for HttpTaskHandler using httptest.Server`
139. `feat(plugin/handlers): implement ShellTaskHandler executing sandboxed shell commands`
140. `feat(plugin/handlers): add stdout/stderr capture and exit-code evaluation in ShellTaskHandler`
141. `test(plugin/handlers): add unit tests for ShellTaskHandler execution and timeout killing`
142. `feat(plugin/handlers): implement TransformTaskHandler for JSON payload mapping`
143. `test(plugin/handlers): add unit tests for TransformTaskHandler field extraction`
144. `feat(plugin/handlers): implement WaitTaskHandler for timed delay execution steps`
145. `test(plugin/handlers): add unit tests for WaitTaskHandler respecting context cancellation`
146. `feat(events): define Event schema and EventType constants (RunStarted, RunCompleted, etc.)`
147. `feat(events): implement transactional OutboxPublisher saving events to store`
148. `feat(events): implement EventBus for in-process decoupled event subscriptions`
149. `test(events): add unit tests for EventBus publish and subscriber fan-out`
150. `feat(webhook): define WebhookSubscription struct with target URL and secret`
151. `feat(webhook): implement HMAC-SHA256 request signer for webhook payload integrity`
152. `test(webhook): add unit tests for HMAC signature generation and verification`
153. `feat(webhook): implement WebhookDeliverer worker dispatching HTTP POST notifications`
154. `feat(webhook): add exponential retry loop with backoff for failed webhook deliveries`
155. `test(webhook): add integration tests for webhook delivery with mock HTTP receiver`
156. `test(events): add round-trip serialization tests for all defined event types`
157. `test(webhook): add goleak verification for WebhookDeliverer shutdown`
158. `docs(plan): record Phase 5 completion in PLAN.md`

### Phase 6: Cache, Observability, Maintenance & Dashboard (Commits 159–188)
159. `feat(lru): implement generic thread-safe LRU cache with eviction callback`
160. `test(lru): add unit tests for LRU capacity eviction and element access order`
161. `feat(cache): implement TTL cache layer with automatic background expiration`
162. `feat(cache): implement tenant-partitioned cache keys preventing cross-tenant leakage`
163. `feat(cache): implement explicit CacheInvalidator hook on workflow definition update`
164. `test(cache): add integration tests verifying cache invalidation on workflow edit`
165. `feat(metrics): scaffold Prometheus metrics registry and custom collectors`
166. `feat(metrics): add counters for workflow_runs_total and tasks_executed_total`
167. `feat(metrics): add histogram for task_execution_duration_seconds with custom buckets`
168. `feat(metrics): add gauges for active_workers and queue_depth`
169. `feat(metrics): implement /debug/metrics HTTP endpoint exposing Prometheus metrics`
170. `feat(metrics): implement /debug/pprof profiling routes on dedicated internal port`
171. `test(metrics): add unit tests verifying metric increment and observation accuracy`
172. `feat(worker): scaffold background MaintenanceManager with ticker loops`
173. `feat(worker): implement RunExpiryWorker pruning runs exceeding retention TTL`
174. `test(worker): add unit test for RunExpiryWorker verifying deletion of expired runs`
175. `feat(worker): implement HistoryCompactionWorker squashing old step logs`
176. `test(worker): add unit test for HistoryCompactionWorker record consolidation`
177. `feat(worker): implement OrphanedLeaseReaper reclaiming expired worker leases`
178. `test(worker): add unit test asserting orphaned lease recovery by reaper`
179. `feat(dashboard): scaffold server-rendered HTML status dashboard in internal/dashboard`
180. `feat(dashboard): implement embedded HTML templates for active and historical runs`
181. `feat(dashboard): add queue health indicators and worker pool utilization widgets`
182. `feat(dashboard): implement GET /dashboard and GET /dashboard/runs/{id} HTTP routes`
183. `test(dashboard): add test verifying dashboard HTML rendering against memstore`
184. `test(dashboard): assert dashboard renders without external network CSS/JS dependencies`
185. `feat(tracing): implement lightweight trace context propagator across steps`
186. `test(tracing): add unit tests for trace ID generation and context propagation`
187. `test(cache): add concurrency stress test for cache read/write under high load`
188. `docs(plan): update PLAN.md with Phase 6 status and metrics`

### Phase 7: Hardening, Security, RBAC & Static Analysis (Commits 189–213)
189. `feat(auth): define Role hierarchy (RoleReader, RoleOperator, RoleAdmin)`
190. `feat(auth): implement TokenAuthenticator resolving API tokens to user principals`
191. `feat(auth): implement RBAC authorization matrix for all API and CLI operations`
192. `test(auth): add unit tests verifying RoleReader cannot perform write actions`
193. `test(auth): add unit tests verifying RoleAdmin full access privileges`
194. `feat(auth): implement HTTP auth middleware enforcing Bearer token and roles`
195. `test(auth): add negative test asserting HTTP 401 Unauthorized on missing token`
196. `test(auth): add negative test asserting HTTP 403 Forbidden on insufficient role`
197. `feat(validation): perform input validation audit on all JSON request DTOs`
198. `feat(validation): add sanitize checks against path traversal in artifact names`
199. `test(validation): add fuzz-style negative boundary tests for input payloads`
200. `refactor(persistence): audit resource cleanup ensuring all sql.Rows are closed`
201. `refactor(api): audit HTTP response body closing across all outbound client calls`
202. `refactor(scheduler): verify all spawned goroutines have deterministic termination channels`
203. `test(leak): add TestMain with goleak in all internal packages`
204. `chore(lint): run go vet ./... and resolve all warnings`
205. `chore(lint): run staticcheck ./... and resolve all code smell diagnostics`
206. `feat(audit): implement structured AuditLogger recording all administrative actions`
207. `test(audit): add unit tests verifying audit event emission on run cancellation`
208. `feat(crypto): implement constant-time comparison for all token validations`
209. `test(crypto): add unit tests for constant-time hash comparisons`
210. `refactor(errors): eliminate any remaining generic errors in favor of Classified`
211. `chore(repo): audit and resolve all TODO and FIXME comments across codebase`
212. `chore(deps): run go mod tidy and verify go.sum integrity`
213. `docs(plan): record Phase 7 audit completion in PLAN.md`

### Phase 8: Comprehensive Test Suite & F2P/P2P Tagging (Commits 214–248)
214. `test(statemachine): add rapid property-based invariant test for state transition paths`
215. `test(encoding): add rapid round-trip serialization property test for all DTOs`
216. `test(fuzz): implement FuzzWorkflowDefinitionParser fuzzing JSON/YAML workflow parsers`
217. `test(fuzz): implement FuzzIdentifierValidator fuzzing name and namespace validation`
218. `test(fuzz): implement FuzzCronScheduleParser fuzzing recurrence expression parser`
219. `test(fuzz): implement FuzzHMACSignatureVerifier fuzzing webhook header verification`
220. `test(e2e): implement end-to-end integration test of complete 5-step DAG execution`
221. `test(e2e): implement e2e test of task failure, exponential backoff, and eventual success`
222. `test(e2e): implement e2e test of run cancellation and step context aborts`
223. `test(e2e): implement e2e test of worker crash and lease recovery by secondary worker`
224. `test(concurrency): add high-load race condition test with 200 concurrent worker goroutines`
225. `test(boundary): add boundary tests for 0-step workflows and 100-step giant DAGs`
226. `test(boundary): add boundary tests for max payload size limits (10MB)`
227. `test(tags): tag baseline state machine invariant test with [P2P]`
228. `test(tags): tag baseline DAG step ordering test with [P2P]`
229. `test(tags): tag baseline lease acquisition test with [P2P]`
230. `test(tags): tag baseline worker pool bounding test with [P2P]`
231. `test(tags): tag baseline HTTP submit workflow test with [P2P]`
232. `test(tags): tag baseline gRPC cancel execution test with [P2P]`
233. `test(tags): tag baseline config precedence test with [P2P]`
234. `test(tags): tag baseline webhook HMAC verification test with [P2P]`
235. `test(tags): tag baseline cache invalidation test with [P2P]`
236. `test(tags): tag baseline maintenance reaper test with [P2P]`
237. `test(tags): tag baseline RBAC role authorization test with [P2P]`
238. `test(tags): pre-seed candidate regression test DEF-001 with [F2P] marker`
239. `test(tags): pre-seed candidate regression test DEF-002 with [F2P] marker`
240. `test(tags): pre-seed candidate regression test DEF-003 through DEF-010 with [F2P] markers`
241. `test(tags): pre-seed candidate regression test DEF-011 through DEF-020 with [F2P] markers`
242. `test(tags): pre-seed candidate regression test DEF-021 through DEF-030 with [F2P] markers`
243. `test(tags): pre-seed candidate regression test DEF-031 through DEF-036 with [F2P] markers`
244. `chore(scripts): create run_all_tests.sh script running unit, race, and leak suites`
245. `chore(scripts): create run_fuzz.sh running all fuzz targets for 30s each`
246. `test(coverage): generate full test coverage profile and verify subsystem thresholds`
247. `docs(testing): document testing architecture and tagging conventions in docs/testing.md`
248. `docs(plan): record Phase 8 test coverage and LOC verification in PLAN.md`

### Phase 9: Golden Baseline Verification & Tagging (Commits 249–257)
249. `chore(ci): update CI pipeline to run complete test suite including -race and goleak`
250. `test(verify): run full go test ./... asserting 100% green status`
251. `test(verify): run go test -race ./... asserting zero data races`
252. `test(verify): run staticcheck and go vet asserting zero diagnostic warnings`
253. `test(verify): run short fuzz corpus check verifying zero parser crashes`
254. `perf(bench): add standard benchmark suite measuring run submission and dispatch latency`
255. `docs(bench): record benchmark baseline performance numbers in docs/benchmarks.md`
256. `docs(golden): finalize golden state documentation and sign off on stability`
257. `chore(release): tag golden baseline release v1.0.0-golden`

### Phase 10: Defect Catalog & Isolated Injections (Commits 258–293)
*(36 distinct, cleanly revertible defect commits `DEF-001` through `DEF-036` as listed in Master Defect Table)*

### Phase 11: Sand-Style Benchmark Task Packaging (Commits 294–325)
*(Bundling each defect into standalone benchmark task packages under `benchmarks/tasks/`)*

### Phase 12: Project Documentation & CI Finalization (Commits 326–340)
326. `docs(readme): author comprehensive README.md with architecture overview and quickstart`
327. `docs(contributing): write CONTRIBUTING.md outlining coding standards and PR process`
328. `docs(changelog): reconstruct detailed CHANGELOG.md from actual Git history`
329. `docs(architecture): add ASCII and Mermaid architecture diagrams to docs/architecture.md`
330. `docs(api): publish OpenAPI 3.0 specification for HTTP REST API in docs/openapi.yaml`
331. `docs(grpc): publish gRPC API documentation in docs/grpc.md`
332. `docs(plugins): create custom task handler development guide in docs/plugins.md`
333. `docs(ops): create production operations and deployment guide in docs/operations.md`
334. `chore(ci): finalize GitHub Actions matrix for multi-platform local verification`
335. `chore(bench): write internal BENCHMARK_NOTES.md documenting task manifest index`
336. `chore(scripts): create verify_repo.sh running full definition of done checks`
337. `test(verify): run verify_repo.sh confirming all 340+ commits and LOC thresholds`
338. `docs(plan): finalize PLAN.md recording all phase accomplishments and final metrics`
339. `chore(license): verify Apache 2.0 license headers across all source files`
340. `chore(release): tag final benchmark master release v1.0.0-benchmark`

---

## Comprehensive 36-Defect Catalog Specification (Phase 10)

Before writing any injection code, author `internal-bench/defects.yaml`. Every entry must have:
- `id`: `DEF-001` through `DEF-036`
- `subsystem`: Target subsystem
- `category`: Exactly one of the 12 categories
- `mechanism`: Exact technical description of the realistic mistake
- `detection`: Specific tool and test command surfacing the defect
- `f2p_test`: Target `[F2P]` regression test that fails on broken and passes on fixed
- `p2p_tests`: Two existing `[P2P]` tests that pass in both broken and fixed states
- `fix_commit`: Commit hash or clean patch reversing the injection

### Master Defect Table

| ID | Subsystem | Category | Failure Mechanism | Primary Detection | F2P Test Name |
|---|---|---|---|---|---|
| `DEF-001` | `internal/statemachine` | Incorrect state transitions | Transition to `SUCCEEDED` allowed directly from `PAUSED` without passing through `RUNNING` | Table-driven unit test | `TestStateMachine_PausedToSucceeded_Illegal` |
| `DEF-002` | `internal/statemachine` | Incorrect state transitions | `CANCELLED` state allows subsequent transition to `FAILED` when late step error returns | Rapid invariant property test | `TestStateMachine_TerminalStateImmutability` |
| `DEF-003` | `internal/statemachine` | Incorrect state transitions | Workflow transition to `WAITING_RETRY` fails to reset active step execution counters | State machine integration test | `TestStateMachine_RetryStepCounterReset` |
| `DEF-004` | `internal/scheduler` | Concurrency/race conditions | Unsynchronized read of `activeWorkers` atomic counter during worker pool resize | `go test -race ./...` | `TestWorkerPool_ConcurrentResizeAndDispatch` |
| `DEF-005` | `internal/scheduler` | Concurrency/race conditions | Double unlock of `sync.RWMutex` on task dispatch queue under error path | `go test -race ./...` | `TestPriorityQueue_ConcurrentPopAndClear` |
| `DEF-006` | `internal/persistence/memstore` | Concurrency/race conditions | Range loop over shared map without holding read lock during list query | `go test -race ./...` | `TestMemStore_ConcurrentListAndWriteRuns` |
| `DEF-007` | `internal/webhook` | Concurrency/race conditions | Data race on `delivererCount` when scaling webhook workers dynamically | `go test -race ./...` | `TestWebhookDispatcher_DynamicWorkerScaleRace` |
| `DEF-008` | `internal/persistence/pgstore` | Resource-management problems | `database/sql.Rows` not closed on early break in run pagination scanner | `goleak` / explicit close test | `TestPgStore_ListRuns_RowsLeakOnBreak` |
| `DEF-009` | `internal/plugin/handlers` | Resource-management problems | Unclosed `http.Response.Body` on non-200 status codes in `HttpTaskHandler` | Resource-leak unit test | `TestHttpHandler_ResponseBodyClosedOnNon200` |
| `DEF-010` | `internal/cli` | Resource-management problems | Temporary config file descriptor leaked when parsing invalid YAML input | `goleak` / FD count test | `TestConfigLoader_FileDescriptorLeakOnError` |
| `DEF-011` | `internal/cache` | Stale-cache behavior | Workflow definition cache key omits `namespace` prefix, cross-pollinating tenants | Tenant isolation test | `TestCache_TenantKeyIsolationOnUpdate` |
| `DEF-012` | `internal/cache` | Stale-cache behavior | Cache invalidation hook triggered before DB transaction commits; rollback re-caches stale data | Integration invalidation test | `TestCache_InvalidationAfterCommitOnly` |
| `DEF-013` | `internal/retry` | Boundary-condition errors | Off-by-one in retry loop: executes `maxRetries + 1` attempts before halting | Table boundary test | `TestRetryPolicy_MaxAttemptsExactBound` |
| `DEF-014` | `internal/backoff` | Boundary-condition errors | Integer overflow when computing exponential backoff duration for large attempt values (>32) | Fuzz / boundary test | `TestBackoff_DurationOverflowProtection` |
| `DEF-015` | `internal/api` | Boundary-condition errors | Pagination limit of `0` returns all records instead of clamping to default minimum | Negative API boundary test | `TestAPI_PaginationZeroLimitClamping` |
| `DEF-016` | `internal/workflow` | Incorrect error propagation | Underlying store `ErrRunNotFound` swallowed and re-wrapped as generic internal server error | `errors.Is` assertion test | `TestWorkflow_NotFoundErrorPreserved` |
| `DEF-017` | `internal/persistence/pgstore` | Incorrect error propagation | DB transaction rollback error shadows the primary execution failure error | Error hierarchy unit test | `TestPgStore_TxRollbackPreservesOriginalError` |
| `DEF-018` | `internal/api/v1` | Serialization inconsistencies | JSON serialization of `StepRun.CreatedAt` omits timezone offset (RFC3339 vs UTC) | Round-trip property test | `TestSerialization_TimestampRoundTripUTC` |
| `DEF-019` | `internal/events` | Serialization inconsistencies | Protobuf event serialization drops custom metadata key-value map with integer keys | Serialization test | `TestEvent_MetadataSerializationIntegrity` |
| `DEF-020` | `internal/config` | Serialization inconsistencies | Durations formatted as integers in JSON config parsed as nanoseconds instead of seconds | Config round-trip test | `TestConfig_DurationStringAndIntegerCoercion` |
| `DEF-021` | `internal/scheduler` | Lifecycle bugs | Worker pool `Shutdown()` hangs indefinitely when queue contains unconsumed tasks | Integration lifecycle test | `TestScheduler_ShutdownDrainWithPendingTasks` |
| `DEF-022` | `internal/worker` | Lifecycle bugs | Maintenance reaper ticker channel not stopped on context cancellation, leaking goroutine | `goleak` in `TestMain` | `TestWorker_ReaperStopsCleanlyOnCancel` |
| `DEF-023` | `internal/webhook` | Lifecycle bugs | Webhook deliverer exits immediately on single delivery failure instead of continuing pool | Deliverer recovery test | `TestWebhook_DelivererSurvivesIndividualFailure` |
| `DEF-024` | `internal/config` | Configuration mistakes | Environment variable `TEMPEST_STORE_TIMEOUT` ignored when config file sets `store.timeout` | Precedence conflict test | `TestConfig_EnvOverridesFileExplicitPrecedence` |
| `DEF-025` | `internal/config` | Configuration mistakes | CLI `--port` flag defaults override non-empty config file port settings | Flag precedence test | `TestConfig_FlagDefaultDoesNotOverrideFile` |
| `DEF-026` | `pkg/validation` | Validation gaps | Workflow DAG parser allows self-referencing step dependency (`step.depends_on = [step.id]`) | Cyclic DAG validation test | `TestValidation_RejectSelfReferentialStep` |
| `DEF-027` | `internal/api` | Validation gaps | Workflow submit endpoint accepts negative timeout duration (`"timeout": "-5s"`) | Negative body test | `TestAPI_RejectNegativeTimeoutDuration` |
| `DEF-028` | `internal/auth` | Validation gaps | Bearer token parser accepts empty token string when Authorization header is `"Bearer "` | Auth boundary test | `TestAuth_RejectEmptyBearerToken` |
| `DEF-029` | `internal/scheduler` | Memory/resource leaks | Lease heartbeat map retains expired run IDs indefinitely without eviction | Heap snapshot / leak test | `TestScheduler_LeaseMapPrunedOnRunCompletion` |
| `DEF-030` | `internal/events` | Memory/resource leaks | EventBus channel buffer blocks and leaks goroutines when subscriber unsubscribes | `goleak` in `TestMain` | `TestEventBus_UnsubscribeClosesChannelCleanly` |
| `DEF-031` | `internal/dashboard` | Memory/resource leaks | Historical runs slice in dashboard handler grows unbounded on continuous refresh | Heap comparison test | `TestDashboard_HistoricalRunsBoundEnforced` |
| `DEF-032` | `internal/auth` | Type-safety mistakes | Unchecked type assertion from `context.Value("principal")` panics on unauthenticated route | Negative unit test | `TestAuth_SafeTypeAssertionOnPrincipalContext` |
| `DEF-033` | `internal/plugin` | Type-safety mistakes | Unchecked interface cast of task handler output payload panics on string output | Plugin error handling test | `TestPlugin_PayloadTypeAssertionSafety` |
| `DEF-034` | `internal/dashboard` | Validation gaps / Security | Unescaped step error message rendered in HTML dashboard allows XSS injection | XSS negative render test | `TestDashboard_ErrorStringHtmlEscaped` |
| `DEF-035` | `internal/retry` | Concurrency/race conditions | Race condition on retry attempt counter shared across concurrent sub-step executions | `go test -race ./...` | `TestRetry_SubStepAttemptCounterIsolation` |
| `DEF-036` | `internal/persistence/pgstore` | Incorrect state transitions | Lease acquisition succeeds even when lease has not expired due to missing `< NOW()` check | Concurrency lease test | `TestPgStore_LeaseStealRequiresExpiration` |

---

## Per-Defect Sand-Style Task Packaging Architecture (Phase 11)

For every surviving defect `DEF-001` through `DEF-036`, generate a complete, standalone benchmark task package under `benchmarks/tasks/<DEF-ID>/`:

```
benchmarks/tasks/DEF-001/
├── problem.md                # Technical problem description
├── instructions.md           # User/operator symptom description (no spoiler)
├── broken.patch              # Diff that induces the bug onto the golden tag
├── solution.patch            # Clean fix diff that resolves the bug
├── task_spec.json            # Machine-readable task metadata
├── test_runner.sh            # Automated verification script
└── evidence/                 # Visual or terminal proof of defect symptom
    ├── terminal_fail.txt     # Captured log or test failure output
    └── dashboard_symptom.png # (For dashboard/UI defects) Screenshot evidence
```

### Packaging Requirements
1. **Broken State**: Generated by applying `broken.patch` on top of `v1.0.0-golden`.
2. **Fixed State**: Generated by applying `solution.patch` on top of the broken state (restoring golden behavior).
3. **`instructions.md` Format**:
   - Describe the observed symptom purely from an operator, user, or client perspective (e.g., *"When running workflows with paused steps, the scheduler occasionally transitions them directly to succeeded, skipping required steps. Fix this so paused workflows cannot finish without running."*).
   - **Do not reveal the internal variable name, line number, or exact code fix.**
4. **Test Suite Requirements**:
   - Exactly one `[F2P]` regression test that **fails** in the broken state and **passes** in the fixed state.
   - At least two `[P2P]` tests in the same package that **pass** in both broken and fixed states.

---

## Automated Verification & Sanity Scripts

Create and commit these helper scripts under `scripts/`:

### 1. Verification Script (`scripts/verify_repo.sh`)
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Running Repo Definition of Done Checks ==="

COMMITS=$(git log --oneline | wc -l | tr -d ' ')
echo "Total commits: $COMMITS"
if [ "$COMMITS" -lt 280 ]; then
  echo "FAIL: Commit count ($COMMITS) is under minimum threshold of 280."
  exit 1
fi

echo "Checking build..."
go build ./...

echo "Running full test suite..."
go test ./...

echo "Running race detector..."
go test -race ./...

echo "Running static analysis..."
go vet ./...
if command -v staticcheck >/dev/null 2>&1; then
  staticcheck ./...
fi

echo "=== All Checks Passed Successfully! ==="
```

### 2. Task Benchmark Verifier (`scripts/validate_task.sh`)
```bash
#!/usr/bin/env bash
set -euo pipefail

TASK_ID="${1:?Usage: $0 <DEF-ID>}"
TASK_DIR="benchmarks/tasks/$TASK_ID"

echo "=== Validating Benchmark Task: $TASK_ID ==="

# 1. Start from clean golden tag
git checkout v1.0.0-golden -q

# 2. Apply broken patch
git apply "$TASK_DIR/broken.patch"

# 3. Verify F2P test fails on broken state
echo "Verifying [F2P] test fails on broken state..."
if go test -run "$TASK_ID" ./... > /dev/null 2>&1; then
  echo "FAIL: F2P test unexpectedly passed on broken state!"
  git checkout . -q
  exit 1
fi
echo "Confirmed: F2P test fails on broken state."

# 4. Apply solution patch
git apply "$TASK_DIR/solution.patch"

# 5. Verify F2P test now passes
echo "Verifying [F2P] test passes on fixed state..."
go test -run "$TASK_ID" ./...

# 6. Clean up
git checkout . -q
echo "=== Task $TASK_ID Successfully Validated! ==="
```

---

## Resumption & Failure Recovery Protocol

If your session is interrupted or hits context length limits:
1. Inspect `PLAN.md` to identify the last completed phase and the recorded commit count.
2. Run `git status` and `git log -n 5 --oneline` to verify the working tree.
3. Review the **Granular Commit Blueprint** to locate the exact next commit in the sequence.
4. Resume execution from that exact commit, maintaining the author persona, timestamp arc, and commit sizing rules.

---

## Definition of Done Checklist

Before declaring the repository complete, you must satisfy 100% of these criteria:

- [ ] `git log --oneline | wc -l` is between **280 and 350 commits**.
- [ ] Production Go source code (excluding tests, vendor, fixtures), measured with `cloc`, is between **32,000 and 40,000 lines**.
- [ ] `go build ./...` compiles cleanly from a fresh clone of `v1.0.0-golden`.
- [ ] `go test ./...` is 100% green on `v1.0.0-golden`.
- [ ] `go test -race ./...` produces zero race warnings on `v1.0.0-golden`.
- [ ] `go vet ./...` and `staticcheck ./...` report zero warnings on `v1.0.0-golden`.
- [ ] Every package spawning goroutines includes `goleak.VerifyNone(t)` in `TestMain`.
- [ ] Fuzz targets exist for JSON, YAML, HMAC, and schedule decoders and pass crash-free.
- [ ] `internal-bench/defects.yaml` documents all 36 candidates with unique root causes.
- [ ] All 36 defects have isolated, cleanly revertible git commits.
- [ ] Every defect package under `benchmarks/tasks/` has valid `instructions.md`, patches, metadata, evidence, and `[F2P]`/`[P2P]` test tags.
- [ ] `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, and `LICENSE` (Apache 2.0) are complete and match the real commit history.
- [ ] The entire build and test process runs completely offline with zero network dependencies or wall-clock timing flakiness.
