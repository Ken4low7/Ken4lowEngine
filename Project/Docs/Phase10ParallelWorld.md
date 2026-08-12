# Phase 10 — Parallel World

## Goal

Phase 10 moves CPU-side world work from ad-hoc parallel calls toward an explicit dependency-driven schedule. The goal is to expose safe parallelism without allowing Actor, Physics, transform propagation, or spatial queries to observe partially updated world state.

Phase 10 builds on the existing fixed-worker `JobSystem`; it does not introduce a second thread pool.

## Planned steps

### 10.1 Job Dependency — implemented

`JobSystem` now supports prerequisite-aware dispatch through:

- `DispatchAfter(JobHandle, Job, priority)` for a single dependency
- `DispatchAfter(vector<JobHandle>, Job, priority)` for fan-in
- `ParallelForAfter(vector<JobHandle>, itemCount, grainSize, IndexedJob, priority)` for dependency-gated parallel batches

A dependency is an ordering dependency: the dependent becomes runnable when every valid prerequisite handle reaches completion. A prerequisite that completed before registration releases immediately, and an invalid/default handle does not block the dependent.

Dependency registration is race-safe even when a prerequisite completes while another prerequisite is still being registered. A shared `DependencyBatch` tracks:

- unresolved prerequisite count
- whether dependency registration has finished
- whether the batch has already been enqueued

Completion callbacks are attached to each prerequisite `JobState`. The batch is allowed into the normal priority queues only after registration is closed and the unresolved count reaches zero. An atomic one-time enqueue guard prevents duplicate release when multiple prerequisites finish concurrently.

`ParallelForAfter` creates the same chunk tasks as `ParallelFor`, but all chunks share one completion `JobState` and remain outside the runnable queues until their prerequisites complete. The returned handle therefore represents completion of the entire parallel batch rather than only the dependency gate.

Prerequisite failure does not cancel a dependent in 10.1. Dependencies currently express execution order, not success propagation. The failing handle retains its original exception through `HasFailed` / `RethrowIfFailed`, while dependents are still released once that prerequisite has finished. Cancellation/failure policies can be layered above the dependency graph later without changing scheduling identity.

The existing `Dispatch`, `ParallelFor`, priority queues, `Wait`, `WaitIdle`, and fixed worker pool remain compatible.

#### Validation

`Tests/Phase10/test_job_dependencies.py` validates the source-level dependency contract and, when a portable C++20 compiler is available, builds and runs `JobDependencyRuntimeTests.cpp` against the real `JobSystem.cpp`.

Runtime coverage includes:

- linear A -> B -> C ordering
- fan-in where C waits for A and B
- fan-out where A releases B and C
- already-completed and invalid handles
- dependency-gated `ParallelForAfter`
- failed prerequisite completion still releasing an ordering-only dependent

TeamDevelopmentCI runs Phase 10 tests before Debug/Release translation-unit compilation.

### 10.2 System Scheduling — planned

Introduce a world/system scheduler that converts phase relationships into `JobHandle` dependencies instead of hard-coding waits between independent CPU systems. The first migration should preserve current frame semantics and make read/write ownership explicit before increasing concurrency.

Likely schedule boundaries include world pre-update, independent Actor/Component work, transform finalization, physics preparation/step, and post-physics consumers. GPU API calls and Editor UI stay outside worker execution.

### 10.3 Dirty Tracking — planned

Avoid recomputing transforms/bounds/derived world data for unchanged objects. Dirty propagation must integrate with the 10.2 schedule so readers depend on the job that finalizes the corresponding dirty data rather than observing stale state.

### 10.4 Spatial Query Optimization — planned

Use the stabilized scheduled/dirty world data to reduce broad spatial-query cost. Candidate work includes persistent spatial partitions, batched queries, and parallel read-only query phases. Correctness and deterministic ownership come before changing the spatial structure.

## Compatibility strategy

Phase 10 remains incremental. Existing synchronous update paths stay the regression baseline while individual systems migrate to explicit dependency edges. No worker job may call GPU APIs or mutate Editor UI. Shared world data must have one clear writer phase or explicit synchronization before it is consumed in parallel.

## Boundary with later phases

Phase 10 owns CPU world scheduling and world-query optimization. Phase 11 remains responsible for Editor workflow/tooling improvements, while Phase 12 owns production-readiness validation such as crash handling, replay/compatibility checks, and release testing.
