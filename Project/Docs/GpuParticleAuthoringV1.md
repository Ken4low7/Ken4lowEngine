# GPU Particle Authoring V1

## Goal

GPU Particle Authoring V1 makes effects data-driven instead of adding a new hard-coded `GpuParticleType` for every visual variation.

Gameplay addresses an Effect by name. An Effect contains one or more Emitters, and each Emitter is compiled into four explicit responsibilities:

- Emission Module
- Spawn Module
- Update Module
- Render Module

The editor preview and gameplay use the same `GpuParticleEffectRuntime` path.

## Supported Authoring Features

### Effect

- Multiple emitters per effect
- JSON load/save
- Effect-level float User Parameters
- Per-emitter User Parameter bindings
- Runtime hot registration/reload
- One-shot playback
- Loop-instance playback with handles
- Per-instance loop position updates
- Per-effect and per-instance User Parameter overrides

### Emission

- `maxParticles`
- startup `burstCount`
- `spawnRate`
- finite `duration`
- infinite looping when `loop=true` and played through `PlayLoop`
- bounded eight-slot burst-emitter pool for repeated one-shot effects

`Play()` always behaves as a bounded one-shot effect. It may continue emitting for `duration`, but it never turns `loop=true` into an infinite gameplay loop.

`PlayLoop()` gives the caller an explicit lifetime handle. For `loop=true` emitters it emits indefinitely until `StopLoop`. For `loop=false` emitters, periodic emission stops after `duration`, while already alive particles finish normally.

## Spawn Shapes

The authored custom spawn shader executes all exposed shapes:

- Point
- Sphere
- Box
- Cone
- Circle
- Ring
- Hemisphere

`spawnRadius` is used by radial shapes. `spawnBoxSize` is used by Box and its Y component is used as Cone height.

## Update Modules

The custom GPU update path supports:

- gravity
- damping
- velocity and velocity randomization
- size randomization
- 4-key size-over-life LUT
- start/end color
- 4-key color-over-life gradient LUT
- alpha fade
- sprite Z rotation
- mesh XYZ rotation
- mesh angular velocity
- deterministic procedural noise force
- vortex force
- attractor force with optional radius falloff

The 4-key LUT convention is:

```text
Key 0 = lifetime 0.0
Key 1 = lifetime 1/3
Key 2 = lifetime 2/3
Key 3 = lifetime 1.0
```

The GPU linearly interpolates between adjacent keys.

## Render Module

### Renderer types

- Sprite
- Mesh

Mesh paths are relative to:

```text
Resources/Models/Sources/
```

For example:

```text
Sample/cube.gltf
```

`meshSubMeshIndex` selects the submesh loaded from Assimp.

### Blend modes

Authoring blend modes are real PSO choices:

- Alpha -> Normal alpha blend PSO
- Additive -> Add blend PSO
- Multiply -> Multiply blend PSO

Sprite and Mesh pipelines both own the required PSOs.

### Material isolation

Authored emitters may all use the legacy custom-spawn code path internally, so `GpuParticleType::Default` cannot be used as the final material identity.

The runtime therefore builds a stable render-group hash from:

```text
texture-or-mesh key + material draw type + blend mode
```

That render group is stamped into authored particles at spawn time. Sprite and Mesh pixel shaders reject particles belonging to another render group. Legacy emitters retain their original type-based filtering.

This prevents one authored texture from accidentally redrawing particles created for another texture or blend mode.

## Sprite Sheet

Sprite emitters support:

- rows
- columns
- frame rate
- looping playback

The frame is resolved in the sprite pixel shader from particle lifetime time.

## User Parameters

An Effect may expose float parameters such as:

```text
Charge
Intensity
DamageScale
WeatherStrength
```

Each emitter binding selects one target:

- SpawnRate
- BurstCount
- LifeTime
- Speed
- Size
- Alpha
- Force

A binding converts the parameter into a multiplier:

```text
factor = max(0, bias + scale * parameterValue)
```

Multiple bindings targeting the same property multiply together.

Gameplay example:

```cpp
#include <GpuParticleEffectRuntime.h>

using Ken4lowEngine::GpuParticleEffectRuntime;

GpuParticleEffectRuntime* effects = GpuParticleEffectRuntime::GetInstance();
effects->LoadEffect("Resources/Effects/Phase13/BossCharge.effect.json");

auto handle = effects->PlayLoop("BossCharge", bossPosition);

// ChargeAmount is authored as a 0..1 User Parameter.
effects->SetFloatParameter(handle, "Charge", chargeAmount);
effects->SetLoopPosition(handle, bossPosition);

if (chargeFinished)
{
    effects->StopLoop(handle);
}
```

Effect-wide values may be set with:

```cpp
effects->SetFloatParameter("BossCharge", "Charge", chargeAmount);
```

Handle-level values only affect that loop instance.

## Samples

### `Explosion.effect.json`

Three Sprite emitters:

- Flash
- Smoke
- Sparks

It exercises:

- Alpha and Additive blending
- Hemisphere and Ring spawn shapes
- size curves
- color gradients
- noise
- vortex
- attractor
- `Intensity` User Parameter bindings

### `MeshDebris.effect.json`

A real Mesh emitter using:

```text
Sample/cube.gltf
```

It exercises:

- Assimp mesh loading
- finite emission duration
- random XYZ start rotation
- XYZ angular velocity
- Alpha blend
- color gradient
- `Intensity` User Parameter

### `BossCharge.effect.json`

A long-lived multi-emitter loop effect with a `Charge` parameter.

It exercises:

- runtime handle-scoped parameter updates
- spawn-rate scaling
- force scaling
- size and alpha scaling
- inward attractor motion
- vortex motion
- additive core/inward particles plus alpha smoke

## GPU/CPU Layout Contracts

`GpuEmitterCBData` and HLSL `EmitterCBData` intentionally mirror each other.

Current C++ contract:

```text
GpuEmitterCBData = 480 bytes
```

The global particle structured-buffer stride is currently:

```text
Particle = 384 bytes
```

With `131072` global particle slots, the main particle state buffer is approximately 48 MiB before auxiliary free-list resources.

The larger stride is deliberate in V1: immutable curve/force state is copied into each particle so an emitter slot can be reused or hot-reloaded without changing particles that are already alive.

## Rendering Performance Safety

The current global buffer still submits the global particle slot count for each active render group. To avoid pushing dead or foreign material particles through rasterization, V1 performs an early vertex-stage rejection before billboard/mesh transforms and retains pixel-stage render-group validation as a safety net.

This is correct and materially cheaper than pixel-only filtering, especially for Mesh particles, but it is not the final possible GPU optimization.

The next performance architecture, if profiling shows the need, is:

```text
Particle Update
    -> GPU alive/material compaction
    -> visible particle index list
    -> indirect draw arguments
    -> DrawIndexedInstancedIndirect / ExecuteIndirect
```

That removes the remaining `kMaxParticles` vertex-invocation scan from each material group. It should be implemented as a profiling-driven rendering phase rather than mixed into the authoring contract, because it changes GPU resource ownership, barriers, indirect-argument state, and root-signature bindings while leaving Effect assets unchanged.

## CI Contracts

Phase 13 tests protect:

- module compilation boundaries
- serialization compatibility
- all authored spawn shapes
- real blend PSO selection
- Mesh asset loading path
- Mesh rotation
- size/color LUT GPU execution
- noise/vortex/attractor execution
- User Parameter bindings
- finite emission duration
- render-group isolation
- CPU/GPU stride contracts
- Editor preview through gameplay runtime
- sample Effect coverage

Windows CI also compiles the GPU Particle HLSL files with DXC so shader syntax and include errors fail before runtime shader creation.
