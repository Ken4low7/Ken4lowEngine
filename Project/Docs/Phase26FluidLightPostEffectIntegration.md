# Phase 26 — Fluid / Light / PostEffect Integration

## Status

**Phase26 repository integration is complete through 26.10.**

Phase26 extends the Niagara-like VFX Graph with subsystem output nodes while preserving the ownership boundaries established in earlier phases.

## Architecture

```text
.vfxgraph.json
      |
      v
VfxGraphCompiler
      |
      +--> Particle modules/renderers ----------> Phase13 GpuParticleEffectRuntime
      |
      `--> Fluid/Light/PostEffect Output nodes --> generated Phase18 VfxCueDesc
                                         |
                                         v
                                   VfxCueRuntime
                                         |
               +-------------------------+-----------------------+
               |                         |                       |
               v                         v                       v
       FluidEmitterComponent      LightComponent        PostEffectManager
       Phase16 / Phase17          LightManager sync     ref-count adapter
```

There is no second fluid simulator, light buffer, post-effect chain, or GPU particle backend in Phase26.

## Roadmap

- [x] 26.1 Append FluidOutput / LightOutput / PostEffectOutput node types without changing existing enum values
- [x] 26.2 Fluid2D / Volumetric3D output authoring
- [x] 26.3 Transient point-light output authoring
- [x] 26.4 PostEffect activation output authoring
- [x] 26.5 Graph user-parameter bridge for intensity/radius
- [x] 26.6 Graph compiler -> Phase18 integration Cue lowering
- [x] 26.7 One-shot and loop integration lifecycle
- [x] 26.8 Graph Editor authoring / live preview
- [x] 26.9 Serializer + showcase asset
- [x] 26.10 Regression contracts / CI

## Output nodes

### FluidOutput

`FluidOutput` lives in the Update stage and lowers to either a `Fluid2D` or `VolumetricFluid` Phase18 track. It exposes local offset, source velocity, duration, radius, velocity strength, density, temperature, and falloff exponent.

The node creates a transient `FluidEmitterComponent` through the existing Phase18 adapter. A Volumetric3D source therefore still uses the Phase17 manager and a Fluid2D source still uses the Phase16 manager.

This is an emitter-position source, not per-particle GPU splatting. Phase26 deliberately does not read particles back to CPU or introduce particle-to-fluid deposition kernels.

### LightOutput

`LightOutput` lives in the Render stage and lowers to the existing transient Point `LightComponent` adapter. ActorWorld -> LightManager synchronization remains the only light-buffer path.

### PostEffectOutput

`PostEffectOutput` lives in the Render stage and lowers to the existing Phase18 PostEffect adapter. Overlapping graph instances therefore inherit its effect-name reference counting.

The current generic `IPostEffect` interface still has no universal numeric weight setter, so `weight * IntensityScale` is an activation gate exactly like Phase18. Effect-specific numeric parameter APIs are outside this phase.

## User parameters

Fluid and Light outputs can name existing Graph user parameters for `intensityParameter` and `radiusParameter`. PostEffect can name `intensityParameter`.

The integration compiler converts the Graph parameter declarations to Phase18 Cue parameters and emits `IntensityScale` / `RadiusScale` bindings. `VfxGraphRuntime::SetFloatParameter(handle, ...)` forwards the live value to both the Phase13 particle handle and the Phase18 integration Cue handle.

One-shot Graph playback still returns only success/failure, so one-shot instance parameters use authored defaults. This preserves the pre-Phase26 one-shot API contract.

## Runtime lifecycle

For graphs with integration outputs the compiler creates two internal Cue descriptions:

- `__VFX_GRAPH__<GraphName>__OneShot`
- `__VFX_GRAPH__<GraphName>__Loop`

One-shot playback starts the generated one-shot Cue and the existing Phase13 particle effect. Loop playback keeps the existing Phase13 loop handle and stores the Phase18 Cue handle beside it. Position updates, parameter updates, and Stop are forwarded to both handles.

If a loop integration Cue cannot start, the particle loop is rolled back instead of leaving half of the effect alive.

## Scope boundary

Phase26 does not implement:

- per-particle Fluid deposition;
- per-particle dynamic lights;
- GPU -> CPU event readback;
- a second PostEffect chain;
- bounds / LOD / VFX budget / culling policy.

Bounds, LOD, budget, and culling remain Phase27.
