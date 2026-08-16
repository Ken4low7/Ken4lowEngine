# Phase27 — LOD / Bounds / Budget / Culling

Phase27 makes the Phase20-26 VFX Graph stack practical for production scenes with many simultaneous effects. It does not add a second particle, fluid, lighting, or post-effect backend.

## Goals

- Add graph-level Bounds authoring with Automatic and FixedSphere modes.
- Reuse the existing Phase15 `BoundingSphere`, `Frustum`, and active camera data for visibility decisions.
- Apply Near / Mid / Far LOD to real GPU particle emission work.
- Apply the same runtime LOD scale to Phase26 Fluid / Light / PostEffect integration intensity.
- Reuse and extend the existing Phase18 `VfxRuntimeBudget` instead of introducing a second budget owner.
- Cull one-shot effects before backend work is started.
- Re-evaluate loop effects every frame so moving effects and cameras change LOD automatically.

## Bounds

`VfxGraphScalabilityDesc` is stored at graph level.

### Automatic

The compiler estimates a conservative local `BoundingSphere` from authored particle data:

- spawn sphere / box extent
- maximum lifetime
- initial velocity and random velocity
- gravity travel
- initial particle size
- ribbon / trail length
- sub-emitter travel
- FluidOutput radius / velocity travel
- LightOutput range

This is intentionally conservative. It is safer to draw an effect slightly longer than necessary than to cut visible particles.

### FixedSphere

Artists can override the automatic result with a local center and radius. This is useful for unusual effects whose motion cannot be estimated accurately from the authored modules.

The asset schema remains version 1; older graphs that do not contain `scalability` load the default values.

## LOD

Each graph has three runtime tiers:

- Near: scale `1.0`
- Mid: `lodMidScale`
- Far: `lodFarScale`

The Phase13 GPU particle runtime accepts an optional runtime scale. It scales SpawnRate, BurstCount, and Phase22 sub-emitter count while continuing to use the same GPU particle pool, free-list, execution graph, render paths, and event model.

Phase26 integration cues use `VfxCueRuntime::SetRuntimeScale`. The existing adapters receive the scaled `intensityScale`, so Fluid density/temperature contribution, transient Light intensity, and PostEffect weight are reduced without creating alternate subsystem implementations.

## Culling

`VfxGraphRuntime` uses `CameraManager::GetActiveCameraPosition()` and `GetActiveViewProjectionMatrix()` plus the existing `Frustum` implementation.

One-shot graphs are rejected before particle/integration playback when their world-space Bounds are outside the frustum or beyond `maxDrawDistance`.

Loop graphs use soft culling. They keep their logical runtime handle so gameplay can move them back into view, but their runtime scale becomes zero while culled. This stops new GPU particle emission and drives integration intensity to zero. Existing particles are allowed to finish naturally instead of being force-killed.

## Budget

Phase27 extends the existing `VfxRuntimeBudget` with:

- `maxVfxGraphStartCostPerFrame`
- `maxActiveVfxGraphLoopCost`

Each graph owns an authored integer `budgetCost`. `VfxGraphRuntime::BeginFrame()` resets the per-frame start cost before gameplay updates. Loop cost remains active until the loop is stopped.

This keeps budget ownership in the existing Phase18 VFX runtime configuration instead of introducing an unrelated graph-only global budget singleton.

## Runtime statistics

`VfxGraphRuntimeStats` now tracks:

- culled one-shots
- budget-rejected plays
- Near / Mid / Far LOD selections
- loop LOD scale changes
- loop cull transitions
- per-frame graph start cost
- active loop count / cost

The detailed profiling UI, graphs, stress visualizers, and frame-history diagnostics remain Phase28 work.

## Editor

The Phase25 VFX Graph Editor now exposes a `Phase27 Scalability` section containing Bounds mode, fixed bounds, frustum culling, max draw distance, LOD distances/scales, and budget cost. Live compile/preview continues to use the production `VfxGraphRuntime` path.

## Sample

`Resources/VfxGraph/Phase27/ScalableIntegratedExplosion.vfxgraph.json` combines:

- GPU particles
- Volumetric Fluid
- transient Light
- Bloom PostEffect
- automatic Bounds
- frustum/distance Culling
- Near/Mid/Far LOD
- graph Budget cost

## Phase boundary

Phase27 intentionally does **not** add the Phase28 profiling UI, persistent frame-history telemetry, GPU readback diagnostics, or stress-test dashboard. Those belong to Phase28 — Debug / Profiling / Stress Test.