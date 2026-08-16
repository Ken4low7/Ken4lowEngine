# Phase 24 — GPU Execution Graph / Optimization

Phase20〜23で構築したVFX GraphとGPU Particle backendを維持したまま、1フレーム内のUpdate / Emit実行順とGPU resource state遷移を軽量Execution Graphとして整理し、不要なGPU workを削減する。

## 実装チェックリスト

- [x] 24.1 Update / Emitを`GpuParticleExecutionPass`として表現
- [x] 24.2 Update → Emitの既存依存順をExecution Graphで固定
- [x] 24.3 Active Particleが存在しないフレームの全Particle Update dispatchを省略
- [x] 24.4 複数Compute Passを1つのParticle UAV state intervalへ集約
- [x] 24.5 Pass間へ明示的なglobal UAV ordering barrierを挿入
- [x] 24.6 Root Signature / Particle UAV / PerFrame CBの共通bindを1回へ集約
- [x] 24.7 Update / Emit PSOの冗長bindを削減
- [x] 24.8 Phase22 Collision / EventとPhase23 Trail / MeshのGPU layoutを維持
- [x] 24.9 Execution Graph cost estimator / static regression contractを追加
- [x] 24.10 Phase13〜24のstacked CIを追加

## 基本方針

Phase24でもGPU Particle backendを二重化しない。

```text
VFX Graph
   ↓ compile
GpuParticleEmitterDesc
   ↓
GpuParticleEmitter
   ↓ BuildCB / RequestEmit
GpuParticleExecutionGraph
   ├─ Update Pass
   └─ Emit Pass × N
   ↓
既存GpuParticleComputePipeline
```

Execution Graphは新しいParticle simulationではなく、既存Update / Emit computeを安全かつ少ないcommand overheadで実行するためのCPU側scheduleである。

## Phase23までの実行

従来は1フレームで次のように処理していた。

```text
DispatchUpdate
  SRV -> UAV
  Update
  UAV -> SRV

DispatchEmit A
  SRV -> UAV
  Emit A
  UAV -> SRV

DispatchEmit B
  SRV -> UAV
  Emit B
  UAV -> SRV

...
```

Update 1回 + Emit N回ならParticle Bufferのstate transition数は

```text
2 × (1 + N)
```

となる。

例えばEmitが8件あるフレームでは18回のstate transitionになる。

## Phase24 Execution Graph

Phase24ではまず実行予定を構築する。

```text
ExecutionGraph
  [0] Update
  [1] Emit A
  [2] Emit B
  [3] Emit C
  ...
```

その後、全Passを1つのUAV区間で実行する。

```text
Particle Buffer
SRV
 ↓ transition
UAV
 ├─ Update
 ├─ UAV Barrier
 ├─ Emit A
 ├─ UAV Barrier
 ├─ Emit B
 └─ ...
 ↓ transition
SRV
```

GPU workが1つ以上あるフレームのParticle Buffer state transitionは原則2回になる。

```text
Legacy : 2 × PassCount
Phase24: 2
```

8 Emit + Updateなら、

```text
Legacy : 18 transitions
Phase24: 2 transitions + 8 UAV ordering barriers
```

となる。

## UAV ordering barrier

単純にstate transitionだけを消すと、連続するUAV read/writeの依存関係が曖昧になる。

そのためPass間には

```cpp
D3D12_RESOURCE_BARRIER_TYPE_UAV
```

を挿入する。

`pResource = nullptr`のglobal UAV barrierとしており、Particle Bufferだけでなくfree-list / counter / Event Bufferなど、同じCompute Pass群が扱うUAV全体の順序を保つ。

これにより

```text
Update writes
   ↓ guaranteed ordering
Emit reads/writes
   ↓ guaranteed ordering
next Emit
```

という依存を維持する。

## Update → Emit順序

Phase23までの重要な契約として、UpdateはEmitより先に走る。

```text
Existing Particle → Update
New Particle      → Emit
```

したがって新規Particleは生成されたそのフレームではUpdateされず、次フレームからsimulationへ入る。

これはPhase23 Ribbon / Trailのzero-age history処理とも整合するため、Phase24でもExecution GraphのUpdate Passを必ずEmit Passより前へ置く。

## Idle Update culling

従来はParticleが1個も存在しなくても毎フレーム全Particle Updateをdispatchしていた。

Phase24ではEmitterが持つCPU側のactive batch推定を利用する。

```text
estimated active particle > 0
    ├─ yes -> Update Passを追加
    └─ no  -> Update Passを省略
```

新規Emitだけが存在するフレームは

```text
Update: skip
Emit  : execute
```

となる。

完全idleならExecution Graph自体が空になるためParticle compute commandを発行しない。

CPU側active batchはlifeTimeRandomとSub Emitter lifetimeを含む保守的な寿命推定を使っている。Collisionによる早期死亡などでは余分なUpdateが残る方向に倒れるため、安全側のcullingとなる。

## Bind batching

Execution Graph内では共通状態を1回だけbindする。

```text
UAVManager::PreDispatch()
Compute Root Signature
Particle UAV descriptor
PerFrame CB
```

Update PSOは最大1回、Emit PSOも最大1回のbindにする。

従来の`DispatchEmit()`はEmitterごとに同じPSO / Root Signature / Descriptorを再bindしていたため、Emitter数が増えるほどCPU command recording overheadが増えていた。

## Module fusion

Phase20〜23のGraph ModuleはVFX Graph Compilerで`GpuParticleEmitterDesc`へloweringされ、GPU側では既存`GpuParticleUpdate.CS.hlsl`の1 Update dispatch内でGravity / Drag / Curve / Collision / Eventなどを処理する。

つまり現在のbackendには「Moduleごとに別dispatch」という構造がない。

そのためPhase24で無理に別Shaderを統合するのではなく、すでに融合済みのUpdate kernelを維持し、実際に存在していたUpdate / Emit間の冗長barrierとbindを最適化対象にした。

## GPU layout

Phase24ではParticle data layoutを変更しない。

```text
GpuEmitterCBData = 624 bytes
ParticleCS       = 544 bytes
```

Phase23の

```text
previousTranslate
```

もそのまま維持する。

Collision / Event / Sub Emitter用のPhase22 fieldsも変更しない。

## Cost estimator

`GpuParticleExecutionGraphStats`はExecution Graphの構造から次を計算できる。

- Update Pass count
- Emit Pass count
- skipped Update count
- legacy transition estimate
- batched transition estimate
- UAV barrier estimate
- pipeline switch estimate

これはPhase28のProfiling / Diagnosticsへ接続できる最小の計測契約として残す。

## Async Computeについて

現在のEngineはGPU Particleを既存Graphics command list上で記録している。

Phase24で別Compute Queueを追加すると、

- queue ownership
- fence
- graphics / compute間同期
- descriptor lifetime
- render前wait

まで同時に変更する必要があり、VFX最適化の範囲を超えて同期リスクが大きくなる。

そのため今回のExecution Graphはqueue非依存のpass modelまでを作り、実行自体は既存command list上で行う。

将来Async Computeを導入する場合も、`GpuParticleExecutionPass`をqueue schedulerへ渡せる境界ができたため、Manager内へ個別dispatchを増やす必要はない。

## Regression contract

Phase24 static testsでは次を固定する。

- Update PassがEmit Passより前
- idle Update culling
- normal `Update()`がlegacy `DispatchUpdate()` / `DispatchEmit()`を直接呼ばない
- Particle Buffer transitionが2回
- common root/UAV bindが1回
- Pass間global UAV barrier
- Update / Emit PSO bindの集約
- `emitDispatchCount_` diagnostics維持
- Phase23 544-byte Particle layout維持

## Phase25への境界

次のPhase25は **VFX Graph Editor / Preview** を扱う。

Phase24では実装しない項目:

- Node Graph GUI
- Node placement / connection editing
- Curve / Gradient editor UI
- Preview viewport
- live compile error visualization
- asset browser integration

Phase24はRuntime execution schedulingとGPU work削減までで完結させ、Authoring UIはPhase25へ分離する。
