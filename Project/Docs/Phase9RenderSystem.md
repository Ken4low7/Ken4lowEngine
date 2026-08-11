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

### 9.2 D3D12 Barrier Generation — next

Use declared `ResourceState` values and compiled pass order to generate a transition plan:

- imported resource initial/final states
- transition barriers between incompatible states
- UAV ordering barriers where required
- validation for unknown state declarations
- graph diagnostics before executing D3D12 commands

Physical barriers will initially be emitted on the graphics command list without changing queue scheduling.

### 9.3 Pass Culling

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

The existing `RenderPipelineController` still explicitly chains passes to preserve rendering order while Phase 9 infrastructure is introduced. Hazard tracking can therefore be validated before removing conservative dependencies.

Once barrier generation and pass-side-effect declarations are stable, explicit chains can be relaxed incrementally and the graph scheduler can expose real parallelism/reordering opportunities.

## Validation

Phase 9 tests are added under `Tests/Phase9` and run in TeamDevelopmentCI. C++ Debug/Release translation-unit compilation remains required after every graph API change.

## Boundary with later phases

Phase 9 owns renderer scheduling/resource-state/cache infrastructure. Phase 10 owns Parallel World work such as Job Dependency, System Scheduling, Dirty Tracking, and Spatial Query optimization. Phase 11 owns Editor workflow/tooling improvements, and Phase 12 owns production-readiness validation such as crash/replay/compatibility/release testing.
