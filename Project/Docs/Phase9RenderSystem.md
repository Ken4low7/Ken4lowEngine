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

### 9.3 Pass Culling — implemented

`RenderGraph` now supports explicit graph sinks through `MarkResourceOutput` and external side effects through `MarkPassSideEffect`. When at least one sink exists, compilation walks required predecessors backwards and removes passes that cannot contribute to a required result.

Culling intentionally distinguishes data requirements from execution-order hazards:

- RAW predecessors stay alive because the surviving reader needs their produced data
- explicit dependencies stay alive because they may represent externally declared ordering/side effects
- WAR and WAW only order passes that already survive; they do not keep overwritten/dead work alive by themselves

Resources with an explicit final state also act as roots because their end-of-graph state is externally observable. For backward compatibility, graphs that declare no output, side-effect pass, or final-state contract keep every pass exactly as before.

After culling, logical resource lifetimes are rebuilt from the surviving compiled schedule rather than declaration order. This is required for Phase 9.4 because transient allocation and aliasing must reason about the actual executable lifetime, not a pass that was removed before execution.

`CompileStats` now exposes declared/executed/culled pass counts plus output-resource and side-effect-root counts, and `IsPassCulled` makes the decision available to the RenderGraph visualizer.

`RenderPipelineController` now marks the BackBuffer as the visible graph output and explicitly preserves `BeginDraw`, `EditorUiBuild`, and `EditorPicking` as side-effect roots. The Performance window reports declared/executed/culled pass counts and the active culling-root counts.

The current controller still uses the conservative explicit pass chain to preserve legacy rendering order, so the normal frame may intentionally report zero culled passes until more implicit ordering is converted into explicit resource/side-effect declarations. The culling algorithm itself is validated independently by Phase 9 contract tests, and future chain relaxation can now be measured without changing the culling source of truth.

### 9.4 Transient Resource Pool + Resource Aliasing — implemented foundation

`RenderGraphTransientPool` converts the culling-adjusted `ResourceLifetime` intervals into reusable physical allocation slots. A transient registration supplies:

- required allocation size
- required alignment
- a compatibility key for resources allowed to share physical memory
- whether aliasing is allowed for that resource

Imported resources are rejected by the transient planner and remain externally owned. Culled or otherwise unused logical resources have no executable lifetime and therefore receive no physical allocation.

The planner sorts active resources by the compiled schedule and reuses a slot only when the previous lifetime ended strictly before the next lifetime begins, the compatibility key matches, the slot is large enough, and both sides permit aliasing. Best-fit selection prefers the smallest compatible reusable slot. Every reuse emits an `AliasingRecord` containing the old resource, new resource, physical slot, and the compiled pass before which ownership changes.

Transient diagnostics now expose:

- registered and active logical resource counts
- physical slot count
- aliasing reuse count
- logical bytes versus physical slot bytes
- peak simultaneously live bytes
- bytes saved relative to one allocation per logical resource
- physical-slot fragmentation above the theoretical peak-live requirement

`RenderGraphD3D12TransientPool` is the physical backend. `DescribeResource` uses `ID3D12Device::GetResourceAllocationInfo` so the graph receives the real D3D12 allocation size/alignment. `BuildHeaps` materializes one reusable D3D12 heap for each planned physical slot, and `CreatePlacedResource` creates every logical resource in that slot at offset zero. This makes non-overlapping logical resources truly share the same heap storage instead of merely sharing bookkeeping.

`RenderGraphD3D12BarrierEmitter::EmitAliasing` emits a native `D3D12_RESOURCE_BARRIER_TYPE_ALIASING` between the old and new placed resources when ownership of a transient slot changes. The planner exposes the exact pass handle for that transition through the culling-adjusted compiled schedule.

The existing Scene/PostEffect render targets are still owner-managed committed resources and are intentionally not switched to placed transient resources in the same change. Physical migration remains opt-in per resource, just like Phase 9.2 barrier migration, so current rendering output stays the regression baseline while the pool/aliasing backend is validated.

### 9.5 Descriptor Management — implemented foundation

`SRVManager` now divides one shader-visible CBV/SRV/UAV heap into two non-overlapping ownership domains:

- persistent descriptors for textures, ImGui, long-lived buffers, and asset-owned views
- transient descriptors reserved for RenderGraph/pass-local descriptor tables

Persistent allocation is prevented from entering the transient range even when the persistent free list is empty. Persistent indices also track their allocated state so double-free and invalid-range frees fail immediately instead of silently inserting duplicate entries into the free queue.

Transient allocation supports contiguous ranges rather than one descriptor at a time. This allows a future RenderGraph pass to reserve an entire descriptor table with one allocation and receive the first CPU/GPU handles plus the descriptor count.

The transient region is partitioned by `DX12CommandManager` Frame Resource count. Each frame receives its own bump-allocation segment. `DX12CommandManager::GetFrameFenceValue` is exposed as that Frame Resource's generation identifier; a segment is reset only after the same frame index returns with a newer submitted fence generation. Because `PrepareFrame` / `WaitAndPrepareFrame` wait before making that Frame Resource current again, the reset occurs only after GPU use of the previous generation is safe.

`AllocateTransient` also rejects requests while the command list is in the submitted/closed window. This prevents an allocator call between submission and the next safe Frame Resource preparation from accidentally treating a newly written fence generation as reusable memory.

This generation-based design is intentionally stronger than guessing that `currentFence + 1` will always correspond to the command list being recorded. Mid-frame `ExecuteAndWait` or other synchronization paths may advance the global fence, while the per-Frame Resource fence remains the authoritative owner generation for the descriptor segment.

Descriptor diagnostics now expose:

- persistent capacity / currently in-use descriptors
- persistent high-water mark
- total transient capacity and minimum capacity per Frame Resource
- transient descriptors currently retained by frame segments
- transient high-water mark
- transient allocation / reclamation counts
- Frame Resource recycle count
- allocator exhaustion count

The heap was expanded while preserving index 0 as reserved. Existing `Allocate` / `Free` users remain on the persistent path, so TextureManager, ImGui, existing render targets, and other owner-managed systems do not need to migrate in the same change. `GpuDeferredReleaseQueue` continues to protect persistent asset descriptors until their retire fence completes.

The current implementation centralizes shader-visible CBV/SRV/UAV lifetime management first. RTV/DSV heaps remain owner-managed CPU-visible descriptor heaps; they can adopt the same transient ownership model when Phase 9.4 placed render targets begin migrating into normal frame execution.

### 9.6 Shader Cache + PSO Cache — implemented foundation

`DXCCompilerManager` now owns a memory-resident DXIL cache. `ShaderCompiler` asks the cache before invoking DXC and stores only successfully compiled shader blobs.

The shader cache key is derived from data that changes compiled output rather than object identity:

- normalized project shader path
- entry point
- shader profile
- the current DXC option set
- root HLSL source bytes
- recursively discovered quoted/angle local include source bytes

Include traversal uses the include path relative to the including source file, records missing inputs deterministically, and stops repeated/cyclic traversal. Because source and local `.hlsli` contents participate directly in the key, editing either automatically creates a cache miss without relying on file timestamps. `InvalidateShader` and `ClearShaderCache` remain available for explicit editor/hot-reload invalidation and teardown.

Shader diagnostics expose request, hit, miss, successful compile, invalidation, clear, and live entry counts.

`PipelineFactory` now caches the complete graphics `PipelineBundle` so a cache hit reuses both the `ID3D12RootSignature` and `ID3D12PipelineState`. The PSO key is structural and deliberately excludes COM pointer addresses. It includes:

- serialized RootSignature bytes
- VS / PS / GS / HS / DS bytecode bytes
- every input-layout semantic and numeric field
- Blend state
- Rasterizer state
- Depth/Stencil state
- RTV/DSV formats
- primitive topology type
- sample mask/count

The key is built before creating the D3D12 RootSignature/PSO, so a hit skips both expensive D3D12 object creations. `ClearCache` and `Finalize` explicitly release cached bundles, while cache diagnostics expose request/hit/miss/create/clear/live-entry counts.

Both caches are currently process-memory caches rather than disk caches. Phase 8 already owns deterministic cooked-data persistence; Phase 9.6 intentionally starts with runtime/editor reuse without adding another persistent cache format. A future persistent PSO cache can layer on top of the same structural inputs without changing call-site ownership.

### 9.7 RenderGraph Visualizer — implemented

`RenderGraphVisualizer` is a read-only ImGui diagnostic window registered beside the existing Render Pipeline performance window in `PerformancePhaseValidation`. `F10` toggles the window without changing graph execution.

The visualizer reads the already compiled data directly from `RenderGraph` and exposes separate tabs for:

- compiled pass order, executed/culled state, side-effect roots, and resource accesses
- resource ownership, output roots, culling-adjusted lifetime, and initial/final resource state
- explicit dependencies plus RAW/WAR/WAW dependency records
- generated Transition/UAV barrier records and their placement
- real `RenderGraphTransientPool` allocation/slot/alias ownership diagnostics
- persistent/transient SRV descriptor capacity, pressure, high-water marks, reclamation, recycle, and exhaustion counters
- DXIL shader cache and graphics PSO cache hit/miss/create/entry statistics

The visualizer does not rerun topological sorting, culling, hazard discovery, barrier generation, or alias planning. New RenderGraph getters expose only the minimal metadata required by diagnostics (`GetPassCount`, `GetResourceCount`, side-effect/output flags, and initial/final states), while pass order continues to come from `GetCompiledPassHandle` and all edges/barriers/lifetimes come from the graph-owned compiled records.

Transient allocation display follows the actual planner state. Because the current Scene/PostEffect resources are still owner-managed committed resources, the normal frame correctly reports no active placed-resource allocation instead of inventing an estimated alias layout. When physical migration begins, the same window will display the real allocation and alias records without a visualizer-side model change.

Shader and PSO cache tabs also provide explicit Debug clear controls. These call the existing cache invalidation APIs; the visualizer remains an observer/control surface rather than a second cache owner.

Phase 9.7 contract tests verify that the visualizer consumes `GetDependencies`, `GetBarrierPlan`, `GetResourceLifetime`, culling metadata, transient-pool diagnostics, descriptor statistics, and shader/PSO cache statistics without invoking private graph compilation algorithms.

With 9.7 implemented, the Phase 9 render-system hardening roadmap is complete at the infrastructure/foundation level. Physical migration of individual committed render targets into graph-owned transient resources remains an incremental follow-up rather than a requirement for the Phase 9 foundation.

## Compatibility strategy

The existing `RenderPipelineController` still explicitly chains passes to preserve rendering order while Phase 9 infrastructure is introduced. Hazard tracking, barrier planning, culling roots, transient allocation planning, alias ownership, Frame Resource-safe transient descriptors, and shader/PSO caches can therefore be validated before removing conservative dependencies or replacing owner-managed resources.

Existing shader and pipeline creation call sites keep their current APIs. Cache reuse is internal to `ShaderCompiler`/`DXCCompilerManager` and `PipelineFactory`, so individual renderers do not need to adopt a new ownership model at the same time.

Once barrier generation and pass-side-effect declarations are stable, explicit chains can be relaxed incrementally and the graph scheduler can expose real parallelism/reordering opportunities. Transient resources can then migrate individually from committed ownership into the shared placed-resource pool and request descriptor tables from the transient SRV range without requiring a renderer-wide switch.

## Validation

Phase 9 tests are added under `Tests/Phase9` and run in TeamDevelopmentCI. C++ Debug/Release translation-unit compilation remains required after every graph API change. Descriptor contract tests verify persistent/transient range separation, Frame Resource fence-generation reuse, submitted-command-list rejection, contiguous allocation, double-free rejection, per-frame capacity partitioning, and pressure diagnostics. Shader/PSO cache contracts verify content-based shader keys, recursive local include dependency tracking, cache-before-compile behavior, explicit invalidation, pointer-independent PSO keys, and cache lookup before D3D12 object creation. Visualizer contracts verify that editor diagnostics read the graph-owned schedule, dependency/barrier/lifetime records, transient allocation state, descriptor pressure, and cache statistics without rebuilding scheduling truth.

## Boundary with later phases

Phase 9 owns renderer scheduling/resource-state/cache infrastructure. Phase 10 owns Parallel World work such as Job Dependency, System Scheduling, Dirty Tracking, and Spatial Query optimization. Phase 11 owns Editor workflow/tooling improvements, and Phase 12 owns production-readiness validation such as crash/replay/compatibility/release testing.
