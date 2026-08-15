# Phase 18 — Unified VFX Orchestration

## Status

**Phase 18 status: repository integration complete through 18.10.**

Windows / Visual Studio / actual D3D12 runtime validation is still required on the development machine. The repository changes, runtime wiring, authoring path, diagnostics, static regression contracts, and sample Cue are complete.

## Goal

Phase 18 adds a small orchestration layer above the effect systems that already exist in the engine.

- Phase 13 remains the owner of authored GPU Particle effects.
- Phase 16 remains the owner of 2D GPU Fluid simulation.
- Phase 17 remains the owner of 3D Volumetric Fluid simulation/rendering.
- LightComponent / LightManager remain the owner of lighting.
- PostEffectManager remains the owner of post effects.
- Camera / DebugCamera remain the owner of view state.

Phase 18 does **not** merge those systems into one giant renderer. It schedules them together through one gameplay-facing Cue.

```text
Gameplay
   |
   | Play("Phase18Explosion", worldPosition)
   v
VfxCueRuntime
   |
   v
VfxCueProgram
   |
   +--> Particle adapter ------> GpuParticleEffectRuntime
   +--> Fluid2D adapter -------> FluidEmitterComponent -> Phase16
   +--> Volumetric adapter ----> FluidEmitterComponent -> Phase17
   +--> Light adapter ---------> transient LightComponent
   +--> PostEffect adapter ----> PostEffectManager
   `--> CameraShake adapter ---> Camera / DebugCamera
```

Gameplay selects the meaning of an effect. It does not need to know that an explosion contains particles, smoke, a flash light, bloom, and camera shake.

## Roadmap

- [x] 18.1 VFX Cue Data Model / Schema / Compiler Foundation
- [x] 18.2 Cue Scheduler / Instance Handle / Play-Stop API
- [x] 18.3 GPU Particle Track Adapter
- [x] 18.4 Fluid2D / Volumetric Fluid Track Adapter
- [x] 18.5 Light Track Adapter
- [x] 18.6 PostEffect Track Adapter
- [x] 18.7 Camera Shake Track Adapter
- [x] 18.8 User Parameters / Track Bindings
- [x] 18.9 VFX Timeline Editor / Preview / Hot Reload
- [x] 18.10 Diagnostics / Budget / Stress Test / Closure

## 18.1 — Data Model / Schema / Compiler

`Engine/Vfx/Data/VfxCueTypes.h` owns explicit typed track payloads through `std::variant`.

Track types:

- `Particle`
- `Fluid2D`
- `VolumetricFluid`
- `Light`
- `PostEffect`
- `CameraShake`

Common track data contains only name, enabled state, timing, local offset, and parameter bindings. Subsystem implementation values remain in dedicated payloads.

Cue assets use `.vfx.json` with `schemaVersion = 1`. Unknown schema versions and unknown track strings fail load instead of silently becoming another effect type.

`VfxCueCompiler` validates the editable description and creates a `VfxCueProgram` sorted by start time. Equal-time tracks preserve their source order. Disabled tracks do not enter the program.

## 18.2 — Scheduler / Handle / Play-Stop

`Engine/Vfx/Runtime/VfxCueRuntime.h/.cpp` provides the gameplay facade.

```cpp
VfxCueRuntime* vfx = VfxCueRuntime::GetInstance();
vfx->LoadCue("Resources/Vfx/Phase18/Explosion.vfx.json");

VfxCueHandle handle = vfx->Play("Phase18Explosion", hitPosition);
vfx->SetWorldPosition(handle, movingPosition);
vfx->Stop(handle);
```

The scheduler owns:

- start-time ordered instruction consumption;
- track start/end events;
- one-shot and looping Cue lifetime;
- independent `VfxCueHandle` instances;
- moving world positions;
- explicit Stop / StopAll;
- a maximum 0.25 second frame advance clamp after debugger stalls;
- bounded loop catch-up;
- per-frame start budget delay instead of silently dropping due tracks.

Each active instance copies the compiled program at `Play()`. Hot reload therefore affects future plays without mutating an effect that is already halfway through its timeline.

### World lifetime

Fluid and Light tracks create transient Actors in the active ActorWorld. They are tagged `__VFX_RUNTIME` and destroyed when the track ends.

When SceneManager changes to another ActorWorld, `VfxCueRuntime` first tells Fluid/Light adapters to abandon entries owned by the old World using pointer-value comparison only. It then stops Particle/PostEffect/Camera tracks normally. This avoids dereferencing a World that SceneManager may already have destroyed.

## 18.3 — GPU Particle Track Adapter

Particle tracks reference the existing Phase13 asset instead of copying emitter data:

```text
VFX Particle Track
   -> effectAssetPath
   -> effectName
   -> GpuParticleEffectRuntime
```

The adapter loads the effect on demand when it is not yet registered.

- zero-duration/non-loop Particle tracks use Phase13 `Play()`;
- looping Particle tracks use `PlayLoop()` and retain the Phase13 handle;
- moving Cue instances update loop position through `SetLoopPosition()`;
- loop-scoped ParticleFloat parameters use Phase13 handle-scoped `SetFloatParameter()`.

Current compatibility boundary: Phase13 one-shot playback does not expose an instance-scoped parameter handle, so `ParticleFloat` bindings are intended for looping Particle tracks. The VFX layer does not mutate Phase13 effect-global parameter state just to fake one-shot instance parameters.

## 18.4 — Fluid2D / Volumetric Fluid Track Adapter

Fluid tracks reuse the existing `FluidEmitterComponent` contract.

At track start the adapter creates a transient Actor and configures:

- world position + local Cue offset;
- 2D or Volumetric target domain;
- radius;
- velocity;
- velocity strength;
- density rate;
- temperature rate;
- falloff exponent.

The Actor follows `SetWorldPosition()` while the track is alive and is destroyed on track stop.

A Volumetric track lazily enables `GpuVolumetricFluidManager`. It is not automatically disabled at track end because the smoke already injected into the 3D field needs to continue advecting/dissipating after the source itself stops.

## 18.5 — Light Track Adapter

Light tracks create a transient Point `LightComponent` in the current ActorWorld.

The existing ActorWorld -> LightManager synchronization remains the rendering source of truth. Phase18 does not create a second light buffer path.

Track payload controls:

- color;
- intensity;
- range;
- world/local position.

Intensity/Radius user bindings can scale the authored values per Cue instance.

## 18.6 — PostEffect Track Adapter

PostEffect tracks call the existing PostEffectManager by effect name.

Overlapping VFX tracks use a reference count per effect name:

```text
Explosion A Bloom ----+
Explosion B Bloom ----+--> one enabled PostEffect
                       |
last track ends -------+--> DisableEffect
```

The generic `IPostEffect` contract currently has no universal numeric weight interface. Therefore `weight * IntensityScale` currently acts as an activation gate rather than mutating effect-specific internals. Effect-specific numeric authoring can be added later through a typed PostEffect parameter interface without changing the Cue scheduler.

## 18.7 — Camera Shake Track Adapter

CameraShake tracks accumulate deterministic sine-wave translation, rotation, and optional FOV offsets.

The adapter follows a strict presentation-offset rule:

1. at frame begin, remove the offset applied last frame;
2. let normal gameplay/editor camera update run;
3. compute current active shake sum;
4. apply it once and rebuild the active Camera matrices.

This prevents shake values from accumulating permanently into Camera transforms.

Both Main Camera and Debug Camera are supported.

## 18.8 — User Parameters / Track Bindings

Cue assets may declare up to 64 float parameters.

```json
"userParameters": [
    {
        "name": "Intensity",
        "defaultValue": 1.0,
        "minValue": 0.25,
        "maxValue": 3.0
    }
]
```

Gameplay can drive a live instance without knowing track internals:

```cpp
VfxCueHandle explosion = vfx->Play("Phase18Explosion", position);
vfx->SetFloatParameter(explosion, "Intensity", damageScale);
```

Bindings support:

- `IntensityScale`
- `RadiusScale`
- `ParticleFloat`

Each binding applies `parameter * scale + bias`.

The compiler rejects duplicate/invalid user parameters, unknown parameter references, non-finite binding values, and invalid ParticleFloat target declarations.

`Resources/Vfx/Phase18/Explosion.vfx.json` uses one `Intensity` parameter to scale the volumetric smoke, flash light, bloom activation, and camera shake. Radius is also scaled on the smoke track.

## 18.9 — VFX Timeline Editor / Preview / Hot Reload

`Engine/Vfx/Editor/VfxTimelineEditor.h/.cpp` adds the `VFX Timeline` window.

The editor supports:

- `.vfx.json` path load;
- save + in-memory register;
- explicit hot reload;
- Cue name / loop / duration;
- preview world position;
- Preview / Stop Preview;
- user parameter editing;
- adding/removing all six track types;
- start time / duration / local offset editing;
- payload editing for Particle / Fluid / Light / PostEffect / CameraShake;
- simple horizontal timeline bars;
- timeline zoom;
- Intensity/Radius/ParticleFloat binding editing;
- unsaved in-memory preview through the same gameplay Runtime.

The window is wired into the normal GameApplication ImGui frame and shares an `EditorWindowState` visibility flag.

## 18.10 — Diagnostics / Budget / Stress Test

`VfxRuntimeStats` tracks:

- total Play / Stop requests;
- track starts / stops;
- completed instances;
- adapter failures;
- active/peak instance count;
- active/peak track count;
- active counts by subsystem;
- budget-rejected instance requests;
- delayed track starts;
- hot reloads;
- stress-play count;
- last operation status.

Default runtime budgets are:

```text
Active Cue instances     128
Track starts / frame      64
Active tracks            512
Transient VFX lights      32
Fluid tracks              64
Camera shakes             16
```

The Timeline window exposes these budgets for development tuning.

`RunStressBurst()` places multiple Cue instances on an X/Z lattice. The editor `Stress Burst Current Cue` button uses this path to exercise Particle + Fluid + Light + PostEffect + Camera orchestration together rather than benchmarking one subsystem in isolation.

## Application Integration

`GameApplication` owns ordering, not VFX subsystem implementation:

```text
Frame Begin
  -> VfxCueRuntime::BeginFrame()    // restore previous Camera shake
  -> normal Camera / Framework update
  -> SceneManager::Update()
  -> resolve current ActorWorld
  -> VfxCueRuntime::Update()
  -> PostEffect update
  -> Draw
```

Shutdown order is:

```text
VFX Timeline stop
 -> VfxCueRuntime::Finalize()
 -> SceneManager::Finalize()
 -> Reflection / Framework shutdown
```

This lets transient VFX Actors release while their World still exists.

## Sample Gameplay Usage

```cpp
VfxCueRuntime* vfx = VfxCueRuntime::GetInstance();

if (!vfx->IsCueRegistered("Phase18Explosion"))
{
    vfx->LoadCue("Resources/Vfx/Phase18/Explosion.vfx.json");
}

VfxCueHandle explosion = vfx->Play("Phase18Explosion", hitPosition);
vfx->SetFloatParameter(explosion, "Intensity", 1.5f);
```

The gameplay code does not reference FluidEmitterComponent, LightComponent, PostEffectManager, or Camera implementation details.

## Validation

Phase18 static regression coverage is split into:

- `Tests/Phase18/test_vfx_cue_foundation.py`
- `Tests/Phase18/test_vfx_orchestration_runtime.py`

The contracts cover:

- typed Cue schema;
- serializer strictness;
- compiled timeline ordering;
- scheduler/handle API;
- all adapters;
- transient World lifetime policy;
- Phase17 lazy volumetric enablement;
- user parameters/bindings;
- GameApplication update/finalize order;
- Timeline editor/preview;
- diagnostics/budgets/stress;
- sample Cue composition;
- MSBuild registration.

These Python contracts are repository-level static tests. They do not replace Visual Studio translation-unit compilation or D3D12 runtime validation.

## Known Boundaries

1. PostEffect `weight` is currently a generic activation gate because `IPostEffect` has no shared numeric weight interface.
2. ParticleFloat is safe for loop instances through Phase13 handle-scoped parameters; Phase13 one-shot does not yet expose equivalent per-instance parameter overrides.
3. VFX transient Actors are tagged `__VFX_RUNTIME`. A future editor polish pass may hide that tag from the World Outliner and exclude it explicitly from save workflows while preview is running.
4. Camera shake is presentation-space transform/FOV offset, not a physics camera spring.
5. Actual Windows / Visual Studio / GPU validation still needs to be run after pulling the branch.

## Completion Boundary

Phase18 is complete at repository-integration level when:

1. a `.vfx.json` Cue loads and compiles;
2. gameplay can Play/Stop it by Cue name/handle;
3. Particle, 2D/3D Fluid, Light, PostEffect, and CameraShake tracks schedule through adapters;
4. Cue parameters drive multiple track classes without exposing subsystem internals;
5. the Timeline editor previews the same Runtime used by gameplay;
6. Hot Reload updates future instances without mutating active ones;
7. Runtime budgets prevent unbounded instance/track/light/fluid/shake growth;
8. Stress Burst and diagnostics expose cross-system load;
9. application update/shutdown order preserves Camera and World lifetime contracts;
10. Phase18 regression contracts and real Windows build/runtime validation are performed before merge.

After the Windows validation pass, this VFX layer is suitable for use while building a complete game rather than adding another mandatory engine architecture phase.
