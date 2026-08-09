# Phase 1 Cleanup Audit

このドキュメントは、FPS削除後の残骸整理、Engine / Gameplay / Editor の責務分類、Legacy候補の棚卸し、メモリ / Allocation計測、およびPhase 2へ進む前の構造的な健全性確認を記録するものです。

## Phase 1の扱い

Phase 1は「残骸を見つけて計測できるようにする」だけでは完了としません。

現在までに完了しているのは **Phase 1-A: 現状把握と計測基盤** です。

Phase 1全体の完了には、実際のbuild設定整理、依存方向の確認、Legacy移行可否の確定、Resource寿命監査、Debug / Release検証まで必要とします。

## Phase 1-A: 現状把握と計測基盤

- [x] FPS削除後の残骸を洗う
- [x] Engine / Gameplay / Editor を一次分類する
- [x] Legacyコード候補一覧を作る
- [x] Assetメモリ使用量を概算できるようにする
- [x] 毎フレームAllocationを計測できるようにする
- [x] 再実行可能な監査スクリプトを追加する

## Phase 1-B: 構造・build設定の実整理

- [ ] `.vcxproj` の削除済みPlayer / Enemy / Boss / FpsCamera登録を実際に削除する
- [ ] `.vcxproj.filters` に同様の残骸がないか確認し整理する
- [ ] `AdditionalIncludeDirectories` の存在しない / 不要なディレクトリを削除する
- [ ] `.vcxproj` 整理後に `Directory.Build.targets` の互換 `Remove=` を縮小または削除する
- [ ] Engine Runtime -> ApplicationLayer の逆依存がないか確認する
- [ ] Runtime -> `Engine/Editor` の直接依存を確認し、Editor-only境界を確定する
- [ ] `Engine/Scene/Actor/Character` の依存関係を追い、汎用Character基盤 / Gameplay Framework / Game固有の3段階に分類する
- [ ] `Engine/Physics/Collision/Legacy` の参照元を洗い、新Physics APIへ移行済み / 未移行を確定する
- [ ] `Engine/Misc/Audio` と `Engine/System/Audio` の責務重複を整理し、Decoderの移動先を確定する
- [ ] `LegacyFPSCounter`、旧OBJ Loader、重複Model APIの利用箇所を確認する
- [ ] Resource Managerの所有権、キャッシュ寿命、Unload / Finalize順序を監査する
- [ ] `shared_ptr` / raw pointer / Singletonについて、所有権が不明瞭な箇所や循環参照リスクを棚卸しする

## Phase 1-C: 計測の実用化と完了判定

- [ ] Allocationの回数 / bytesを表示するだけでなく、増加する処理を特定できる計測手順を決める
- [ ] Texture / Model / Audioメモリ計測の「含むもの / 含まないもの」を固定する
- [ ] ModelのCPU geometry二重保持を実測し、維持するか解消するか判断材料を作る
- [ ] Asset cacheが増え続けるケースを確認し、Unload / Eviction方針の要否を判断する
- [ ] Debugビルドを通す
- [ ] Releaseビルドを通す
- [ ] `AuditPhase1Cleanup.ps1` を実行し、未解決項目を「許容済み」または「修正済み」に分類する
- [ ] Phase 2で大規模なファイル移動 / World・Scene再設計へ入ってもよい状態であることを確認する

---

## 1. FPS削除後の残骸

`Ken4lowEngine.vcxproj` には、既に削除された旧FPSゲーム由来のファイル登録が残っています。

主な対象:

- `ApplicationLayer/Character/Player/...`
- `ApplicationLayer/Character/Enemy/...`
- `ApplicationLayer/Character/Boss/...`
- `ApplicationLayer/DebugTools/EnemyScalability/...`
- `Engine/Graphics/Camera/FPS/FpsCamera.*`

現在は `Directory.Build.targets` の `ClCompile Remove` / `ClInclude Remove` が、これらの削除済みファイルをbuild対象から外す互換ガードとして機能しています。

この状態は「壊れてはいない」が「整理済み」ではありません。Phase 1-Bで `.vcxproj` 側の旧登録を削除してから、互換 `Remove=` を縮小します。

また `AdditionalIncludeDirectories` にも旧ディレクトリが残る可能性があるため、監査スクリプトでファイル登録だけでなくinclude pathも確認します。

## 2. Engine / Gameplay / Editor の一次分類

### Engine Runtimeとして維持する候補

- `Engine/Core`
- `Engine/Math`
- `Engine/Platform`
- `Engine/Graphics`
- `Engine/Physics`
- `Engine/System/Input`
- `Engine/System/Audio`
- 汎用Actor / Component基盤
- 汎用Scene / Level基盤

### Character周辺

`Engine/Scene/Actor/Character` には汎用Character基盤とGameplay寄り処理が混在しています。

汎用基盤として残す候補:

- `CharacterActor`
- `CharacterMovementComponent`
- `CharacterColliderComponent`
- `CharacterAnimationComponent`

Gameplay Framework候補:

- `CharacterHealthComponent`
- `CharacterDamage`
- `CharacterTargetComponent`

Game / Combat側へ寄せる候補:

- `AttackComponent`
- `AttackBehaviors`
- `HumanoidCharacterActor`
- `HumanoidDefinition`
- `HumanoidVisualComponent`

これは名前だけによる一次分類です。Phase 1-Bではinclude / factory登録 / serialization依存まで追って最終分類します。

### Editor

Editor固有責務は `Engine/Editor` を中心に扱います。

代表例:

- Outliner
- Details / Inspector
- Content Browser
- Gizmo
- Selection
- Diagnostics
- PIE / Editor固有UI

Phase 1-BではRuntime側がEditorクラスを直接要求していないか確認し、Release境界を明確にします。

## 3. Legacy候補

### `Engine/Physics/Collision/Legacy`

Legacyディレクトリが現存しています。

例:

- `CollisionBroadPhase.h`
- `CollisionHitResult.h`
- `CollisionManager.cpp/.h`
- `CollisionPreset.h`
- `CollisionPresetLibrary.cpp/.h`
- `CollisionTypeIdDef.h`
- `CollisionTypes.h`
- `ObjectCollisionResponseMatrix.h`
- `TraceResponseMatrix.h`

名前だけで一括削除せず、参照元と新Physics APIへの移行状況を確認してから整理します。

### `Engine/Misc/Audio`

`Engine/Misc/Audio/MFAudioDecoder.cpp/.h` は現在の `Engine/System/Audio/Manager/AudioManager.h` から利用されています。

現時点では削除不可です。責務としては `Engine/System/Audio` 側へ集約する候補です。

### その他の要調査候補

- `Engine/Core/Time/LegacyFPSCounter`
- `ModelManager::LoadObjFile`
- `ModelManager::FindModel`
- 旧Collision APIと新Physics APIの重複箇所
- Visual Studio projectの旧Player / Enemy / Boss / FpsCamera登録

## 4. Assetメモリ計測

### Texture

`TextureManager::GetMemoryStats()`:

- ロード済みTexture数
- Texture用Descriptor数
- 推定GPU payload bytes

Mip / Array / Depthを考慮した論理payloadの概算です。

含まないもの:

- D3D12 Heap alignment
- Driver residency
- Transient upload resource
- Driver / allocator内部オーバーヘッド

### Model

`ModelManager::GetMemoryStats()`:

- ロード済みModel数
- 推定CPU geometry bytes
- 推定GPU geometry bytes

現行 `Model::Initialize()` は `ModelData::subMeshes` から `Mesh` へgeometryをコピーするため、CPU geometryが二重保持される構造です。現在のCPU概算は両方の保持量を含めます。

Phase 1-Cで、この二重保持が実際にどの程度効いているか確認します。

### Audio

`AudioManager::GetMemoryStats()`:

- 生存中のキャッシュ済みAudio Clip数
- Active Voice数
- デコード済みPCM保持量

PCMは `std::vector<BYTE>::capacity()` を基準に概算します。

### PerformanceMonitor

`PerformanceStats` にTexture / Model / Audio統計を統合しています。

Asset統計は約0.5秒間隔で更新します。

`trackedAssetMemoryMB` は主要payloadの概算であり、process Working Setや総VRAM使用量ではありません。

## 5. 毎フレームAllocation計測

`Engine/DebugTools/Performance/FrameAllocationTracker.h` を追加しています。

DebugビルドではDebug CRTの `_CrtSetAllocHook` を使い、Frameworkの1フレーム単位で以下を記録します。

- Allocation回数
- Allocation要求bytes
- Peak Allocation回数
- Peak Allocation要求bytes

Frameworkの境界:

1. `BeginFrame()`
2. Window / Resize処理
3. `Update()`
4. `Draw()`
5. `EndFrame()`

Releaseでは無効化されます。

現在は「フレーム全体のAllocation trafficを観測する」段階です。Phase 1-Cでは、どのSubsystem / 処理で増えているかを切り分ける運用または計測ポイントを追加します。

## 再監査方法

Projectディレクトリを基準に実行します。

```powershell
pwsh ./Tools/Scripts/AuditPhase1Cleanup.ps1 -OutputPath "Generated/Phase1Audit.md"
```

監査対象:

- `.vcxproj` の `ClCompile` / `ClInclude`
- 存在しないファイル参照
- `Directory.Build.targets` のRemove項目
- `AdditionalIncludeDirectories`
- FPS / Player / Enemy / Boss / Bullet / Weapon等の名称残存
- Engine / Gameplay / Editorの一次分類
- `Engine/Scene/Actor/Character` の現存ファイル

`$(...)` のMSBuild macro、`%(...)` のItem metadata、wildcardは通常ファイルとして存在チェックしません。

## Phase 2へ進む条件

Phase 2ではCharacterの物理移動、Gameplay Framework分離、Audio / Collision Legacy統合、World / Scene責務分割など、より大きな構造変更を行います。

その前にPhase 1-B / 1-Cを完了し、build設定・依存方向・Resource寿命・計測基準が把握できている状態にします。

## 検証状況

- GitHub上の `feature/phase1-cleanup-profiling` を基準に静的確認を実施。
- Allocation trackerはheader-onlyで追加し、新規 `.cpp` 登録を不要にしている。
- Debug / Releaseで呼び出しコードを共有し、ReleaseではCRT Hook処理をコンパイル対象外にしている。
- Visual StudioによるDebug / Release実buildはまだ未確認。
- Phase 1は未完了。Phase 1-A完了、Phase 1-B / 1-C継続中。
