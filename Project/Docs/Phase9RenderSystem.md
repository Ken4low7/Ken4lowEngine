# Phase 9 — Render System Hardening

## Goal

Phase 9 turns the Phase 6 logical Render Graph into a renderer-owned scheduling/resource-state system that can reason about hazards, barriers, transient GPU memory, descriptors, and reusable shader/PSO state.

The existing fixed rendering behavior remains the regression baseline while each optimization is introduced behind validated graph metadata.

## Planned steps

### 9.1 Resource Access State + RAW/WAR/WAW hazards — implemented

`RenderGraph` now exposes an explicit resource access model:

- `AccessType::Read`
- `AccessType::Write`
- `AccessType::ReadWrite`
- `ResourceState` values for Common / RenderTarget / Depth / ShaderResource / UAV / Copy / Present usage

The legacy `AddPass(reads, writes, callback)` API remains available and is translated to `ResourceAccess` records with an unknown physical state, so Phase 9 can be introduced without rewriting the current pipeline in one change.

Dependency generation no longer serializes every access through one `lastAccess` pointer. The compiler tracks:

- previous writer per resource
- all readers since the previous write

Only real hazards generate resource ordering edges:

- RAW — Read After Write
- WAR — Write After Read
- WAW — Write After Write

Read/Read access is intentionally not ordered by the resource graph. Explicit pass dependencies remain supported for side effects and for the current conservative pipeline chain.

`DependencyRecord` exposes the pass pair, resource, and hazard type so later barrier generation and the RenderGraph visualizer use the same compiled truth instead of reconstructing dependencies independently.

Compile statistics now separately count RAW/WAR/WAW hazards while `dependencyCount` continues to represent unique graph edges.

### 9.2 D3D12 Barrier Generation — implemented

The graph compiler now creates a deterministic barrier plan after topological sorting. Resources can declare both an initial and final `ResourceState`, while each pass declares the state required for each resource access.

The generated plan contains:

- transition barriers before passes when the previous known state differs from the requested state
- UAV ordering barriers for consecutive unordered-access usage when either access writes
- final-state transitions after the graph
- the target resource/pass and before/after state for every barrier

`CompileStats` records transition count, UAV barrier count, and accesses whose physical state is still unknown.

Unknown state is handled conservatively. The graph never invents a D3D12 transition from an unknown state; an unknown-state access invalidates graph-side state knowledge until a later explicitly declared state establishes a new known state. Conflicting known states for the same resource inside one pass fail graph compilation.

`RenderGraph::Execute(BarrierCallback, ...)` emits planned records immediately before the owning pass and final transitions after the graph. `RenderGraphD3D12BarrierEmitter` is the D3D12 backend adapter: it binds a logical `ResourceHandle` to an `ID3D12Resource`, converts every known graph state into `D3D12_RESOURCE_STATES`, and emits either a transition or UAV `ResourceBarrier` on the supplied graphics command list.

Physical emission is opt-in per bound resource. The existing render targets still contain manual D3D12 transitions and remain on the legacy `Unknown` state path, so Phase 9 never emits a second transition on top of an owner-managed barrier. Physical ownership can now migrate resource-by-resource by binding that target to the graph emitter and removing its old local transition at the same time.

### 9.3 Pass Culling — next

Mark graph outputs/side-effect passes, walk dependencies backwards, and skip passes whose outputs cannot contribute to a required sink.

Editor UI, readback/picking, Present, and other external side effects must be explicitly preserved.

### 9.4 Transient Resource Pool + Resource Aliasing

Convert logical transient lifetimes into reusable D3D12 allocations:

- transient resource descriptors
- lifetime interval analysis
- reusable heap blocks
- alias-compatible allocation
- aliasing barriers
- high-water/fragmentation diagnostics

Imported resources remain externally owned.

### 9.5 Descriptor Management

Centralize transient/pass descriptor allocation and lifetime tracking instead of growing ad-hoc descriptor ownership in individual render paths.

### 9.6 Shader Cache + PSO Cache

Cache shader bytecode and graphics/compute PSOs from deterministic keys derived from shader inputs and pipeline state. Cache invalidation must remain explicit and observable in Debug builds.

### 9.7 RenderGraph Visualizer

Expose compiled graph state in the editor:

- pass order
- RAW/WAR/WAW edges
- resource access/state
- resource lifetime
- culled passes
- generated barriers
- transient allocation/alias ownership

The visualizer is diagnostic and must not become a second source of scheduling truth.

## Compatibility strategy

The existing `RenderPipelineController` still explicitly chains passes to preserve rendering order while Phase 9 infrastructure is introduced. Hazard tracking and barrier planning can therefore be validated before removing conservative dependencies or replacing owner-managed resource transitions.

Once barrier generation and pass-side-effect declarations are stable, explicit chains can be relaxed incrementally and the graph scheduler can expose real parallelism/reordering opportunities.

## Validation

Phase 9 tests are added under `Tests/Phase9` and run in TeamDevelopmentCI. C++ Debug/Release translation-unit compilation remains required after every graph API change.

## Boundary with later phases

Phase 9 owns renderer scheduling/resource-state/cache infrastructure. Phase 10 owns Parallel World work such as Job Dependency, System Scheduling, Dirty Tracking, and Spatial Query optimization. Phase 11 owns Editor workflow/tooling improvements, and Phase 12 owns production-readiness validation such as crash/replay/compatibility/release testing.
