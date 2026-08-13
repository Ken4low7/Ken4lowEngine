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

The current maximum particle count is 131072, a power of two, so the fixed-size Bitonic network requires 153 compare/swap dispatch passes. This is a correctness-first implementation with no CPU synchronization. A future performance pass may replace it with radix/tiled sorting if profiling shows alpha-heavy effects are sort-bound.

## UAV Clear Synchronization

D3D12 UAV clears are explicitly separated from following dispatch work by UAV barriers. `UAVManager` also keeps a non-shader-visible CPU descriptor mirror specifically for `ClearUnorderedAccessViewXxx`, while compute binding continues to use the shader-visible heap.

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

## Validation

`Tests/Phase14/test_gpu_driven_particle_rendering.py` protects:

- visible-index and indirect-argument buffer ownership
- alive/render-group filtering
- compacted-index vertex shader lookup
- ExecuteIndirect usage
- alpha depth-sort contract
- UAV clear CPU-descriptor mirror
- explicit UAV/resource-state synchronization

The main CI also runs Phase 14 contract tests and compiles the GPU particle shader set configured in the workflow.

## Remaining Phase 14 Work

### 14.6 Stress / Performance Validation

Run representative particle loads and record:

- GPU particle update time
- compaction time
- alpha sort time
- graphics particle time
- active render-group count
- alpha render-group count
- peak particle count

Targets should be based on the actual target GPU rather than guessed desktop budgets.

### 14.7 Final Cleanup

After stress validation:

- remove obsolete Phase 13 early-rejection assumptions
- tighten comments and diagnostics
- verify Debug and Release CI
- perform an actual Windows/DX12 visual smoke test
- then merge Phase 14 only after those checks are green
