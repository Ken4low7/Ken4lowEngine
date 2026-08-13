# Phase 14 - GPU Driven Particle Rendering

## Goal

Phase 13 completed the Effect Authoring / Gameplay Runtime V1 boundary. Phase 14 removes the remaining full-buffer graphics scan and moves draw-count generation and alpha ordering onto the GPU.

## 14.1 Alive / RenderGroup Compaction

`GpuParticleCompact.CS.hlsl` scans the global particle pool and keeps only particles that:

- are alive (`lifeTime > 0`)
- have visible alpha
- belong to the current render group

The compacted result stores original particle indices, so simulation storage does not need to be moved or duplicated.

## 14.2 Visible Particle Index Buffer

`GpuParticleBuffers` owns a `uint` visible-index buffer with one slot per maximum particle. Sprite and Mesh vertex shaders now resolve:

`SV_InstanceID -> visible particle index -> Particle`

This removes dead and foreign-material instances before vertex work.

## 14.3 GPU Generated Indirect Arguments

The compaction shader atomically increments the indirect `InstanceCount` while it writes visible indices. CPU code only provides primitive/index count and the render-group identity.

No particle-count readback is required.

## 14.4 ExecuteIndirect

Sprite and Mesh draws use D3D12 command signatures:

- `D3D12_INDIRECT_ARGUMENT_TYPE_DRAW`
- `D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED`

The old direct `DrawInstanced` / `DrawIndexedInstanced` full-pool path is no longer used by `GpuParticleRenderer`.

## 14.5 Alpha Depth Sort

Normal alpha blending runs a GPU Bitonic Sort on the compacted visible-index buffer.

The sort key is `-NDC depth`, which produces back-to-front particle order while retaining a deterministic particle-index tie breaker. Additive and multiply paths skip sorting because they do not require the same conventional alpha ordering.

The current maximum particle count is 131072, a power of two, so the fixed Bitonic network has 153 possible compare/swap dispatch stages. The sort shader reads the GPU-generated visible `InstanceCount`, rounds it to the next power of two, and makes stages/ranges outside that active capacity no-op. This keeps correctness without a CPU count readback while reducing sparse alpha-effect compare work.

## UAV Clear Synchronization

D3D12 UAV clears are explicitly separated from following dispatch work by UAV barriers. `UAVManager` also keeps a non-shader-visible CPU descriptor mirror specifically for `ClearUnorderedAccessViewXxx`, while compute binding continues to use the shader-visible heap.

The compaction-to-sort barrier covers both visible indices and indirect arguments because the sort shader reads both resources.

## Resource State Flow

Per render group:

1. Particle / visible-index / indirect-argument resources enter UAV state.
2. Indirect args and visible-index scratch are cleared.
3. Alive/render-group compaction runs.
4. Alpha-only depth sorting runs when required.
5. Particle and visible-index resources transition to non-pixel SRV state.
6. Indirect args transition to `INDIRECT_ARGUMENT`.
7. Graphics executes the generated command.

Repeated draw groups transition the scratch resources back to UAV state before rebuilding them.

## 14.6 Stress / Performance Validation Tooling

Phase 14 includes two complementary measurements.

### GPU command workload statistics

`GpuParticleRenderer::GpuDrivenStatistics` records CPU-side command emission counts without GPU readback:

- draw requests
- compaction dispatches
- total particle slots scanned by compaction
- alpha-sort render groups
- alpha-sort dispatches
- indirect draws

`GpuParticleWorkloadEstimator` provides the same theoretical workload math as a portable header-only utility. Its runtime test verifies the current 131072-particle pool, 512 compaction thread groups per render group, and 153 Bitonic stages per alpha group.

### No-stall GPU timestamp timings

`GpuParticleRenderer` owns a bounded timestamp query/readback ring keyed by FrameResource and its fence generation. It exposes last/EMA/max/sample-count metrics for:

- compaction
- alpha sort
- graphics / ExecuteIndirect section
- total GPU-driven render-group work

Resolved timestamps are mapped only when that FrameResource is reused after the existing frame-fence synchronization. The profiler does not call `ExecuteAndWait` or `WaitAndReset`, so profiling itself does not introduce a new GPU synchronization point.

The profiler supports up to 256 render-group samples per frame and silently stops adding timestamp samples beyond that bound while normal rendering continues.

## Validation

Phase 14 tests protect:

- visible-index and indirect-argument buffer ownership
- alive/render-group filtering
- compacted-index vertex shader lookup
- ExecuteIndirect usage
- alpha depth-sort ordering and Bitonic network semantics
- adaptive visible-count sort capacity
- UAV clear CPU-descriptor mirror
- explicit UAV/resource-state synchronization
- portable workload-estimator runtime math
- randomized workload stress bounds
- bounded no-stall GPU timestamp profiler contracts

The main CI runs all Phase 14 tests and directly DXC-compiles the GPU particle shader set, including Compact and Sort compute shaders, before Debug/Release C++ translation-unit compilation.

## 14.7 Final Cleanup / Acceptance

Code-side Phase 14 completion criteria are:

- no old full-pool direct instance draw path in `GpuParticleRenderer`
- no obsolete Phase 13 vertex-stage dead/material rejection dependency
- compaction, alpha sort, indirect draw, resource-state and descriptor contracts covered by CI
- workload and GPU timing diagnostics available for real hardware profiling
- Debug and Release translation-unit compilation green

All automated criteria above are now green on GitHub Actions run #449 for commit `82db34fe7544e221a5c7a1984c1254c2c26f5d38`.

The final machine-dependent acceptance step is an actual Windows/DX12 visual smoke and stress run on target hardware. Use `Docs/Phase14WindowsDx12Acceptance.md` as the repeatable checklist and baseline record. Record the exposed timings under representative effects instead of inventing universal GPU budgets. If alpha-heavy scenes show sort pressure, the next optimization candidate is a tiled/radix or GPU-generated dynamic-dispatch sort rather than reducing ordering correctness.

## Phase 14 Status

Automated implementation and validation are complete. Phase 14 remains intentionally unmerged until the Windows/DX12 acceptance checklist has been run on real hardware and any visual/runtime regressions have been addressed.
