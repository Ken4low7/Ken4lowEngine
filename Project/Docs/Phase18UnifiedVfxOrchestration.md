# Phase 18 — Unified VFX Orchestration

## Goal

Phase 18 adds an orchestration layer above the effect systems that already exist in the engine.

Phase 13 owns authored GPU Particle effects. Phase 16/17 own 2D/3D GPU Fluid simulation. Existing Light, PostEffect, and Camera systems remain the source of truth for their own runtime behavior. Phase 18 does **not** merge those systems into one giant renderer. It introduces a small VFX Cue layer that schedules them together.

```text
Gameplay
   |
   | Play("Explosion", worldTransform)
   v
VfxCueRuntime
   |
   v
VfxCueProgram
   |
   +--> Particle adapter ------> GpuParticleEffectRuntime
   +--> Fluid2D adapter -------> GpuFluidManager
   +--> Volumetric adapter ----> GpuVolumetricFluidManager
   +--> Light adapter ---------> Light system
   +--> PostEffect adapter ----> PostEffectManager
   `--> CameraShake adapter ---> Camera system
```

Gameplay should select the meaning of an effect, not know that an explosion consists of several particle emitters, a smoke volume, a flash light, bloom, and camera shake.

## Roadmap

- [x] 18.1 VFX Cue Data Model / Schema / Compiler Foundation
- [ ] 18.2 Cue Scheduler / Instance Handle / Play-Stop API
- [ ] 18.3 GPU Particle Track Adapter
- [ ] 18.4 Fluid2D / Volumetric Fluid Track Adapter
- [ ] 18.5 Light Track Adapter
- [ ] 18.6 PostEffect Track Adapter
- [ ] 18.7 Camera Shake Track Adapter
- [ ] 18.8 User Parameters / Track Bindings
- [ ] 18.9 VFX Timeline Editor / Preview / Hot Reload
- [ ] 18.10 Diagnostics / Budget / Stress Test / Closure

## 18.1 — Data Model

`Engine/Vfx/Data/VfxCueTypes.h` introduces explicit track payloads instead of one catch-all parameter structure.

Supported authoring track types are reserved from the start:

- `Particle`
- `Fluid2D`
- `VolumetricFluid`
- `Light`
- `PostEffect`
- `CameraShake`

Common track data owns only:

- track name
- enabled flag
- start time
- duration
- local position offset

Subsystem-specific values live in typed payloads stored in `VfxCueTrackPayload` (`std::variant`). This prevents Particle implementation fields from leaking into Fluid/Light tracks and gives future adapters a narrow input contract.

### Particle Track

The Cue does not copy Phase 13 emitter settings. It references the authored Particle Effect by `effectAssetPath` and `effectName`.

```text
VFX Cue Particle Track
        |
        `--> Phase13 .effect.json
```

This keeps Particle authoring owned by the existing Effect Asset workflow.

### Fluid Track

2D and Volumetric tracks share the same world/local injection intent:

- local velocity
- radius
- velocity strength
- density rate
- temperature rate
- falloff exponent

The track type determines whether the future runtime adapter sends that source to Phase16 or Phase17.

### Light / PostEffect / CameraShake

18.1 records only the small set of values required to define scheduling intent. Runtime ownership remains in each existing subsystem and will be connected in 18.5–18.7.

## 18.1 — Schema

Cue assets use `.vfx.json` and begin with `schemaVersion = 1`.

Example:

```json
{
    "schemaVersion": 1,
    "cueName": "Phase18Explosion",
    "loop": false,
    "duration": 1.2,
    "tracks": [
        {
            "name": "ParticleCore",
            "type": "Particle",
            "startTime": 0.0,
            "duration": 0.0,
            "particle": {
                "effectAssetPath": "Resources/Effects/Phase13/Explosion.effect.json",
                "effectName": "Phase13Explosion"
            }
        }
    ]
}
```

`VfxCueSerializer` rejects unknown schema versions and unknown track type strings instead of silently converting them to another effect type. A Cue can drive multiple subsystems, so partial or accidental execution is considered more dangerous than a failed load.

The serializer owns JSON/file compatibility only. Semantic validation is intentionally delegated to the compiler.

## 18.1 — Compiler

`VfxCueCompiler` converts editable `VfxCueDesc` data into a normalized `VfxCueProgram`.

The compiler:

1. validates schema/cue/track counts;
2. rejects non-finite or negative timing values;
3. validates the payload type against the declared track type;
4. validates subsystem-specific required values;
5. skips explicitly disabled tracks;
6. calculates the final Cue duration from authored duration and track end times;
7. stable-sorts instructions by start time and original track index.

The runtime therefore does not need to repeatedly inspect authoring order or revalidate payload variants every frame.

```text
Authoring Tracks
  [0] Light    @ 0.00
  [1] Smoke    @ 0.03
  [2] Particle @ 0.00
           |
           v compile
Runtime Program
  Light    @ 0.00
  Particle @ 0.00
  Smoke    @ 0.03
```

Equal-time instructions preserve their original source-track order.

## Sample Asset

`Resources/Vfx/Phase18/Explosion.vfx.json` demonstrates a single Cue containing:

- Phase13 Explosion Particle Effect
- Phase17 Volumetric Fluid smoke injection
- flash Light
- Bloom PostEffect weight
- Camera Shake

18.1 only loads/compiles this asset. Actual cross-system playback begins in 18.2 and the per-track adapters are added in 18.3–18.7.

## Validation

`Tests/Phase18/test_vfx_cue_foundation.py` protects the static architecture contract:

- typed track enum and payloads;
- Particle/Fluid/Light/PostEffect/CameraShake schema names;
- schema version and track-count guards;
- strict unknown-track rejection;
- compiler payload validation;
- disabled-track removal;
- stable time sorting;
- compiled duration expansion;
- Phase13 Particle Effect reference reuse;
- sample Cue and build registration.

The Python contract is added in 18.1, but Windows C++ compilation/runtime validation remains required on the development machine.

## Next Implementation Target

**18.2 — Cue Scheduler / Instance Handle / Play-Stop API**

The next step will introduce a small runtime facade with an API shaped like:

```cpp
VfxCueRuntime* vfx = VfxCueRuntime::GetInstance();
vfx->LoadCue("Resources/Vfx/Phase18/Explosion.vfx.json");

VfxCueHandle handle = vfx->Play("Phase18Explosion", worldPosition);
vfx->Stop(handle);
```

18.2 will schedule instruction start/end events but will not yet know Particle or Fluid internals. Those dependencies enter through track adapters from 18.3 onward.
