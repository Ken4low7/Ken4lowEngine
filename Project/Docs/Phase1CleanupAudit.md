# Phase 1 Cleanup Audit

このドキュメントは、FPS削除後の残骸整理と Engine / Gameplay / Editor の責務分類、Legacy候補の棚卸し、プロファイリング導入の結果を記録するものです。

## Phase 1 進捗

- [x] FPS削除後の残骸を洗う
- [x] Engine / Gameplay / Editor を分類する
- [x] Legacyコード一覧を作る
- [x] Assetメモリ使用量を計測できるようにする
- [x] 毎フレームAllocationを計測できるようにする

## 1. FPS削除後の残骸

### 確認できたもの

`Ken4lowEngine.vcxproj` には、既に削除された旧FPSゲーム由来のファイル登録が残っています。

主な対象は以下です。

- `ApplicationLayer/Character/Player/...`
- `ApplicationLayer/Character/Enemy/...`
- `ApplicationLayer/Character/Boss/...`
- `ApplicationLayer/DebugTools/EnemyScalability/...`
- `Engine/Graphics/Camera/FPS/FpsCamera.*`

現在は `Directory.Build.targets` の `ClCompile Remove` / `ClInclude Remove` が、これらの削除済みファイルをVisual Studio projectのビルド対象から外す互換ガードとして機能しています。

そのため、`Directory.Build.targets` だけを先に削除すると、`.vcxproj` に残る存在しないファイル参照が再び有効になり、ビルドを壊す可能性があります。Phase 1では無理に削除せず、残骸として検出・記録し、`.vcxproj` と同時に整理できる状態まで監査しました。

`Project/Tools/Scripts/AuditPhase1Cleanup.ps1` で、今後も存在しないproject参照と旧ゲーム固有名称を再検出できます。

## 2. Engine / Gameplay / Editor の分類

### Engineとして維持する領域

以下はエンジン基盤として扱います。

- `Engine/Core`
- `Engine/Math`
- `Engine/Platform`
- `Engine/Graphics`
- `Engine/Physics`
- `Engine/System/Input`
- `Engine/System/Audio`
- 汎用Actor / Component基盤
- 汎用Scene / Level基盤

### Gameplay / Gameplay Framework 移行候補

`Engine/Scene/Actor/Character` には、汎用Character基盤とGameplay寄りの機能が混在しています。

汎用基盤として残す候補:

- `CharacterActor`
- `CharacterMovementComponent`
- `CharacterColliderComponent`
- `CharacterAnimationComponent`

Gameplay Frameworkとして整理する候補:

- `CharacterHealthComponent`
- `CharacterDamage`
- `CharacterTargetComponent`

Gameplay側へ移す候補:

- `AttackComponent`
- `AttackBehaviors`
- `HumanoidCharacterActor`
- `HumanoidDefinition`
- `HumanoidVisualComponent`

Phase 1では物理的なファイル移動を行いません。依存関係を整理した上でPhase 2で移行します。

### Editor

Editor固有の責務は `Engine/Editor` を中心に扱います。

代表例:

- Outliner
- Details / Inspector
- Content Browser
- Gizmo
- Selection
- Diagnostics
- PIE / Editor固有UI

Runtime側からEditor固有状態を参照している箇所があれば、Phase 2以降でサービス分離の対象にします。

## 3. Legacyコード一覧

### 移行を確認してから整理する候補

#### `Engine/Physics/Collision/Legacy`

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

名前だけを理由に削除せず、新しいCollision / Physics APIへの移行状況を確認してから整理します。

#### `Engine/Misc/Audio`

`Engine/Misc/Audio/MFAudioDecoder.cpp/.h` が残っていますが、現在の `Engine/System/Audio/Manager/AudioManager.h` が `MFAudioDecoder.h` を利用しています。

したがって、これは現時点では削除不可です。将来的にはDecoderを `Engine/System/Audio` 配下へ移し、Audio責務を一か所へ集約する候補です。

#### Visual Studio projectの旧登録

旧Player / Enemy / Boss / FpsCamera等の明示登録は、`.vcxproj` 側のLegacy構成として整理対象です。現在は `Directory.Build.targets` が互換ガードになっています。

### 要調査候補

以下は名前や構成だけでは削除可否を断定しません。

- `Engine/Core/Time/LegacyFPSCounter`
- `ModelManager::LoadObjFile`
- `ModelManager::FindModel`
- 旧Collision APIと新Physics APIの重複箇所

利用箇所を確認してからPhase 2以降で判断します。

## 4. Assetメモリ計測

主要Asset payloadの概算値を取得できるようにしました。

### Texture

`TextureManager::GetMemoryStats()` で以下を取得します。

- ロード済みTexture数
- Texture用Descriptor数
- 推定GPU payload bytes

`DirectX::TexMetadata` と `DirectX::ComputePitch` を使い、Mip / Array / Depthを考慮した論理payloadを合計します。

含まないもの:

- D3D12 Heap alignment
- Driver residency
- Transient upload resource
- Driver / allocator内部オーバーヘッド

### Model

`ModelManager::GetMemoryStats()` で以下を取得します。

- ロード済みModel数
- 推定CPU geometry bytes
- 推定GPU geometry bytes

現行 `Model::Initialize()` は `ModelData::subMeshes` の頂点・Indexから `Mesh` へGeometryをコピーしており、CPU側にGeometryが二重保持されます。

CPU概算は、この現行設計で実際に保持される `ModelData` 側と `Mesh` 側の両方を含む値として扱います。GPU側はVertex / Indexの論理payloadです。

Material、文字列、map/node/animationの内部オーバーヘッドやD3D12 Heap alignment等は含みません。

### Audio

`AudioManager::GetMemoryStats()` で以下を取得します。

- 生存中のキャッシュ済みAudio Clip数
- Active Voice数
- デコード済みPCM保持量

AudioManagerのmutexを取得して統計を読み、PCMは `std::vector<BYTE>::capacity()` を基準に保持量を概算します。

### PerformanceMonitor

`PerformanceStats` にTexture / Model / Audio統計を統合しています。

Asset統計は毎フレームではなく約0.5秒間隔で更新し、プロファイリング自体による負荷を抑えます。

`trackedAssetMemoryMB` はTexture GPU + Model CPU + Model GPU + Audio PCMの主要payload概算の合計であり、プロセス全体のWorking Setや総VRAM使用量ではありません。

## 5. 毎フレームAllocation計測

`Engine/DebugTools/Performance/FrameAllocationTracker.h` を追加しました。

DebugビルドではDebug CRTの `_CrtSetAllocHook` を使って、Frameworkの1フレーム単位で以下を記録します。

- Allocation回数
- Allocation要求バイト数
- Peak Allocation回数
- Peak Allocation要求バイト数

`_CRT_BLOCK` は除外し、既存CRT Hookが存在する場合は呼び出しをチェーンし、終了時に元のHookへ戻します。

Frameworkのフレーム境界は以下です。

1. `BeginFrame()`
2. Window / Resize処理
3. `Update()`
4. `Draw()`
5. `EndFrame()`

Worker Thread上でフレーム中に発生したDebug CRT Allocationもprocess-wideで計測対象になります。

Releaseビルドでは安全に無効化されます。

`PerformanceMonitor` は直前に完了したフレームの結果を参照するため、表示値は1フレーム遅れます。

注意: `realloc` は新たに要求されたサイズをAllocation trafficとして加算します。これは「そのフレームで増加したLive Memory量」ではなく、「そのフレームで要求されたAllocation量」を見るための指標です。

`PerformanceMonitor::Reset()` ではAllocationのPeakもリセットします。

## 再監査方法

Projectディレクトリを基準に、以下で監査結果を再生成できます。

```powershell
pwsh ./Tools/Scripts/AuditPhase1Cleanup.ps1 -OutputPath "Generated/Phase1Audit.md"
```

監査スクリプトは以下を確認します。

- `.vcxproj` の `ClCompile` / `ClInclude`
- 存在しないファイル参照
- `Directory.Build.targets` のRemove項目
- FPS / Player / Enemy / Boss / Bullet / Weapon等の名称残存
- Engine / Gameplay / Editorのおおまかな分類
- `Engine/Scene/Actor/Character` の現存ファイル

`$(...)` のMSBuild macro、`%(...)` のItem metadata、wildcardは通常ファイルとして存在チェックしないため、誤検出を避けます。

## Phase 1で意図的に行わないこと

- Character系ファイルの大規模移動
- `Engine/Physics/Collision/Legacy` の一括削除
- `Engine/Misc/Audio` の削除
- `.vcxproj` の残骸だけを無理に削除して `Directory.Build.targets` の互換ガードを壊すこと
- Custom Allocator / Memory Poolの導入
- World / Scene / Rendererの大規模再設計

## Phase 2へ持ち越す項目

1. `Engine/Scene/Actor/Character` の依存関係を精査し、Engine / Gameplay Framework / Gameplayへ物理的に分離する。
2. `.vcxproj` の旧Player / Enemy / Boss / FpsCamera登録を削除し、その後 `Directory.Build.targets` の互換ガードを縮小または削除する。
3. `Engine/Misc/Audio/MFAudioDecoder` を `Engine/System/Audio` 配下へ統合するか判断する。
4. `Engine/Physics/Collision/Legacy` の利用箇所を洗い、新Physics APIへの移行順序を決める。
5. `LegacyFPSCounter`、旧OBJ Loader、重複Model API等の利用箇所を確認する。

## 検証状況

- GitHub上の現行featureブランチを基準に静的確認を実施。
- 新規Allocation trackerはheader-onlyのため、新しい `.cpp` のVisual Studio project登録を必要としない構成にした。
- Debug / Release双方で呼び出しコードを共有し、ReleaseではCRT Hook処理をコンパイル対象外にした。
- この作業環境ではVisual Studioによる実ビルドは実行していないため、ビルド成功は未確認。
