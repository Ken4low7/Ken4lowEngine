# Phase 22 — Collision / Events / Sub Emitters

Phase20のVFX Graph Foundation、Phase21のParticle Module / Curve / Gradientを維持したまま、GPU Particle UpdateへCollisionとEvent駆動Sub Emitterを追加する。

## 実装チェックリスト

- [x] 22.1 Collision / Event / SubEmitter Graph型を追加
- [x] 22.2 Plane Collisionを追加
- [x] 22.3 Sphere Collisionを追加
- [x] 22.4 Bounce / Slide / Kill Responseを追加
- [x] 22.5 Collision Eventを追加
- [x] 22.6 Death Eventを追加
- [x] 22.7 Collision latchで接触中の多重Event発火を抑止
- [x] 22.8 GPU free-listを利用したSub Emitter生成を追加
- [x] 22.9 JSON Serializer / Compiler / Runtime伝播を追加
- [x] 22.10 Sample Graph / Regression Test / CIを追加

## 基本方針

### EventはPhase22ではGPU-local

Particle更新は既存どおりGPU上で完結する。

```text
Parent Particle
    ↓
GpuParticleUpdate.CS.hlsl
    ↓
Collision / Death
    ↓
Transient Event Bit
    ↓
GPU Free List Allocate
    ↓
Child Particle Spawn
```

CollisionやDeathのたびにGPUからCPUへreadbackすると同期点が増えるため、Phase22のSub Emitter EventはUpdate Dispatch内だけで消費する。

CPUへEventを可視化するDebug ReadbackやEvent CounterはPhase28 Debug / Profiling / Stress Testで扱う。

## Collision

### Plane

Planeは次のAuthoring値を使用する。

- `planeNormal`
- `planeDistance`
- `particleRadius`

Particle中心とPlaneのsigned distanceがparticleRadius未満になった場合、ParticleをPlane外へ押し戻して接触点を解決する。

### Sphere

SphereはEmitter位置基準のローカル中心をAuthoringし、Emit時にworld位置へ変換する。

- `sphereCenter`
- `sphereRadius`
- `particleRadius`

Particle中心が `sphereRadius + particleRadius` より内側へ入った場合、Sphere表面まで押し戻す。

## Collision Response

### Bounce

法線方向速度へrestitutionを適用し、接線速度へfrictionを適用する。

### Slide

法線方向へめり込む速度を除去し、接線速度をfrictionで減衰する。

### Kill

Collision Eventを発生させた後にParticleを終了し、Death Eventも有効ならDeath Sub Emitterも発火できる。

## Event

Phase22は2種類のEventを持つ。

- `Collision`
- `Death`

`Collision` は `Collision.generateEvent=true` の場合のみproducerとなる。

`Death` は `DeathEvent` Nodeが有効な場合のみproducerとなる。

CompilerはSubEmitterのsourceEventに対応するproducerが存在しないGraphをエラーにする。

## Collision latch

BounceやSlideでParticleが接触面に数フレーム残る場合、毎フレームSub Emitterを生成するとParticle数が急増する。

そのため `GPU_PARTICLE_CUSTOM_COLLISION_LATCHED` を利用する。

```text
非接触
  ↓
初回Collision
  ↓
Event発火 + latch ON
  ↓
接触継続中はEventなし
  ↓
離れる
  ↓
latch OFF
```

再度接触した場合は新しいCollision Eventとして扱う。

## Sub Emitter

Phase22のSub Emitterは親Particleと同じGPU Particle backend・Render Groupを再利用するinline child templateである。

Authoring値:

- `sourceEvent`
- `count`
- `lifeTime`
- `speed`
- `spread`
- `inheritVelocity`
- `startSize / endSize`
- `startColor / endColor`
- `alphaFade`

生成されたChild Particleは親のTexture / Blend / Billboard / Render Groupを継承する。

### 再帰禁止

Child Particleでは次を0へ設定する。

```text
collisionShape
Event Mask
Sub Emitter Event Mask
Sub Emitter Count
```

これによりCollision → SubEmitter → Collision → SubEmitterという指数的な再帰増殖を防ぐ。

## GPU Layout

Phase22でParticleとEmitter Constant BufferへCollision / Event / SubEmitter設定を追加した。

```text
GpuEmitterCBData: 480 bytes → 624 bytes
ParticleCS:       384 bytes → 528 bytes
```

C++側の `static_assert` とHLSL側のフィールド順序を同時に更新し、CPU/GPU stride不一致を回帰テストで固定する。

## Sample

`Resources/VfxGraph/Phase22/CollisionSubEmitterBurst.vfxgraph.json`

2 Emitterを収録する。

### CollisionSparks

```text
Burst
 ↓
Spawn Sphere
 ↓
Initial Velocity
 ↓
Gravity
 ↓
Plane Collision / Bounce
 ↓
Collision Event
 ↓
Sub Emitter
 ↓
Sprite Renderer
```

### DeathPop

```text
Burst
 ↓
Lifetime
 ↓
Death Event
 ↓
Death Sub Emitter
 ↓
Sprite Renderer
```

## Phase23への境界

Phase22ではRibbon / Trail / Mesh Particle Graph Moduleを追加しない。

Phase23では今回のParticle Event基盤を維持しながら、以下を追加する。

- Ribbon Renderer
- Trail history / segment generation
- Mesh Particle Graph Module
- EventからRibbon/Trailを開始するための接続点
