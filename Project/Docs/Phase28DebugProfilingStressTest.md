# Phase28 — Debug / Profiling / Stress Test

Phase28 adds production-facing observability to the existing Phase20-27 VFX stack. It does not create a second particle backend, a second VFX budget owner, or a synchronous GPU readback path.

## Goals

- Capture a compact rolling history of VFX and frame timing statistics.
- Reuse the existing `GameTimer`, `GpuParticleManager`, `VfxGraphRuntimeStats`, and `VfxRuntimeStats` counters.
- Visualize current values, history, peaks, budget pressure, culling, and LOD activity in the editor.
- Provide a repeatable stress runner that submits one-shot and loop VFX Graph instances on a grid.
- Keep the diagnostics path non-blocking: no GPU fence wait and no synchronous GPU readback are introduced.
- Preserve all Phase22 event, Phase23 renderer, Phase24 execution graph, Phase26 integration, and Phase27 scalability contracts.

## Diagnostics collector

`VfxGraphDiagnostics` is a lightweight singleton diagnostics layer. `GameApplication` calls `CaptureFrame()` once per frame after VFX Graph scalability and VFX Cue runtime updates.

The collector stores up to 240 completed-frame samples in a ring buffer. Each sample records:

- completed frame / update / draw / present time from `GameTimer`
- GPU particle emitter count and active emitter count
- estimated active particle count
- particle draw-call count
- emit dispatch delta for the frame
- active VFX Graph loop count / loop cost / start cost
- VFX Graph play, culling, budget rejection, and LOD selection deltas
- active VFX Cue instance / track counts
- cue track-start, budget-rejection, and budget-delay deltas

`BuildSummary()` calculates moving-window averages and peaks from that same history. Counter deltas tolerate counter reset/wrap by treating a lower current value as a fresh baseline.

## GPU synchronization boundary

Phase28 intentionally uses CPU-visible counters already maintained by the production runtime. `estimatedActiveParticles` remains the existing CPU-side estimate; it is not presented as an exact GPU occupancy count.

No fence wait, command-queue flush, staging-copy readback, or other synchronous GPU synchronization is added for the profiler. Exact GPU timestamp queries or asynchronous occupancy/event readback can be added later only if they preserve this non-blocking runtime contract.

## Diagnostics editor window

`VfxDiagnosticsWindow` is shown alongside the Phase25 VFX Graph Editor and contains four tabs:

### Overview

Shows the latest frame sample plus rolling averages and peak particle / draw / graph / cue values.

### History

Plots frame, update, draw, and estimated particle history for the 240-frame ring buffer. History can be reset without affecting runtime VFX state.

### Stress

Runs a configurable VFX Graph stress burst using the real `VfxGraphRuntime` path. One-shots and loops are distributed on a grid, with hard safety caps of 512 one-shots and 128 loops per submission. Loop handles are retained so the stress run can be stopped explicitly.

The Phase27 `ScalableIntegratedExplosion` sample can be loaded directly for LOD / culling / budget stress checks.

### Budget

Edits the existing `VfxRuntimeBudget` owned by `VfxCueRuntime` and displays current graph and cue pressure. No Phase28-only budget singleton is introduced.

## Stress result

`VfxGraphStressResult` reports:

- requested and successful one-shot starts
- requested and successful loop starts
- stopped loop count
- graph budget rejection delta
- graph culling delta
- estimated active particles after submission
- active emitter count after submission
- whether the requested graph was registered

The stress runner is a diagnostic workload generator, not a benchmark harness with deterministic GPU-time pass/fail thresholds. Machine-dependent frame-time thresholds are deliberately not baked into static CI.

## CI contracts

Phase28 static tests verify:

- 240-frame ring-history and summary APIs
- reuse of existing timer/runtime/particle counters
- frame-delta collection for graph/cue/dispatch counters
- stress caps, grid submission, retained loop handles, and stop path
- Overview / History / Stress / Budget editor surfaces
- Application capture ordering after VFX runtime updates
- absence of the temporary integration helper/workflow
- Phase29 remains outside Phase28 scope

The dedicated Phase28 CI also reruns Phase13 and Phase20-27 VFX regression contracts.

## Phase boundary

Phase28 does **not** add the Phase29 Production VFX Library. Reusable production effect presets, library organization, authoring conventions, and final showcase assets belong to Phase29.
