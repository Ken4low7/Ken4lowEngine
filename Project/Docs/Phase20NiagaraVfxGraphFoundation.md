# Phase 20 — Niagara-like VFX Graph Foundation

## Status

**Phase 20 status: repository integration complete through 20.10.**

Phase20 introduces a data-driven Niagara-like graph layer above the existing Phase13 GPU Particle authoring/runtime. It does **not** replace the working particle allocator, compute update, renderer, GPU-driven compaction, or Effect Runtime. The graph compiler translates authored nodes into the existing `GpuParticleEffectDesc` contract so the backend remains one source of truth.

This is intentionally the foundation for the later VFX phases:

- Phase21: particle modules, curves, gradients
- Phase22: collision, events, sub emitters
- Phase23: ribbon / trail / mesh particles
- Phase24: GPU execution graph / optimization
- Phase25: visual graph editor / preview
- Phase26: fluid / light / post effect graph integration
- Phase27: bounds / LOD / budget / culling
- Phase28: profiling / debug / stress
- Phase29: production VFX library

## Roadmap

- [x] 20.1 Versioned VFX Graph asset schema
- [x] 20.2 Typed node payload model
- [x] 20.3 Spawn / Initialize / Update / Render stage model
- [x] 20.4 Deterministic DAG / edge compiler
- [x] 20.5 Stage / cycle / duplicate node validation
- [x] 20.6 Graph -> Phase13 `GpuParticleEffectDesc` backend bridge
- [x] 20.7 User Parameter bridge
- [x] 20.8 Play / Loop / Reload runtime facade
- [x] 20.9 Sample Energy Burst graph / diagnostics
- [x] 20.10 Static contracts / module registration / Windows CI

## Architecture

```text
.vfxgraph.json
      |
      v
VfxGraphSerializer
      |
      v
VfxGraphDesc
  |- Emitters
  |- Typed Nodes
  |- Execution Edges
  `- User Parameters
      |
      v
VfxGraphCompiler
  |- schema validation
  |- stage validation
  |- payload validation
  |- cycle detection
  |- deterministic topological sort
  `- backend lowering
      |
      v
VfxGraphProgram
      |
      v
GpuParticleEffectDesc
      |
      v
Phase13 GpuParticleEffectRuntime
      |
      v
GPU Particle Manager / Compute / Renderer
```

The graph layer owns **authoring intent and compilation only**. It does not create a second GPU particle simulation.

## 20.1 — Asset Schema

Graph files use `.vfxgraph.json` and currently require `schemaVersion = 1`.

Top level fields:

```json
{
  "schemaVersion": 1,
  "graphName": "Phase20EnergyBurst",
  "userParameters": [],
  "emitters": []
}
```

Each emitter stores:

- name
- max particle budget
- loop / duration
- User Parameter bindings
- typed nodes
- execution edges

Node IDs are stable asset IDs and are independent from future Editor screen position. `editorPosition` is stored separately so Phase25 can add a visual graph UI without changing runtime identity.

## 20.2 — Typed Payloads

The graph does not store a giant `params` bag inside runtime C++. `VfxGraphNodePayload` is a `std::variant` containing one payload struct per node type.

Foundation nodes:

```text
Spawn
  SpawnRate
  Burst

Initialize
  SpawnPoint
  SpawnSphere
  SpawnBox
  Lifetime
  InitialVelocity
  InitialColor
  InitialSize

Update
  Gravity
  Drag

Render
  SpriteRenderer
```

Unknown node types fail deserialization instead of silently becoming another node.

## 20.3 — Execution Stages

Every node type owns exactly one legal stage.

```text
Spawn
  -> Initialize
      -> Update
          -> Render
```

An edge may stay inside one stage or move forward, but may not point backward. A `Gravity -> Lifetime` edge is therefore rejected even if the graph would otherwise be acyclic.

Stage order is an execution contract rather than a UI convention. Later phases can add more nodes without changing the fundamental simulation order.

## 20.4 — Deterministic DAG Compiler

`VfxGraphCompiler` validates all node IDs and edge endpoints, then performs deterministic topological sorting.

When multiple nodes are ready at the same time they are ordered by:

1. Stage
2. Node ID

This keeps compiled output stable regardless of `unordered_map` bucket order or JSON insertion behavior.

Disconnected nodes are valid during this foundation phase. They are still ordered by stage/ID and compiled. Phase25 can later add Editor warnings for visually disconnected islands.

## 20.5 — Validation

Compilation rejects:

- unsupported schema version
- empty graph / emitter names
- zero particle budget
- non-finite values
- duplicate node IDs
- duplicate edges
- self edges
- missing edge endpoints
- backward-stage edges
- cycles
- node payload/type mismatch
- node placed in the wrong stage
- multiple enabled nodes of the same foundation module type
- multiple enabled spawn shapes
- anything other than exactly one enabled Sprite Renderer
- unknown User Parameter bindings

If no SpawnRate and no Burst actually emit particles, compilation succeeds with a warning because authoring an intentionally dormant graph is still useful for future parameter-driven activation.

## 20.6 — Phase13 Backend Bridge

The compiler lowers each graph emitter to one existing `GpuParticleEmitterDesc`.

Examples:

```text
Burst node
  -> GpuParticleEmitterDesc::burstCount

SpawnSphere
  -> spawnShape = Sphere
  -> spawnRadius

Lifetime
  -> lifeTime / lifeTimeRandom

InitialVelocity
  -> velocity / velocityRandom / speed

Gravity
  -> gravity

Drag
  -> damping

SpriteRenderer
  -> texturePath / blendMode / billboard
```

The result is registered with `GpuParticleEffectRuntime`. Existing GPU-driven rendering, particle buffers, descriptor ownership, sprite PSOs, and emission scheduling remain unchanged.

## 20.7 — User Parameters

Phase20 graph assets reuse the proven Phase13 float parameter contract.

A graph may expose:

```json
{
  "name": "Intensity",
  "defaultValue": 1.0,
  "minValue": 0.25,
  "maxValue": 3.0
}
```

Emitter bindings can drive the existing targets:

- SpawnRate
- BurstCount
- LifeTime
- Speed
- Size
- Alpha
- Force

`VfxGraphRuntime::SetFloatParameter` forwards these values to Phase13 rather than introducing a duplicate parameter store.

## 20.8 — Runtime API

Typical one-shot usage:

```cpp
VfxGraphRuntime* graphs = VfxGraphRuntime::GetInstance();
graphs->LoadGraph("Resources/VfxGraph/Phase20/EnergyBurst.vfxgraph.json");
graphs->Play("Phase20EnergyBurst", hitPosition);
```

Loop usage:

```cpp
auto handle = graphs->PlayLoop("AuraGraph", actorPosition);
graphs->SetLoopPosition(handle, actorPosition);
graphs->SetFloatParameter(handle, "Intensity", 2.0f);
graphs->StopLoop(handle);
```

Reload recompiles the Graph and re-registers the generated Phase13 Effect. The runtime stores the compiled `VfxGraphProgram` primarily for diagnostics and the later visual graph editor.

## 20.9 — Sample Graph

`Resources/VfxGraph/Phase20/EnergyBurst.vfxgraph.json` demonstrates:

```text
Burst
 -> Sphere Spawn
 -> Lifetime
 -> Velocity
 -> Cyan Color
 -> Size
 -> Gravity
 -> Drag
 -> Sprite Renderer
```

It exposes an `Intensity` parameter bound to BurstCount and Size.

The sample is intentionally a small cyan energy burst rather than the final slash effect. The slash reference requires Curve/Gradient and especially Ribbon/Trail, which belong to Phases21 and 23. Keeping Phase20 small prevents the foundation from hard-coding one production effect.

## 20.10 — Diagnostics and CI

`VfxGraphRuntimeStats` records:

- registered Graph count
- compile failures
- play requests / successes
- loop starts / stops
- reload count

Static contracts under `Tests/Phase20` validate the schema, typed payloads, stage ownership, deterministic DAG rules, backend bridge, runtime API, sample asset, module registration, and roadmap.

A dedicated `Phase20VfxGraphCI` workflow runs:

- Engine module ownership validation
- project asset validation
- broken reference validation
- Phase20 static contracts

The existing TeamDevelopmentCI continues to run the repository-wide automated tests, GPU Particle HLSL validation, and Windows Debug/Release translation-unit compilation.

## Validation Boundary

Passing repository CI proves source/module/asset contracts and Windows translation-unit compilation. It does not replace final runtime verification on a D3D12 development machine.

After pulling the branch, the minimum runtime smoke test is:

```cpp
VfxGraphRuntime::GetInstance()->LoadGraph(
    "Resources/VfxGraph/Phase20/EnergyBurst.vfxgraph.json");

VfxGraphRuntime::GetInstance()->Play(
    "Phase20EnergyBurst",
    Vector3{ 0.0f, 1.0f, 0.0f });
```

The full visual node editor intentionally starts in Phase25; Phase20 establishes the asset/runtime/compiler contract it will edit.
