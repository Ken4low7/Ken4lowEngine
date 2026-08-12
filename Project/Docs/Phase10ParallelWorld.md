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

`DebugScene` initially migrated the existing Play update path as three systems:

1. `ActorWorld.Update`
2. `PhysicsWorld.Update`
3. `ActorWorld.PostPhysicsUpdate`

The previous hard-coded phase calls in `DebugScene::Update` were replaced by one `worldSystemScheduler_.ExecuteAndWait(deltaTime)` call. The systems declare ownership of World object state, transform state, Physics registration, Physics state, and render-facing state, so the Actor -> Physics -> PostPhysics ordering is generated from data hazards instead of manual waits.

All migrated gameplay/Physics systems intentionally remain `MainThread`. Actor/Component update code can touch input, cameras, gameplay callbacks, Physics registration, and render-facing CPU state; Physics event dispatch also calls back into Actor/Component code. Moving these callbacks to workers before their thread-safety contracts are separated would create unsafe parallelism rather than useful parallelism.

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

### 10.3 Dirty Tracking — implemented transform foundation

`SceneComponent` now treats derived WorldTransform data as cached state rather than recomputing every hierarchy on every update.

Each SceneComponent tracks:

- `worldTransformDirty_` — this component needs world position/rotation/scale recomputation
- `subtreeTransformDirty_` — this component or one of its descendants contains dirty transform state
- `worldTransformRevision_` — increments only when this component's WorldTransform is actually rebuilt
- `lastParentWorldTransformRevision_` — detects a changed parent even if a caller reaches the child through a non-root path

All normal local transform setters compare the incoming value and mark dirty only when the value changes. The legacy mutable-reference accessors remain source-compatible, but requesting a mutable `LocalPosition`, `LocalRotation`, or `LocalScale` reference marks the hierarchy dirty before the caller can mutate it. The SceneComponent ImGui transform fields also mark dirty only when a drag edit actually changes a value.

Dirty propagation is directional:

- changing a component marks that component and all descendants WorldTransform-dirty because parent-space changes affect descendants
- the same change marks only the ancestor `subtreeTransformDirty_` flags upward, so unrelated sibling transforms are not invalidated

`RefreshWorldTransformHierarchy()` walks only a dirty subtree. A completely clean component exits before doing transform arithmetic. A dirty component rebuilds its WorldTransform, updates the parent revision it observed, increments its own revision, and then visits affected children. After the hierarchy is clean, repeated per-frame SceneComponent updates retain compatibility but become cheap no-op checks instead of repeated transform recomputation.

Attach/detach operations participate in the same invalidation path. `AttachTo` also rejects attempts to attach a component beneath one of its own descendants, preventing a cyclic hierarchy from causing recursive dirty propagation.

#### Scheduled transform finalization

Phase 10.3 splits the scheduler's old generic transform resource into LocalTransform and WorldTransform ownership. `DebugScene` now schedules five World phases:

1. `ActorWorld.Update`
2. `ActorWorld.FinalizePrePhysicsTransforms`
3. `PhysicsWorld.Update`
4. `ActorWorld.PostPhysicsUpdate`
5. `ActorWorld.FinalizePostPhysicsTransforms`

The pre-Physics finalizer reads LocalTransform state and owns the final WorldTransform write consumed by Physics. This catches local edits made late in Actor/Component update order even when the SceneComponent itself already ran earlier in the frame.

The post-Physics finalizer performs the same dirty-only flush after collision/Physics callbacks. Components such as colliders may still call `RefreshWorldTransform()` immediately when same-function correctness requires it; the scheduled finalizer then sees that hierarchy as clean and does no duplicate transform arithmetic.

These finalizers remain MainThread because SceneComponent hierarchy mutation is not yet a concurrent data structure. The important Phase 10.3 boundary is that Physics/render readers now depend on an explicit transform-finalization writer phase. A later parallel transform job can replace the implementation behind that phase without changing data ownership semantics.

The ActorWorld validation window reports the number of dirty SceneComponents observed and the number actually recomputed in both pre-Physics and post-Physics finalization. This makes the optimization measurable: a stable frame should converge toward zero recomputed transforms even though the world still contains many SceneComponents.

#### Validation

`Tests/Phase10/test_transform_dirty_tracking.py` protects the transform dirty contract, including:

- self/subtree dirty flags and revision tracking
- setter, mutable-reference, and ImGui invalidation paths
- downward descendant invalidation and upward subtree propagation
- clean-hierarchy fast-path
- parent revision tracking
- pre-Physics and post-Physics finalizer placement in the SystemScheduler graph
- separate LocalTransform/WorldTransform resource ownership
- debug recomputation diagnostics

Debug/Release translation-unit compilation remains the authoritative C++ integration check for the full SceneComponent/Actor/Editor dependency surface.

### 10.4 Spatial Query Optimization — planned

Use the stabilized scheduled/dirty world data to reduce broad spatial-query cost. Candidate work includes persistent spatial partitions, batched queries, and parallel read-only query phases. Correctness and deterministic ownership come before changing the spatial structure.

## Compatibility strategy

Phase 10 remains incremental. Existing synchronous update behavior remains the regression baseline while individual systems migrate to explicit dependency edges. MainThread systems preserve legacy thread affinity, and only systems with a proven CPU-only/thread-safe contract should opt into `Worker` execution.

No worker job may call GPU APIs or mutate Editor UI. Shared world data must have one clear writer phase or explicit synchronization before it is consumed in parallel.

## Boundary with later phases

Phase 10 owns CPU world scheduling and world-query optimization. Phase 11 remains responsible for Editor workflow/tooling improvements, while Phase 12 owns production-readiness validation such as crash handling, replay/compatibility checks, and release testing.
