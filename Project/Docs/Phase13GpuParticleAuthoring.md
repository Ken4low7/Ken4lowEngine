# Phase 13 — GPU Particle Effect Authoring

## Goal

Phase 13 changes GPU Particle usage from "gameplay code chooses particle implementation details" to "gameplay code plays an authored Effect Asset".

The existing GPU backend remains the source of truth for allocation, compute emission/update, Sprite/Mesh pipelines, and particle rendering. Phase 13 adds an authoring/runtime layer above it instead of replacing the working backend.

```text
GpuParticleEffect JSON
        |
        v
GpuParticleEffectSerializer
        |
        v
GpuParticleEffectCompiler
        |
        +--> Emission Module
        +--> Spawn Module
        +--> Update Module
        +--> Render Module
        |
        v
GpuParticleEffectRuntime
        |
        v
GpuParticleEmitter / GpuParticleManager
        |
        v
GPU Compute + Draw
```

## 13.1 Module Boundary

`Runtime/GpuParticleEffectModules.h` introduces a compiled intermediate representation with four explicit responsibilities.

### Emission Module

Owns:

- maximum particle count
- loop flag
- duration metadata
- spawn rate
- burst count

### Spawn Module

Owns:

- spawn shape
- spawn position random range
- Sphere radius / Box size
- lifetime and lifetime random range
- initial velocity
- speed and speed random range

### Update Module

Owns:

- gravity
- damping
- start/end size
- start/end color
- alpha fade
- rotation
- 3D scale
- angular velocity metadata

### Render Module

Owns:

- Sprite / Mesh selection
- texture / mesh path
- billboard policy
- blend mode metadata
- sprite-sheet settings

`GpuParticleEffectCompiler` converts the existing flat `GpuParticleEmitterDesc` into these modules. This is intentionally a compatibility layer: old Effect JSON remains readable, while future runtime behavior no longer needs to couple directly to every flat serialized field.

## 13.2 Runtime Effect Playback

`Runtime/GpuParticleEffectRuntime.h` provides an Effect-level gameplay API.

A typical one-shot effect is:

```cpp
GpuParticleEffectRuntime* effects = GpuParticleEffectRuntime::GetInstance();
effects->LoadEffect("Resources/Effects/Phase13/Explosion.effect.json");

// Gameplay only selects an authored effect and world position.
effects->Play("Phase13Explosion", hitPosition);
```

A following/continuous effect is:

```cpp
auto handle = effects->PlayLoop("BossCharge", bossPosition);

effects->SetLoopPosition(handle, bossPosition);

// Stop by handle so multiple instances of the same effect can coexist.
effects->StopLoop(handle);
```

The runtime supports:

- `RegisterEffect`
- `LoadEffect`
- `ReloadEffect`
- one-shot `Play`
- `PlayLoop`
- `SetLoopPosition`
- stop by handle
- stop all loop instances with the same effect name
- status diagnostics

## 13.3 Multi-emitter Effects

One Effect Asset can contain multiple emitters. The sample `Resources/Effects/Phase13/Explosion.effect.json` contains:

1. `Flash`
2. `Smoke`
3. `Sparks`

Gameplay therefore issues one `Play("Phase13Explosion", position)` request while each authored layer keeps its own lifetime, velocity, color, size, gravity, and particle count.

This is the foundation required for effects such as:

```text
BossCharge
  |- CoreGlow
  |- InwardParticles
  |- Smoke
  `- Sparks
```

without adding a new hard-coded `GpuParticleType` for every visual variation.

## 13.4 Repeated Burst Safety

A single reusable emitter per Effect is not sufficient for hit/explosion effects. Two calls in one frame at different positions could otherwise update the same emitter position before the GPU consumes both requests.

Phase 13 uses a small bounded burst-emitter pool per Effect. Each `Play` advances through eight slots. Particle state is still stored by the existing GPU backend, while subsequent spawns can safely use another emitter slot and position.

The pool is bounded intentionally so repeated combat effects do not create an unbounded number of runtime emitter objects.

## 13.5 Loop Instance Handles

Looping effects are instance-scoped rather than effect-name-scoped.

Two enemies can therefore run the same authored effect simultaneously:

```cpp
auto enemyAEffect = effects->PlayLoop("PoisonAura", enemyAPosition);
auto enemyBEffect = effects->PlayLoop("PoisonAura", enemyBPosition);

effects->SetLoopPosition(enemyAEffect, enemyAPosition);
effects->SetLoopPosition(enemyBEffect, enemyBPosition);
```

Each loop owns uniquely named runtime emitters and can be stopped independently.

## 13.6 Current Backend Compatibility Gate

The serialized schema already knows more features than the current flat custom-spawn GPU path actually executes. Phase 13 therefore fails closed instead of pretending those fields are working.

The first runtime foundation currently accepts:

- Sprite renderer
- Alpha blend
- Point spawn
- Sphere spawn
- Box spawn

The following remain asset-schema values but are not enabled by `GpuParticleEffectRuntime` yet:

- Mesh authored runtime path
- Additive / Multiply authored blend modes
- Cone / Circle / Ring / Hemisphere custom spawn shapes

This separation lets the schema remain stable while GPU backend support is added incrementally.

## 13.7 Runtime Preview

`GpuParticleEffectEditor` now uses the same `GpuParticleEffectRuntime` as gameplay instead of maintaining an editor-only particle simulation.

The editor exposes:

- `Preview Position`
- `Preview Burst`
- `Start Loop Preview`
- `Stop Loop Preview`

Preview registers the current in-memory `GpuParticleEffectDesc` first, so unsaved authoring changes can be tested immediately. A loop preview keeps its `PlayHandle` and updates its world position while the preview position is edited.

The property UI is also grouped with the same four authoring responsibilities used by the compiler:

- Emission Module
- Spawn Module
- Update Module
- Render Module

This keeps the editor vocabulary, runtime model, and serialized Effect Asset aligned.

## 13.8 Existing Update Behavior

The current GPU update path still uses the existing linear behavior for authored override emitters:

- start size -> end size
- start color -> end color
- gravity
- damping
- rotation
- alpha fade

Phase 13 does not duplicate or replace that code. The Module layer records the authoring intent and feeds the proven `GpuParticleEmitter::EmitterInfo` path.

## 13.9 Next Authoring Steps

The recommended continuation is:

1. implement Cone / Circle / Ring / Hemisphere in the custom Emit compute path;
2. route authored Additive / Multiply through render pipeline selection;
3. add `FloatCurve` and `ColorGradient` authoring data;
4. bake curves/gradients to GPU-friendly LUTs;
5. add Update modules such as Noise, Vortex, and Attractor;
6. add Effect User Parameters so gameplay can drive values such as `ChargeAmount` without knowing emitter internals;
7. add GPU events, trails/ribbons, depth collision, and effect profiling only after the core authoring workflow is proven in a game.

## 13.10 Curve / Gradient Direction

Curves should not perform arbitrary key searches for every particle every frame. The intended path is:

```text
Editor FloatCurve / ColorGradient
             |
             v
        Bake to LUT
             |
             v
GPU Particle Update samples by normalized lifetime
```

For example, a 256-sample LUT can represent `SizeOverLife`, `AlphaOverLife`, or a color gradient while keeping the update shader predictable.

## 13.11 User Parameter Direction

The target gameplay API is intentionally small:

```cpp
auto charge = effects->PlayLoop("BossCharge", bossPosition);

// Future User Parameter API: gameplay publishes meaning, not particle implementation details.
charge.SetFloat("ChargeAmount", chargeRate);
```

The Effect Asset can later map `ChargeAmount` to spawn rate, size, color, noise strength, or other modules without changing boss gameplay code.

## Validation

`Tests/Phase13/test_gpu_particle_effect_authoring.py` protects:

- the four Module boundaries;
- flat-desc -> Module compilation;
- serializer compatibility;
- Effect-level playback API;
- bounded burst emitter pooling;
- handle-scoped loop instances;
- fail-closed backend compatibility checks;
- real-time editor preview integration;
- the multi-emitter Phase 13 sample asset.

Windows Debug/Release translation-unit compilation remains part of TeamDevelopmentCI. `GpuParticleEffectEditor.cpp` includes the runtime facade directly, so the Windows compile job parses the authoring runtime against the real engine include environment.

## Completion Boundary

Phase 13 foundation is ready for game-side evaluation when:

1. an Effect JSON can be loaded and played by effect name;
2. one Effect can author multiple independent emitters;
3. repeated burst effects can occur at different positions without sharing one mutable spawn location;
4. multiple loop instances of the same effect can coexist and follow different Actors;
5. the Effect Editor can preview current in-memory settings through the gameplay runtime;
6. unsupported backend combinations fail closed;
7. Phase 13 tests and Debug/Release compile jobs pass.

Curve/Gradient LUTs and User Parameters are intentionally the next authoring increment rather than prerequisites for this compatibility foundation.
