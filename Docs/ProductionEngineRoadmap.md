# Ken4lowEngine 次期開発企画書
## Production Engine Roadmap

### 1. 開発目的

これまでのKen4lowEngineでは、DirectX 12を基盤として描画・物理・Editor・GPU Particle・VFX・Fluid・Waterなど、個別のエンジン機能を段階的に実装してきた。

Water開発では以下の流れまで到達した。

```text
Water
├─ W1  Water Surface
├─ W2  Gerstner Wave
├─ W3  Reflection
├─ W4  Basic Interaction / Buoyancy
├─ W5  GPU SPH Foundation
├─ W6  Spatial Hash / GPU Sort
├─ W7  3D SPH
├─ W8  Screen Space Fluid Rendering
├─ W9  Rigidbody ↔ SPH
└─ W10 Ocean ↔ SPH Integration
```

ここからの目的は、

**「高度な個別技術を持つエンジン」から「実際に1本のゲームを最後まで制作できるエンジン」へ進化させること**

とする。

---

# Phase 30 — Engine Stabilization

まず、これまで大量に追加した機能を整理する。

```text
Phase 30
Engine Stabilization
│
├─ 30.1 Engine / Gameplay / Editor依存関係整理
├─ 30.2 Manager / Singleton棚卸し
├─ 30.3 Update / FixedUpdate / Render順序整理
├─ 30.4 GPU Resource Lifetime整理
├─ 30.5 Descriptor管理改善
├─ 30.6 Debug / Assert / Error Handling統一
├─ 30.7 Legacy / Dead Code削除
├─ 30.8 Naming / Folder構造整理
├─ 30.9 Regression Test整理
└─ 30.10 Stress Test
```

### 完成条件

- エンジン起動・終了時にResource Leakがない
- Scene切替を繰り返しても破綻しない
- Manager間依存が明確
- GPU Resourceの所有者が明確
- Debug / Release双方で安定動作
- Water / VFX / Physics等の既存機能を壊していない

---

# Phase 31 — Asset System 2.0

ゲーム規模を大きくするため、Asset管理を本格化する。

```text
Phase 31
Asset System 2.0
│
├─ 31.1 AssetID / GUID
├─ 31.2 Asset Registry
├─ 31.3 Asset Metadata
├─ 31.4 Dependency Tracking
├─ 31.5 Async Loading
├─ 31.6 Background Streaming
├─ 31.7 Texture / Model Cache
├─ 31.8 Hot Reload
├─ 31.9 Missing Asset Recovery
└─ 31.10 Asset Profiler
```

理想構造：

```text
Actor
 ↓
AssetHandle<Model>

AssetHandle
 ↓
AssetManager
 ↓
Asset Registry
 ↓
Model / Texture / Animation
```

Actorが直接ファイルパスを大量に保持する構造から離れる。

---

# Phase 32 — Scene / World System 2.0

大規模なゲーム空間を扱えるようにする。

```text
Phase 32
World System
│
├─ 32.1 World / Scene責務分離
├─ 32.2 Persistent World
├─ 32.3 Sub Scene
├─ 32.4 Async Scene Loading
├─ 32.5 Scene Streaming
├─ 32.6 World Partition Foundation
├─ 32.7 Spatial Cell
├─ 32.8 Actor Streaming
├─ 32.9 World Origin対策
└─ 32.10 Large World Stress Test
```

最終的には、

```text
World
│
├─ Cell
│   ├─ Actors
│   └─ Components
│
├─ Cell
├─ Cell
└─ Cell
```

として、プレイヤー周辺だけをロードできる構造を目指す。

---

# Phase 33 — Animation System 2.0

SkeletalMeshを「再生できる」段階から、本格的なゲーム用Animation Systemへ進める。

```text
Phase 33
Animation System
│
├─ 33.1 Animation Clip整理
├─ 33.2 Skeleton / Bone Runtime
├─ 33.3 Animation State Machine
├─ 33.4 Blend
├─ 33.5 Blend Space
├─ 33.6 Animation Layer
├─ 33.7 Animation Event
├─ 33.8 Root Motion
├─ 33.9 IK
└─ 33.10 Animation Debugger
```

目標：

```text
Idle
 ↕
Walk
 ↕
Run
 │
 ├─ Jump
 ├─ Attack
 └─ Damage
```

をGameplayコードへ大量の分岐を書かず制御できるようにする。

---

# Phase 34 — Character Controller

PhysicsのRigidbodyとは別に、ゲーム用Character Controllerを完成させる。

```text
Phase 34
Character Controller
│
├─ 34.1 Capsule Movement
├─ 34.2 Ground Detection
├─ 34.3 Slope Detection
├─ 34.4 Step Up / Step Down
├─ 34.5 Sliding
├─ 34.6 Jump
├─ 34.7 Moving Platform
├─ 34.8 Water Movement
├─ 34.9 Root Motion Integration
└─ 34.10 Network Ready Architecture
```

ここでは以前から重視している、

- 壁判定
- 床判定
- 斜面
- Capsule
- 精密な接地状態

をCharacter Movementとしてまとめる。

---

# Phase 35 — Navigation / AI

敵AIをゲーム固有コードだけで構築する状態から、エンジン機能へ引き上げる。

```text
Phase 35
AI System
│
├─ 35.1 Navigation Mesh
├─ 35.2 NavMesh Generation
├─ 35.3 Path Finding
├─ 35.4 Dynamic Obstacle
├─ 35.5 Navigation Agent
├─ 35.6 Perception
├─ 35.7 Blackboard
├─ 35.8 Behavior Tree
├─ 35.9 EQS-like Query
└─ 35.10 AI Debug Visualization
```

構造：

```text
EnemyActor
   │
AIController
   │
Behavior Tree
   │
Blackboard
   │
Navigation
```

Gameplay ActorとAIの意思決定を分離する。

---

# Phase 36 — Audio System 2.0

```text
Phase 36
Audio System
│
├─ 36.1 Audio Asset
├─ 36.2 Audio Source Component
├─ 36.3 3D Spatial Audio
├─ 36.4 Attenuation
├─ 36.5 Doppler
├─ 36.6 Reverb Zone
├─ 36.7 Audio Bus
├─ 36.8 BGM Transition
├─ 36.9 Audio Event
└─ 36.10 Audio Profiler
```

単純なSE再生ではなく、ゲーム空間と連動したAudio Systemを作る。

Waterとも接続し、

```text
Above Water
      ↓
Water Surface
      ↓
Underwater
 ├─ Low Pass
 ├─ Reverb
 └─ Ambient
```

などへ発展できるようにする。

---

# Phase 37 — UI Framework

HUDをゲームごとにハードコードする状態から脱却する。

```text
Phase 37
UI Framework
│
├─ 37.1 Widget
├─ 37.2 Canvas
├─ 37.3 Anchor
├─ 37.4 Layout
├─ 37.5 Button
├─ 37.6 Image / Text
├─ 37.7 Animation
├─ 37.8 Event
├─ 37.9 UI Editor
└─ 37.10 Resolution / DPI対応
```

目標：

```text
Widget
├─ Image
├─ Text
├─ Button
└─ Widget
```

というComponent Tree形式でUIを構築できるようにする。

---

# Phase 38 — Save / Game Data System

```text
Phase 38
Game Data
│
├─ 38.1 SaveGame
├─ 38.2 Serialization
├─ 38.3 Versioning
├─ 38.4 Player Settings
├─ 38.5 Game Settings
├─ 38.6 Key Binding
├─ 38.7 Checkpoint
├─ 38.8 Auto Save
├─ 38.9 Migration
└─ 38.10 Corruption Recovery
```

Engine用JSONとGame Saveを明確に分離する。

---

# Phase 39 — Gameplay Framework

Actor / Componentの上に、ゲーム制作向けFrameworkを構築する。

```text
Phase 39
Gameplay Framework
│
├─ 39.1 GameMode
├─ 39.2 GameState
├─ 39.3 PlayerController
├─ 39.4 Pawn / Character
├─ 39.5 Spawn System
├─ 39.6 Damage System
├─ 39.7 Gameplay Event
├─ 39.8 Gameplay Tag
├─ 39.9 Ability Foundation
└─ 39.10 Gameplay Debugger
```

最終的な関係：

```text
GameMode
│
├─ GameState
│
├─ PlayerController
│       ↓
│     Character
│
└─ Enemy / World Actors
```

既存のActor / Component思想は維持し、その上へGameplay層を構築する。

---

# Phase 40 — Editor 2.0

ここでEditorを本格的なゲーム制作ツールへ進化させる。

```text
Phase 40
Editor 2.0
│
├─ 40.1 Hierarchy改善
├─ 40.2 Inspector改善
├─ 40.3 Content Browser
├─ 40.4 Asset Preview
├─ 40.5 Scene View
├─ 40.6 Game View
├─ 40.7 Gizmo改善
├─ 40.8 Undo / Redo
├─ 40.9 Copy / Paste
└─ 40.10 Editor Layout保存
```

特に重要なのは、

**Undo / Redo**

と

**Content Browser**

とする。

---

# Phase 41 — Prefab / Template System

```text
Phase 41
Prefab System
│
├─ 41.1 Actor Prefab
├─ 41.2 Component Serialization
├─ 41.3 Nested Prefab
├─ 41.4 Prefab Instance
├─ 41.5 Override
├─ 41.6 Apply / Revert
├─ 41.7 Variant
├─ 41.8 Dependency
├─ 41.9 Editor Integration
└─ 41.10 Runtime Spawn
```

これにより、

```text
Enemy.prefab
├─ Model
├─ Collider
├─ Rigidbody
├─ Health
├─ AI
└─ VFX
```

を作り、Sceneへ何体でも配置できるようにする。

---

# Phase 42 — Rendering Production Pass

既存RendererをProduction品質へ引き上げる。

```text
Phase 42
Production Rendering
│
├─ 42.1 Render Graph
├─ 42.2 GPU Driven Rendering
├─ 42.3 Frustum Culling
├─ 42.4 Occlusion Culling
├─ 42.5 GPU Instance Culling
├─ 42.6 LOD
├─ 42.7 TAA
├─ 42.8 Motion Vector
├─ 42.9 HDR / Tone Mapping
└─ 42.10 GPU Profiler
```

Water / Reflection / VFX / Shadow / PostEffectも最終的にはRender Graph上へ統合する。

---

# Phase 43 — Physics Production Pass

```text
Phase 43
Production Physics
│
├─ 43.1 Broad Phase改善
├─ 43.2 Contact Manifold
├─ 43.3 Persistent Contact
├─ 43.4 Friction改善
├─ 43.5 Constraint Solver
├─ 43.6 Joint
├─ 43.7 Continuous Collision Detection
├─ 43.8 Physics Material
├─ 43.9 Physics Debugger
└─ 43.10 Large Scale Stress Test
```

既存の

```text
Integrate
↓
Detect
↓
Resolve
↓
Sleep
```

をProduction Physicsへ発展させる。

---

# Phase 44 — Engine Profiling / Optimization

```text
Phase 44
Performance
│
├─ 44.1 CPU Profiler
├─ 44.2 GPU Profiler
├─ 44.3 Frame Timeline
├─ 44.4 Memory Profiler
├─ 44.5 Asset Memory
├─ 44.6 Draw Call Analysis
├─ 44.7 Compute Analysis
├─ 44.8 Multithread Job System
├─ 44.9 Async Task System
└─ 44.10 Performance Budget
```

最終的には、

```text
Frame 16.67 ms
│
├─ Game       2.1 ms
├─ Physics    1.4 ms
├─ Animation  0.8 ms
├─ Rendering  6.2 ms
├─ VFX        1.3 ms
├─ Fluid      2.0 ms
└─ Other      1.1 ms
```

のように「何が重いのか」をエンジン自身から確認できる状態を目指す。

---

# Phase 45 — Production Game Project

ここまで来たら、新しい機能を追加することを一度止める。

**Ken4lowEngineだけを使用して1本のゲームを制作する。**

```text
Phase 45
Production Game
│
├─ 45.1 Game Concept
├─ 45.2 Prototype
├─ 45.3 Vertical Slice
├─ 45.4 Core Gameplay
├─ 45.5 Enemy / AI
├─ 45.6 Stage
├─ 45.7 VFX / Water / Physics活用
├─ 45.8 UI / Audio
├─ 45.9 Optimization
└─ 45.10 Release Build
```

このPhaseで重要なのは、

**ゲーム制作中に不便だった部分をエンジンへフィードバックすること。**

```text
Engine
   ↓
Game制作
   ↓
問題発見
   ↓
Engine改善
   ↓
Game制作
```

というProduction Loopへ移行する。

---

# Phase 46 — Engine Production Hardening

実際のゲーム制作で見つかった問題を修正する。

```text
Phase 46
Production Hardening
│
├─ Crash Fix
├─ Memory Leak Fix
├─ Workflow改善
├─ Editor改善
├─ Asset Pipeline改善
├─ Loading改善
├─ Rendering最適化
├─ Physics最適化
├─ Documentation
└─ Final Regression
```

---

# 最終ロードマップ

```text
Water W1-W10
      │
      ▼
Phase30 Engine Stabilization
      │
      ▼
Phase31 Asset System 2.0
      │
      ▼
Phase32 World / Streaming
      │
      ├───────────────┐
      ▼               ▼
Phase33 Animation   Phase35 AI
      │               │
      ▼               │
Phase34 Character ◀───┘
      │
      ▼
Phase36 Audio
      │
      ▼
Phase37 UI
      │
      ▼
Phase38 Save / Data
      │
      ▼
Phase39 Gameplay Framework
      │
      ▼
Phase40 Editor 2.0
      │
      ▼
Phase41 Prefab
      │
      ▼
Phase42 Rendering Production
      │
      ▼
Phase43 Physics Production
      │
      ▼
Phase44 Profiling / Optimization
      │
      ▼
════════════════════════════
 Phase45 GAMEを1本作る
════════════════════════════
      │
      ▼
Phase46 Production Hardening
```

# 開発方針

今後は以下を基本ルールとする。

1. Actor / Component設計を維持する
2. Engine / Gameplay / Editorの責務を分離する
3. GPU処理はCPU Readbackを極力避ける
4. Managerを無制限に増やさない
5. 新機能追加時にはDebug Visualizationも作る
6. Editorから調整可能な設計を優先する
7. Serializationを後付けにしない
8. Release Buildでの性能を必ず確認する
9. Regression Testを各Phaseに追加する
10. 「技術的に面白い」だけでなく「ゲーム制作で使えるか」を完成条件に含める

---

# 次回開始地点

次の作業は、

**Phase 30 — Engine Stabilization**

から開始する。

最初のブランチ候補：

`feature/phase30-engine-stabilization`

最初にコードを書くのではなく、

```text
30.1 現在のEngine構造調査
        ↓
Manager一覧
        ↓
Subsystem依存関係
        ↓
Update順
        ↓
Render順
        ↓
Resource所有関係
        ↓
問題一覧作成
```

から始める。

Water W10までのような「機能追加フェーズ」から一度離れ、Phase 30では**今まで作った巨大なエンジンを壊れにくい土台へ整理すること**を最優先とする。