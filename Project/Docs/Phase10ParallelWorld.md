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

### 10.2 System Scheduling — implemented

`SystemScheduler` now sits above the existing `JobSystem` and converts declared world/system ownership into a dependency DAG. It does not own threads and does not create another worker pool.

Each registered system declares:

- a stable system name
- a callback receiving frame `deltaTime`
- `SystemResourceAccess` entries with `Read`, `Write`, or `ReadWrite`
- `SystemExecutionPolicy::MainThread` or `Worker`
- an optional `JobPriority`
- optional explicit system dependencies for non-data ordering constraints

The compiler tracks the last writer and active readers for each `SystemResourceId` and generates the same fundamental hazards used by the RenderGraph model:

- RAW — Read After Write
- WAR — Write After Read
- WAW — Write After Write

Read/Read access creates no dependency. Multiple accesses to the same resource inside one system are normalized, and a Read + Write combination becomes one `ReadWrite` ownership declaration.

Explicit and resource-derived prerequisites are deduplicated into each system's compiled prerequisite list. A stable topological sort then produces the executable order; dependency cycles fail `Compile()` rather than becoming a runtime deadlock.

Execution keeps thread affinity explicit. `Worker` systems are submitted through `JobSystem::DispatchAfter`, so independent worker systems can remain in flight concurrently while respecting the compiled DAG. `MainThread` systems wait only for their own worker prerequisites, execute on the caller thread, and publish a completed `JobHandle` through `JobSystem::CreateCompletedHandle`. This lets later systems use the same dependency representation without paying for a fake worker task.

`ExecuteAndWait` is the frame boundary. Ordering dependencies remain ordering-only even when a worker prerequisite fails: downstream systems are released after completion, and the first captured exception is rethrown only after scheduled work has been joined. This preserves the Phase 10.1 failure contract while avoiding abandoned worker work.

`SystemScheduleStats` and dependency records expose system count, unique dependency count, explicit dependency count, RAW/WAR/WAW counts, and MainThread/Worker counts for diagnostics.

#### First World migration

`DebugScene` now registers the existing Play update path as three systems:

1. `ActorWorld.Update`
2. `PhysicsWorld.Update`
3. `ActorWorld.PostPhysicsUpdate`

The previous hard-coded phase calls in `DebugScene::Update` were replaced by one `worldSystemScheduler_.ExecuteAndWait(deltaTime)` call. The three systems declare ownership of World object state, transform state, Physics registration, Physics state, and render-facing state, so the Actor -> Physics -> PostPhysics ordering is generated from data hazards instead of manual waits.

All three migrated systems intentionally remain `MainThread` in 10.2. Actor/Component update code can touch input, cameras, gameplay callbacks, Physics registration, and render-facing CPU state; Physics event dispatch also calls back into Actor/Component code. Moving these callbacks to workers before their thread-safety contracts are separated would create unsafe parallelism rather than useful parallelism.

The scheduler therefore establishes the dependency and affinity boundary first. Later Phase 10 work can move individually proven CPU-only systems to `Worker` without changing the frame-level ownership model.

`DebugScene`'s ActorWorld validation window also reports the compiled System/Dependency/MainThread/Worker counts so the active schedule is visible during development.

#### Validation

`Tests/Phase10/test_system_scheduler.py` validates the scheduler contract and the DebugScene migration. When a portable C++20 compiler is present, it builds `SystemSchedulerRuntimeTests.cpp` with the real `JobSystem.cpp` and executes the resulting binary.

Runtime coverage includes:

- Read/Read independence
- WAR fan-in from multiple readers to one writer
- RAW ordering after a writer
- MainThread affinity preservation
- Worker dispatch through the fixed JobSystem pool
- Main -> Worker -> Main dependency execution
- explicit cycle rejection
- failed worker prerequisite releasing its ordering dependent before the frame-boundary exception is rethrown

### 10.3 Dirty Tracking — planned

Avoid recomputing transforms/bounds/derived world data for unchanged objects. Dirty propagation must integrate with the 10.2 schedule so readers depend on the job that finalizes the corresponding dirty data rather than observing stale state.

The first target should be transform-derived world state. A local transform edit should mark only the affected hierarchy dirty, and the system that finalizes world transforms should own the write phase consumed by Physics/render/spatial readers.

### 10.4 Spatial Query Optimization — planned

Use the stabilized scheduled/dirty world data to reduce broad spatial-query cost. Candidate work includes persistent spatial partitions, batched queries, and parallel read-only query phases. Correctness and deterministic ownership come before changing the spatial structure.

## Compatibility strategy

Phase 10 remains incremental. Existing synchronous update behavior remains the regression baseline while individual systems migrate to explicit dependency edges. MainThread systems preserve legacy thread affinity, and only systems with a proven CPU-only/thread-safe contract should opt into `Worker` execution.

No worker job may call GPU APIs or mutate Editor UI. Shared world data must have one clear writer phase or explicit synchronization before it is consumed in parallel.

## Boundary with later phases

Phase 10 owns CPU world scheduling and world-query optimization. Phase 11 remains responsible for Editor workflow/tooling improvements, while Phase 12 owns production-readiness validation such as crash handling, replay/compatibility checks, and release testing.
