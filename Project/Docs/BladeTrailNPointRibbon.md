# N-point Blade Trail

## Purpose

`BladeTrailComponent` is the weapon-swing trail path for swords, axes, scythes, and similar melee weapons. It is intentionally separate from the Phase23 GPU Particle `TrailRenderer`.

The Phase23 trail stores one `previousTranslate` sample per particle and is ideal for projectile / magic / spark streaks. A sword swing instead needs the blade surface swept across several frames:

```text
Tip0  -- Tip1  -- Tip2  -- Tip3
 |        |        |        |
Root0 -- Root1 -- Root2 -- Root3
```

`BladeTrailComponent` records Root/Tip pairs and produces this ribbon directly.

## Runtime flow

```text
Sword Actor / skeletal sockets
        |
        v
BladeTrailComponent
  - Root / Tip history
  - lifetime pruning
  - min-distance sampling
  - Catmull-Rom smoothing
        |
        v
CPU triangle strip expansion
  6 vertices per segment
        |
        v
BladeTrailRenderer
  FrameUploadArena
  one DrawInstanced call
  texture + vertex color
```

The renderer shares one root signature / PSO set through reference-counted `Acquire` / `Release`. Components only upload transient vertices for the current frame.

## Sampling

Gameplay sampling occurs in `PostPhysicsUpdate()` so the trail sees the final Actor transform for the frame. `Update()` only ages history. Editor sampling uses `UpdateEditor()`.

Two endpoint modes are available:

1. Local offsets relative to the owner Root `SceneComponent`.
2. Explicit world endpoints using `SetBladeWorldEndpoints(root, tip)` for skeletal sockets or externally animated weapons.

The default local blade points are:

```text
Root = (0, 0, 0)
Tip  = (0, 0, 1.5)
```

Change these to match the weapon's local forward axis.

## Attack integration

Start the trail only during the useful swing window:

```cpp
bladeTrail->BeginTrail();
```

Feed skeletal endpoints each frame when needed:

```cpp
bladeTrail->SetBladeWorldEndpoints(rootWorld, tipWorld);
```

Stop recording when the swing active window ends:

```cpp
bladeTrail->EndTrail();
```

Existing points are not cleared by `EndTrail`; they fade naturally according to `HistoryLifetime`.

## Authoring defaults

Recommended first blue sword slash:

```text
History Lifetime      0.20 - 0.28 s
Max Samples           24 - 40
Smoothing Subdivisions 2 - 3
Min Sample Distance   0.01 - 0.03
Width Scale           1.0
Blend Mode            Additive
Head Color            (255, 255, 255, 255)
Tail Color            (20, 90, 255, 0)
```

The Inspector uses ImGui's `Uint8` color presentation, so artists see RGBA values in the requested 0-255 convention even though the stored `Vector4` remains normalized 0-1 for rendering.

## Preview

`Preview Arc` synthesizes a curved local swing in the editor. This makes it possible to validate Root/Tip orientation, lifetime, width, texture, color, and smoothing before gameplay animation is connected.

## Smoothing and cost

`SmoothingSubdivisions` is clamped to 1-4. `MaxSamples` is clamped to 2-128. The default 32 samples and subdivision 2 are intentionally modest.

For `N` smoothed points the renderer emits:

```text
(N - 1) * 6 vertices
```

and performs one draw call per visible Blade Trail component.

## Rendering contract

- triangle list
- cull mode: none
- depth test: enabled
- depth write: disabled
- default blend: additive
- default texture: `Effects/white.dds`
- scene render target: existing `PostEffectManager` scene target
- transient vertex memory: existing `FrameUploadArena`

## Relationship to VFX Graph

This feature does not create another particle backend and does not replace `RibbonRenderer` / `TrailRenderer`.

Use:

- GPU Particle Trail for projectiles, sparks, magic bullets, and short moving particles.
- Blade Trail for a weapon surface swept through multiple frames.
- VFX Graph particles / Light / PostEffect alongside Blade Trail for sparks, impact flashes, elemental glows, and slash-hit effects.
