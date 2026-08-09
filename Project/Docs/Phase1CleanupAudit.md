# Phase 1 Cleanup Audit

このドキュメントは、FPSゲーム削除後の残骸整理、Engine / Gameplay / Editor の責務分類、Legacy候補の棚卸し、Assetメモリ計測、毎フレームAllocation計測の結果を記録するものです。

## Phase 1 完了状況

- [x] FPS削除後の残骸を洗い、削除済みファイルへの参照を整理する
- [x] Engine / Gameplay / Editor を分類する
- [x] Legacyコード候補一覧を作る
- [x] Assetメモリ使用量を計測できるようにする
- [x] 毎フレームAllocationを計測できるようにする

補助作業として、再実行可能な監査スクリプトとEditor Diagnosticsへの表示も追加しました。

---

## 1. FPS削除後の残骸整理

### 実際に整理したもの

`Ken4lowEngine.vcxproj` / `Ken4lowEngine.vcxproj.filters` に残っていた、既に実ファイルが存在しない旧FPSゲーム由来の登録を削除しました。

主な対象:

- `ApplicationLayer/Character/Player/...`
- `ApplicationLayer/Character/Enemy/...`
- `ApplicationLayer/Character/Boss/...`
- `ApplicationLayer/DebugTools/EnemyScalability/...`
- `Engine/Graphics/Camera/FPS/FpsCamera.cpp`
- `Engine/Graphics/Camera/FPS/FpsCamera.h`

さらに、存在しない `Engine/Editor/EditorViewportPicking.h` のproject / filter登録も整理しました。

`AdditionalIncludeDirectories` についても実在ディレクトリと照合し、削除済みディレクトリへのinclude pathを整理しました。

旧ファイルをbuild対象から外すためだけに存在していた `Directory.Build.targets` は、`.vcxproj` 本体の整理後に不要となったため削除しました。

### 最終監査結果

`AuditPhase1Cleanup.ps1` をWindows runner上で実行し、以下を確認しました。

- Visual Studio project の存在しない参照: **なし**
- 存在しない `AdditionalIncludeDirectories`: **なし**
- `Directory.Build.targets` の除外項目: **なし**

名称スキャンでは以下のような現存コードも検出されますが、名前だけを理由に削除していません。

- `Engine/Core/Time/LegacyFPSCounter/FPSCounter.*`
- `Engine/Graphics/PostEffect/Effects/PlayerHealthPostEffect/...`
- `Engine/Graphics/Renderer/Animation/Core/AnimationPlayer.*`
- `Engine/Physics/Collision/Specialized/BulletEnemyCollisionSoA.*`
- `ApplicationLayer/Scene/DebugScene/Validation/PlayerMigrationValidationComponent.h`

特に `LegacyFPSCounter/FPSCounter` は現在 `GameTimer` から利用されているフレームレート計測コードであり、FPSゲームの `FpsCamera` 残骸とは別物です。

---

## 2. Engine / Gameplay / Editor 分類

監査スクリプトによる現在の一次分類:

- Engine: 495 files
- Gameplay移行候補: 21 files
- Gameplay / Application: 22 files
- Editor: 45 files
- Other: 0 files

### Engine Runtimeとして維持する領域

- `Engine/Core`
- `Engine/Math`
- `Engine/Platform`
- `Engine/Graphics`
- `Engine/Physics`
- `Engine/System/Input`
- `Engine/System/Audio`
- 汎用Actor / Component基盤
- 汎用Scene / Level基盤

### `Engine/Scene/Actor/Character`

現在は汎用Character基盤とGameplay寄り処理が同じディレクトリにあります。Phase 1では物理移動を行わず、Phase 2で移動するための分類を確定しました。

#### Engine / 汎用Character基盤として残す候補

- `CharacterActor`
- `CharacterMovementComponent`
- `CharacterColliderComponent`
- `CharacterAnimationComponent`

#### Gameplay Framework候補

- `CharacterHealthComponent`
- `CharacterDamage`
- `CharacterTargetComponent`

#### Gameplay / Combat側へ移す候補

- `AttackComponent`
- `AttackBehaviors`
- `HumanoidCharacterActor`
- `HumanoidDefinition`
- `HumanoidVisualComponent`

### Editor

Editor固有責務は `Engine/Editor` を中心に扱います。

代表例:

- Outliner
- Details / Inspector
- Content Browser
- Transform Gizmo
- Selection
- Diagnostics
- PIE / Editor固有UI

物理的なmodule分離やディレクトリ移動はPhase 2で実施します。

---

## 3. Legacyコード候補一覧

### 移行状況を確認してから整理するもの

#### `Engine/Physics/Collision/Legacy`

以下のLegacy Collisionコードが現存しています。

- `CollisionBroadPhase.h`
- `CollisionHitResult.h`
- `CollisionManager.cpp/.h`
- `CollisionPreset.h`
- `CollisionPresetLibrary.cpp/.h`
- `CollisionTypeIdDef.h`
- `CollisionTypes.h`
- `ObjectCollisionResponseMatrix.h`
- `TraceResponseMatrix.h`

新Physics / Collision APIへの移行確認なしに一括削除するのは危険なため、Phase 2以降の移行対象として残します。

#### `Engine/Misc/Audio/MFAudioDecoder.*`

`Engine/System/Audio/Manager/AudioManager.h` から現在も利用されています。

したがって削除対象ではありません。将来的に `Engine/System/Audio` 配下へ責務を集約する移動候補です。

#### `Engine/Core/Time/LegacyFPSCounter`

名称はLegacyですが、`GameTimer` が `FPSCounter` を現在利用しています。

削除不可です。将来Time系を再設計する際の置き換え候補として扱います。

### Gameplay漏れ / 要整理候補

- `Engine/Graphics/PostEffect/Effects/PlayerHealthPostEffect`
- `Engine/Physics/Collision/Specialized/BulletEnemyCollisionSoA`
- `ApplicationLayer/Scene/DebugScene/Validation/PlayerMigrationValidationComponent.h`

これらは現存する有効コードなのでPhase 1では削除していません。Engine汎用層に置くべきか、Gameplay側へ寄せるべきかをPhase 2で判断します。

### API整理候補

- `ModelManager::LoadObjFile`
- `ModelManager::FindModel`
- 旧Collision APIと新Physics APIの重複箇所

利用状況を確認してから整理します。

---

## 4. Assetメモリ使用量計測

### Texture

`TextureManager::GetMemoryStats()` で以下を取得します。

- ロード済みTexture数
- Texture用Descriptor数
- 推定GPU payload bytes

`TexMetadata` と `DirectX::ComputePitch` を使い、Mip / Array / Depthを考慮した論理payloadを概算します。

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

現行 `Model::Initialize()` は `ModelData::subMeshes` のgeometryを `Mesh` 側にもコピーするため、CPU geometryが二重保持されます。

CPU概算は現在実際に保持している `ModelData` 側と `Mesh` 側の両方を含む値として扱います。GPU側はVertex / Indexの論理payloadです。

### Audio

`AudioManager::GetMemoryStats()` で以下を取得します。

- 生存中のキャッシュ済みAudio Clip数
- Active Voice数
- デコード済みPCM保持量

AudioManagerのmutexを取得して読み取り、PCMは `std::vector<BYTE>::capacity()` を基準に概算します。

### PerformanceMonitor

Texture / Model / Audio統計を `PerformanceStats` へ統合しました。

Asset統計は毎フレームではなく約0.5秒間隔で更新します。

`trackedAssetMemoryMB` は以下の合計です。

- Texture GPU
- Model CPU
- Model GPU
- Audio PCM

これはプロセス全体のWorking Setや総VRAM使用量ではなく、Engineが追跡している主要Asset payloadの概算です。

### Editor表示

`EditorDiagnosticsPanel` のProfilerタブに `Memory / Allocation` セクションを追加しました。

表示項目:

- Tracked Asset
- Texture GPU
- Model CPU
- Model GPU
- Audio PCM
- Textures
- Models
- Audio Clips
- Texture SRV
- Audio Voices
- Alloc / Frame
- Alloc MB / Frame
- Peak Alloc
- Peak Alloc MB

UI上にもAsset値が概算であることを表示します。

---

## 5. 毎フレームAllocation計測

`Engine/DebugTools/Performance/FrameAllocationTracker.h` を追加しました。

DebugビルドではDebug CRTの `_CrtSetAllocHook` を使い、Frameworkの1フレーム全体で以下を記録します。

- Allocation回数
- Allocation要求bytes
- Peak Allocation回数
- Peak Allocation要求bytes

フレーム境界:

1. `BeginFrame()`
2. Window / Resize処理
3. `Update()`
4. `Draw()`
5. `EndFrame()`

仕様:

- `_CRT_BLOCK` は除外
- 既存CRT Hookがあれば呼び出しをチェーン
- `Finalize()` で元のHookへ復元
- Worker Thread上のフレーム中Allocationもprocess-wideで計測
- `PerformanceMonitor::Reset()` でPeakもリセット
- Releaseビルドでは安全に無効化

`PerformanceMonitor` は直前に完了したフレームの値を読むため、Editor表示は1フレーム遅れます。

`realloc` はその時点で要求されたsizeをAllocation trafficとして加算します。この値はLive Memory増加量ではなく、そのフレームで発生したAllocation要求量を見る指標です。

---

## 再監査方法

Projectディレクトリを基準に以下で実行します。

```powershell
pwsh ./Tools/Scripts/AuditPhase1Cleanup.ps1 -OutputPath "Generated/Phase1Audit.md"
```

監査対象:

- `.vcxproj` の `ClCompile` / `ClInclude`
- 存在しないproject参照
- `AdditionalIncludeDirectories`
- `Directory.Build.targets` のRemove項目（ファイルが存在する場合）
- FPS / Player / Enemy / Boss / Bullet / Weapon等の名称残存
- Engine / Gameplay / Editorの一次分類
- `Engine/Scene/Actor/Character` の現存ファイル

名称スキャンは候補抽出であり、名前だけでLegacyと断定しません。

---

## 検証結果

GitHub ActionsのWindows / Visual Studio環境でPhase 1の最終監査とコンパイル確認を実施しました。

### 監査

- 存在しないVisual Studio project参照: 0
- 存在しないAdditionalIncludeDirectories: 0
- 旧 `Directory.Build.targets` のRemove項目: 0

### Debug

`Ken4lowEngine.vcxproj` の `ClCompile` ターゲット:

- 成功
- Warning: 0
- Error: 0

### Release

`Ken4lowEngine.vcxproj` の `ClCompile` ターゲット:

- 成功
- Warning: 33
- Error: 0

Releaseのwarningはコンパイルを阻害していません。Phase 1で追加したコードについてDebug / Release双方でC++コンパイルが完了することを確認しました。

### フルLinkについて

Debugのsolution全体Rebuildも試行しましたが、GitHub Actions runner上に `assimp-vc143-mtd.lib` が存在しないため `LNK1104` でLinkのみ失敗しました。

この失敗は今回追加したC++コードのコンパイルエラーではありません。C++コンパイル検証は上記のcompile-onlyでDebug / Release双方成功しています。

---

## Phase 1 結論

Phase 1で予定していた以下の5項目は完了です。

1. FPS削除後の残骸整理
2. Engine / Gameplay / Editor分類
3. Legacyコード一覧作成
4. Assetメモリ使用量計測
5. 毎フレームAllocation計測

Phase 2では、この監査結果を基準にCharacter / Gameplayの物理分離、Legacy Collision / Audio責務整理、World / Scene等のより大きな構造変更へ進みます。
