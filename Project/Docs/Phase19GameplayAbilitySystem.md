# Phase 19 — Gameplay Ability / Event Framework

## Status

**Phase 19 status: repository integration complete through 19.10.**

Phase 19 turns the rendering/VFX foundation from Phases 13–18 into gameplay-facing reusable systems. It adds hierarchical Gameplay Tags, an ActorHandle-based Event Router, generic Attributes and Modifiers, data-driven Ability assets, per-Actor Ability runtime, Phase18 VFX bridging, Editor/Inspector controls, diagnostics, budgets, and stress contracts.

`CharacterHealthComponent` remains the owner of character HP, damage acceptance, invulnerability, and death. Phase19 deliberately does not replace that established responsibility. The new Attribute system is intended for values such as Energy, Stamina, MoveSpeedScale, Charge, Heat, or game-specific resources.

## Roadmap

- [x] 19.1 Gameplay Tag / Event data model
- [x] 19.2 Deterministic Gameplay Event Router
- [x] 19.3 Generic Gameplay Attributes
- [x] 19.4 Timed / stacked Attribute Modifiers
- [x] 19.5 Ability Asset / Serializer / Compiler
- [x] 19.6 Per-Actor Ability Runtime / Cooldown / Cost
- [x] 19.7 Target Context / Required & Blocked Tags
- [x] 19.8 Phase18 Unified VFX Bridge
- [x] 19.9 Actor Details Ability Inspector / Asset workflow
- [x] 19.10 Diagnostics / Budget / Stress / CI contracts

## Architecture

```text
Gameplay input / AI / Script
          |
          v
GameplayAbilityComponent                 GameplayEventRouter
          |                                      ^
          | TryActivateAbility()                 | ActorHandle Event
          v                                      |
GameplayAbilityProgram --------------------------+
          |
          +--> GameplayTagContainer
          +--> GameplayAttributeSet
          |       `--> GameplayModifierSpec
          +--> Cooldown / Cost / Duration
          +--> Target Context
          +--> Phase18 VfxCueRuntime
          `--> Activation / Completion / Cancel Event
```

The component owns only gameplay state for its Actor. Particle, Fluid, Light, PostEffect, and Camera systems remain owned by their existing subsystems.

## 19.1 — Gameplay Tags and Event Data

`Engine/Gameplay/Tags/GameplayTag.h/.cpp` provides a small hierarchical tag type.

Examples:

```text
State
State.CanAct
State.Stunned
Ability.Combat.Pulse
Gameplay.Ability.Pulse.Activated
```

A concrete tag matches itself and its parent query:

```text
State.Stunned matches State.Stunned : true
State.Stunned matches State         : true
State.Stunned matches Sta           : false
```

The dot boundary prevents accidental prefix matching. Empty segments, whitespace, and unsupported punctuation are rejected. `GameplayTagContainer` stores tags sorted and without duplicates so lookup behavior is independent from insertion order.

`GameplayEvent` carries source/target identity through `ActorHandle` fields instead of storing long-lived `Actor*` pointers. It may additionally carry world position, direction, magnitude, Ability name, and a lightweight payload name.

## 19.2 — Deterministic Event Router

`Engine/Gameplay/Events/GameplayEventRouter.h/.cpp` provides synchronous event delivery.

```cpp
GameplayEventSubscriptionHandle subscription =
    GameplayEventRouter::GetInstance()->Subscribe(
        "Gameplay.Ability",
        [](const GameplayEvent& event)
        {
            // Gameplay.Ability.* events arrive here.
        });
```

Important contracts:

- parent-tag subscriptions receive child events;
- exact subscriptions are also available;
- `SubscribeAll` can observe every valid event;
- invalid event tags are rejected and diagnosed;
- subscribers are dispatched in monotonically allocated handle order, not `unordered_map` bucket order;
- Subscribe/Unsubscribe during a callback cannot retroactively alter the current dispatch target snapshot.

## 19.3 — Generic Attributes

`GameplayAttributeSet` owns named values with explicit base/min/max definitions.

```cpp
GameplayAttributeSet& attributes = abilityComponent.GetAttributes();
attributes.Define({ "Energy", 100.0f, 0.0f, 100.0f });
attributes.Define({ "MoveSpeedScale", 1.0f, 0.0f, 3.0f });
```

This is intentionally separate from `CharacterHealthComponent`. A game can bridge Health into the Ability/Event layer later without creating two competing owners for the same HP value.

## 19.4 — Modifiers

Modifiers support three operations:

1. `Add`
2. `Multiply`
3. `Override`

Evaluation is deterministic and always uses that stage order. The latest Override wins by runtime sequence. The final value is clamped to the Attribute definition.

A positive `durationSeconds` expires during `GameplayAbilityComponent::Update`. Zero means the modifier remains until explicitly removed or until the Ability removes it on completion/cancel.

Stacking policies:

- `Independent`: every application creates a separate modifier;
- `RefreshDuration`: same Attribute + stack key reuses the existing modifier and refreshes it;
- `Replace`: same keyed slot is replaced with the new specification.

## 19.5 — Ability Asset / Compiler

Ability files use `.ability.json` with `schemaVersion = 1`.

Sample:

`Resources/Gameplay/Abilities/Phase19Pulse.ability.json`

```json
{
  "schemaVersion": 1,
  "abilityName": "Phase19Pulse",
  "abilityTag": "Ability.Combat.Pulse",
  "targetPolicy": "OptionalActor",
  "cooldownSeconds": 0.5,
  "durationSeconds": 0.35,
  "cost": { "attribute": "Energy", "amount": 10.0 },
  "requiredTags": ["State.CanAct"],
  "blockedTags": ["State.Stunned", "State.Dead"],
  "grantedTags": ["State.Casting"]
}
```

`GameplayAbilitySerializer` strictly parses enum strings and schema version. `GameplayAbilityCompiler` validates names, finite values, tag counts, duplicate/invalid tags, modifier limits, event tags, cost definitions, and VFX configuration before creating `GameplayAbilityProgram`.

The runtime stores a program snapshot on activation. Reloading an Ability asset therefore changes later activations without mutating an Ability already in progress.

## 19.6 — Ability Runtime

`GameplayAbilityComponent` is an `ActorComponent` and registers itself with the existing `ComponentFactory` through the public runtime registration API. It can therefore be added from the existing Add Component UI and restored from Actor JSON without hard-coding another entry into the factory implementation.

Core API:

```cpp
GameplayAbilityComponent* abilities = actor.GetComponent<GameplayAbilityComponent>();
abilities->LoadAbility("Resources/Gameplay/Abilities/Phase19Pulse.ability.json");

GameplayAbilityHandle handle = abilities->TryActivateAbility("Phase19Pulse");
abilities->CancelAbility(handle);
```

Activation validates in this order:

- per-component frame/active/modifier budgets;
- cooldown;
- target requirements;
- required Gameplay Tags;
- blocked Gameplay Tags;
- cost Attribute existence and current value.

On success it spends the cost, starts cooldown, snapshots the compiled program, grants runtime tags, applies self modifiers, starts VFX, and publishes the activation event. Completion and cancel release reference-counted granted tags and configured modifiers.

## 19.7 — Target Context

`GameplayAbilityContext` contains:

- optional ActorHandle target;
- optional explicit world position;
- optional direction;
- scalar strength.

Target policies are `None`, `Self`, `OptionalActor`, and `RequiredActor`. A RequiredActor Ability is rejected when no valid target handle was supplied. World-position VFX falls back to the owning Actor root transform when the caller does not provide one.

## 19.8 — Phase18 VFX Bridge

Ability assets reference a Phase18 Cue instead of copying particle/fluid/light/post-effect settings:

```json
"vfx": {
  "cueName": "Phase18Explosion",
  "cueAssetPath": "Resources/Vfx/Phase18/Explosion.vfx.json",
  "intensityParameter": "Intensity",
  "intensity": 0.65,
  "stopOnAbilityEnd": false
}
```

The component loads the Cue on demand when necessary, calls `VfxCueRuntime::Play`, and applies `vfxIntensity * context.strength` through the existing Phase18 User Parameter API.

This preserves the ownership chain:

```text
Gameplay Ability -> VFX Cue -> Particle / Fluid / Light / PostEffect / Camera
```

## 19.9 — Editor / Inspector Workflow

No second scene editor framework is introduced. `GameplayAbilityComponent::DrawImGui` participates in the existing Actor Details inspector and exposes:

- Ability asset path loading/registration;
- hot reload per registered Ability;
- manual activation;
- cancel-all;
- live Attribute base/final values;
- combined authored/runtime Gameplay Tags;
- cooldown state;
- per-Ability stress activation;
- global Ability/Event budget diagnostics.

The sample Actor prefab `Resources/ActorPrefabs/Phase19AbilityActor.json` is immediately usable in the existing Actor/Prefab workflow. It defines `Energy`, `MoveSpeedScale`, `State.CanAct`, and the Phase19 sample Ability asset.

## 19.10 — Diagnostics / Budget / Stress / CI

Global default budgets:

```text
Registered Abilities / Component : 64
Active Abilities / Component     : 32
Modifiers / Component            : 128
Activations / Frame / Component  : 32
```

Diagnostics track activation attempts/success/rejection, completion/cancel, event publishing, VFX plays, modifier applications, budget rejection, and peak active loads.

`RunStressBurst(abilityName, count)` deliberately drives repeated activation while restoring the tested cost/cooldown between attempts, so budget and active-Ability pressure can be tested without creating a second fake runtime path. The normal `TryActivateAbility` path is still used for every attempt.

Static contracts live under `Tests/Phase19` and are wired into `TeamDevelopmentCI`. The Windows jobs continue to compile every configured C++ translation unit in both Debug and Release with warning-as-error behavior inherited from the project.

## Sample setup

Spawn or load:

```text
Resources/ActorPrefabs/Phase19AbilityActor.json
```

Then select the Actor and open its `GameplayAbilityComponent` in Details. `Phase19Pulse` should already be registered from the prefab. The inspector can Activate, Reload, or Stress it.

The sample demonstrates the full chain:

```text
State.CanAct
   -> Phase19Pulse activation
   -> Energy -10
   -> State.Casting granted
   -> MoveSpeedScale modifier
   -> Gameplay.Ability.Pulse.Activated event
   -> Phase18Explosion VFX Cue
   -> duration completes
   -> tag/modifier cleanup
   -> Gameplay.Ability.Pulse.Completed event
```

## Validation boundary

Repository CI validates schema/static contracts, module ownership, asset/reference checks, existing automated tests, existing GPU Particle HLSL validation, and Debug/Release C++ translation-unit compilation. A final executable launch and gameplay/GPU observation remain development-machine runtime validation tasks because the hosted CI intentionally performs compile-only C++ validation rather than a full executable link/run.
