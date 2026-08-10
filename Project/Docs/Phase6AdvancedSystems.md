# Phase 6 — 高度化

## 完了対象

- [x] Job System
- [x] Streaming
- [x] SubLevel
- [x] World Partition的機構
- [x] Render Graph
- [x] memory allocator最適化

## 1. Job System

`Engine/Core/Concurrency/JobSystem` を追加しました。

固定Worker Thread PoolをEngine全体で共有し、CPU側の独立した処理をJobとして投入できます。

主なAPI:

- `Dispatch()`
- `ParallelFor()`
- `Wait()`
- `WaitIdle()`
- `JobPriority` (`Low / Normal / High / Critical`)
- `JobHandle`

Worker数を明示しない場合は、原則として `hardware_concurrency - 1` を使用し、Main Threadを1本残します。

Job内部で発生した例外は `JobHandle` に保持し、必要な呼び出し側で再送出できます。

### Job Systemの責務境界

Worker Threadから直接行わない処理:

- D3D12 CommandList / Deviceの操作
- ActorWorldのSpawn / Destroy / Component変更
- ImGui / Editor UI更新
- Main Camera等のGlobal Runtime State変更

これらはMain Thread側のCommit処理へ戻してから反映します。

## 2. Streaming

`Engine/Core/Streaming/StreamingManager` を追加しました。

Streamingは次の2段階に分離しています。

```text
Worker Thread
  IO / Decode / CPU Preparation
        ↓
Main Thread Completion Queue
        ↓
ActorWorld / GPU / Engine StateへCommit
```

`StreamingManager::Update()` は毎フレームMain Threadから呼び出され、既定では最大4件のCompletionを処理します。

`StreamingRequestHandle` はCancel可能で、Engine終了時は新規Request受付を止め、既存RequestをCancelしたうえでJob SystemのIdleを待ってからQueueを破棄します。

## 3. SubLevel

`Engine/Scene/Streaming/SubLevelManager` を追加しました。

Base Levelを維持したまま別LevelをAdditive Loadできます。

流れ:

```text
SubLevel Request
    ↓
WorkerでJSON File IO
    ↓
Main Thread Completion
    ↓
LevelSerializer::Deserialize
    ↓
Actor Staging
    ↓
Parent graph構築
    ↓
ActorWorld::AppendStagedActors
```

状態は次の4種類です。

- `Unloaded`
- `Loading`
- `Loaded`
- `Failed`

Load中のRequestをUnloadした場合はCancelとGeneration更新を行い、遅れて到着した古いCompletionを無視します。

Loaded Actorは `ActorHandle` で追跡し、Unload時は `ActorWorld::DestroyActor()` へ変換します。

### 現在のSubLevel制約

- SubLevel内の `ParentId` は同じSubLevel内のActorを参照します。
- Base Level Actorを親にするCross-Level ParentはPhase 6では対応しません。
- Nested SubLevelを自動的にさらにStreamingする処理は未実装です。
- Lighting / Editor CameraはBase Levelを所有者とし、SubLevelからGlobal設定を上書きしません。

## 4. World Partition的機構

`Engine/Scene/Streaming/WorldPartitionManager` を追加しました。

これはUnreal EngineのWorld Partitionを完全再現するものではなく、Ken4lowEngine向けの軽量なGrid Streaming基盤です。

Level Version 3では次の情報を保存できます。

```json
{
  "WorldPartition": {
    "Enabled": true,
    "CellSize": 128.0,
    "LoadRadiusCells": 1,
    "UnloadRadiusCells": 2
  },
  "SubLevels": [
    {
      "Id": "Town_0_0",
      "Path": "SubLevels/Town_0_0.json",
      "CellX": 0,
      "CellZ": 0,
      "Priority": 2,
      "AlwaysLoaded": false
    }
  ]
}
```

Camera位置から現在Cellを求め、Chebyshev距離でCellを判定します。

- `distance <= LoadRadiusCells` → Load
- `distance > UnloadRadiusCells` → Unload
- その間 → 現在状態を維持

Load / Unload半径を分けることで、境界付近を移動したときの頻繁なLoad/Unloadを抑えます。

`AlwaysLoaded` SubLevelはCamera位置に関係なくLoad対象になります。

## Level Format Version 3

Phase 6で `Ken4lowLevel` のCurrent Versionを `3` に更新しました。

Migration経路:

```text
Version 1
   ↓
Version 2
   ↓
Version 3
```

Version 2以前のLevelは、空の `SubLevels` と無効状態の `WorldPartition` を追加してインメモリ移行するため、既存Levelを手作業で書き換える必要はありません。

Project Validation / Broken Reference CheckerもVersion 3へ対応し、SubLevel Path切れ、ID重複、World Partition設定不正をCIで検出します。

## 5. Render Graph

`Engine/Graphics/RenderGraph/RenderGraph` を追加し、既存 `RenderPipelineController` の1フレーム描画をPassとして宣言するようにしました。

現在管理するもの:

- Render Pass名
- Read Resource
- Write Resource
- 明示的なPass依存
- Resource access由来の依存
- Topological Sort
- Cycle Detection
- Logical Resource Lifetime (`firstPass / lastPass`)
- Compile stats

既存のRender Pipeline順序を変えてRegressionを起こさないよう、Phase 6ではPassを明示的にchainして従来と同じ順序を維持します。

Render GraphのCompileまたはExecute開始前に検証失敗した場合は、既存の固定順Render PipelineへFallbackします。

### 現在のRender Graph制約

Phase 6のRender Graphは **論理Pass / 依存関係 / Resource lifetime基盤** です。

まだ行わないもの:

- D3D12 transient heapの自動確保
- Resource aliasing
- Queue間同期の自動生成
- Barrierの完全自動生成
- Async Compute scheduling

これらは基盤を使った後続最適化対象です。

## 6. memory allocator最適化

`Engine/Core/Memory/FrameMemory` を追加しました。

`std::pmr::memory_resource` として実装した1フレーム用Linear Allocatorです。

特徴:

- 既定2 MiBのFrame Buffer
- `BeginFrame()` で一括Reset
- 個別deallocateはno-op
- 容量超過時はupstream allocatorへFallback
- Overflow Blockも次Frameで一括解放
- Capacity / Used / High Water / Overflow統計を取得可能

Phase 6ではRender GraphのTopological Sort用scratch (`indegree / lastAccess / ready`) に実際に使用しています。

### Memory最適化の方針

Phase 1で導入したAllocation計測を無視してEngine全体の `new/delete` を独自Allocatorへ置き換えることはしていません。

FrameMemoryの対象は、寿命が明確に「現在Frameだけ」の一時データに限定します。

Actor、Component、Asset、GPU Resourceなど複数Frameに跨るオブジェクトには使用しません。

## Frameworkへの統合

Engine起動:

```text
FrameMemory Initialize
    ↓
JobSystem Initialize
    ↓
StreamingManager Initialize
    ↓
DirectX / Renderer / Asset / Scene
```

毎フレーム:

```text
FrameMemory BeginFrame
    ↓
Streaming Completion Commit
    ↓
World Partition Streaming判定
    ↓
Game / Scene Update
    ↓
Render Graph Execute
```

終了:

```text
WorldPartition Reset
    ↓
Streaming Finalize
    ↓
JobSystem Finalize
    ↓
FrameMemory Finalize
    ↓
Renderer / Asset等の残りの破棄
```

非同期Completionが破棄済みEngine Stateを触らないよう、Streaming / JobをRendererやAssetより先に停止します。

## CI / Validation

Phase 5で導入した `TeamDevelopmentCI` をそのまま継続利用します。

検証内容:

- Engine Module Ownership
- Project / Asset Validation
- Broken Reference Check
- Automated Tests
- Debug C++ Compile
- Release C++ Compile

Level Version 3のSubLevel / World Partition検証もProject Validationへ追加済みです。

Hosted runnerには既存外部依存 `assimp-vc143-mtd.lib` が無いため、これまでと同じくSolution全体のLink成功を偽装せず、C++ Translation UnitのCompileをCI基準にします。

## Phase 6以降に残す高度化

- Work stealing Job Queue
- Job dependency graph / continuation
- AssetSystemのStreamingManager完全統合
- SubLevel間Actor reference
- Nested SubLevel
- Editor上のWorld Partition Grid可視化
- Streaming source複数対応
- Render GraphによるD3D12 Barrier自動生成
- Transient GPU Heap / Resource Alias
- Async Compute
- Thread-local frame allocator
- Size-class allocator / pool allocatorの計測ベース導入
