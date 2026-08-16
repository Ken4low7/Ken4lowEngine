# Phase29 — Production VFX Library

Phase29 turns the Phase20-28 VFX foundation into a reusable production catalog. It does not add another particle simulator, another graph compiler, or another budget owner. Every shipped preset is loaded through the existing `VfxGraphRuntime` and therefore keeps the same GPU particle, Fluid, Light, PostEffect, LOD, culling, budget, and diagnostics behavior already implemented by earlier phases.

## Production catalog

`Engine/Vfx/Library/VfxProductionLibrary.h` is the code-side catalog. Each entry contains:

- stable library ID
- compiled graph name
- asset path
- category
- cost class
- loop/one-shot policy
- search tags

The catalog currently ships 10 presets across four categories.

### Combat

| Library ID | Graph | Cost | Usage |
| --- | --- | --- | --- |
| `combat.impact.spark` | `ProdCombatImpactSpark` | Low | bullet/melee impact spark with collision sub-emission |
| `combat.muzzle.flash` | `ProdCombatMuzzleFlash` | Low | short weapon flash with transient light/bloom |
| `combat.explosion` | `ProdCombatExplosion` | High | integrated particle + volumetric fluid + light + bloom explosion |
| `combat.debris` | `ProdCombatDebris` | Medium | mesh debris burst |

### Elemental

| Library ID | Graph | Cost | Usage |
| --- | --- | --- | --- |
| `elemental.fire.burst` | `ProdElementalFireBurst` | Medium | fire burst with heat/fluid and light output |
| `elemental.frost.burst` | `ProdElementalFrostBurst` | Low | lightweight frost/ice burst |

### Magic

| Library ID | Graph | Cost | Usage |
| --- | --- | --- | --- |
| `magic.arcane.ribbon` | `ProdMagicArcaneRibbon` | Medium | continuous arcane ribbon loop |
| `magic.energy.trail` | `ProdMagicEnergyTrail` | Medium | continuous projectile/weapon energy trail |

### Environment

| Library ID | Graph | Cost | Usage |
| --- | --- | --- | --- |
| `environment.smoke.plume` | `ProdEnvironmentSmokePlume` | High | persistent smoke with volumetric source |
| `environment.ember.field` | `ProdEnvironmentEmberField` | Medium | ambient looping ember field |

## Loading policy

The production library deliberately does not auto-load every graph at engine startup. Gameplay can load only what a level, weapon set, biome, or encounter needs.

```cpp
using namespace Ken4lowEngine;

VfxProductionLibrary::Load("combat.impact.spark");
VfxProductionLibrary::LoadCategory(VfxProductionCategory::Combat);
VfxProductionLibraryLoadResult result = VfxProductionLibrary::LoadAll();
```

After loading, gameplay uses the normal graph runtime API and the catalog's `graphName`.

```cpp
const VfxProductionLibraryEntry* entry = VfxProductionLibrary::Find("combat.explosion");
if (entry != nullptr)
{
    VfxGraphRuntime::GetInstance()->Play(entry->graphName, worldPosition);
}
```

Loop entries should use `PlayLoop`, retain their handle, update position when attached to moving gameplay objects, and stop the handle when the owning effect ends.

## Naming convention

Production IDs use lowercase dot-separated names:

`<category>.<effect>[.<variant>]`

Examples:

- `combat.impact.spark`
- `magic.energy.trail`
- `environment.smoke.plume`

Production graph names use the `Prod` prefix and do not contain phase numbers. Phase folders remain regression/demo assets; `Resources/VfxGraph/Production` is the reusable game-facing set.

## Cost classes

Cost class is human-facing metadata that matches the authored Phase27 `budgetCost` range:

- **Low**: budget cost 1-2
- **Medium**: budget cost 3-4
- **High**: budget cost 5+

Actual admission still belongs to the existing `VfxRuntimeBudget` and `VfxGraphRuntime`. The Phase29 class does not bypass or duplicate runtime budget checks.

## Scalability rules

Every production graph explicitly authors:

- frustum culling
- max draw distance
- near/far LOD distances
- mid/far runtime scales
- budget cost

Persistent effects use `SpawnRate` and are marked as loops in both the catalog and graph emitter. One-shots use `Burst`. This makes production intent machine-checkable and prevents accidentally shipping an infinite emitter as a one-shot preset.

## Renderer and subsystem coverage

The initial library intentionally exercises the production paths created in previous phases:

- Collision + SubEmitter: impact sparks
- MeshRenderer: combat debris
- RibbonRenderer: arcane ribbon
- TrailRenderer: energy trail
- FluidOutput: explosion, fire, smoke
- LightOutput: muzzle flash, explosion, fire
- PostEffectOutput: muzzle flash, explosion
- Curve/Gradient modules: burst shaping and color/size evolution

All assets continue through the Phase20 compiler and existing GPU particle backend. Phase22 event processing remains GPU-local, and Phase28 diagnostics remain non-blocking.

## Asset dependencies

The first production set intentionally uses dependencies already exercised by regression content:

- `Effects/white.dds`
- `Sample/cube.gltf`

This keeps Phase29 focused on library composition and production conventions rather than introducing a new texture/mesh import problem at the same time.

## Authoring checklist for new production effects

When adding another production effect:

1. Place it below `Resources/VfxGraph/Production/<Category>/`.
2. Give it a stable `Prod...` graph name.
3. Add exactly one catalog entry with a unique ID, graph name, and path.
4. Set explicit scalability and budget metadata.
5. Mark persistent effects as loops and use `SpawnRate`; use `Burst` for one-shots.
6. Reuse existing runtime nodes/backends rather than adding effect-specific execution code.
7. Keep renderer dependencies intentional and reviewable.
8. Run Phase29 contracts plus TeamDevelopmentCI Debug/Release builds.
9. Use Phase28 diagnostics/stress tooling to inspect frame cost, culling, LOD, budget pressure, active particles, and draw counts in the target scene.

## CI contracts

Phase29 static tests verify:

- exactly 10 unique catalog entries
- four production categories
- unique stable IDs, graph names, and paths
- catalog path/graph-name agreement with JSON assets
- schema version and graph caps
- explicit scalability on every production asset
- cost-class/budget-cost agreement
- loop metadata and SpawnRate/Burst policy
- feature coverage across Collision/SubEmitter/Ribbon/Trail/Mesh/Fluid/Light/PostEffect/Curve/Gradient paths
- stable renderer dependencies
- no uncatalogued `.vfxgraph.json` files in the Production folder
- catalog loading stays on `VfxGraphRuntime`

The dedicated Phase29 workflow also reruns Phase13 and Phase20-28 VFX regression contracts.

## Roadmap completion

Phase29 is the final phase of the Phase20-29 VFX roadmap. Further VFX work should now be driven by game-specific needs, measured production bottlenecks, art-direction requirements, or reusable additions to this catalog rather than by creating another parallel VFX architecture.
