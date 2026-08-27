# Production Engine 開発進捗

## 開発計画

[添付企画書のリポジトリ内コピー](ProductionEngineRoadmap.md)を開発計画とする。コピー元の内容は変更しない。Phase番号は企画書とこの進捗管理にのみ使用し、クラス/関数/UI/コメント/GPU Debug Nameへ追加しない。

## Phase 30 — Engine Stabilization

### 30.1 Engine / Gameplay / Editor依存関係整理

- 基準master: `d472d306`（2026-08-27、originをfetchして一致を確認）。
- ブランチ: `codex/engine-dependency-audit`。
- 調査: [構造・問題一覧](EngineArchitectureAudit.md)、[Manager / Singleton一覧](EngineServiceInventory.md)。
- 今回の構造調査・最小修正・下記検証を完了。Core/Runtime/Editorの全面分離は未完了で、残課題を調査書に記録した。
- 修正: GameApplicationをApplication所有へ移し、既存module validatorの短いinclude名・相対includeによる逆依存の検出漏れを閉じた。
- ビルドから判明した追加修正: BasicParticleActorのImGui includeとUI本体をUSE_IMGUIで囲み、Releaseコンパイルを復旧した。Actorの生成/Particle再生処理は維持。
- 維持: Actor / Component、既存処理順、GPU所有/同期、Water/VFX/Physics、既存Manager数。
- Legacy削除: なし（旧GameApplication配置の解消はクラス削除ではない）。互換転送ヘッダーや新Manager/Singletonは追加していない。

### コミット

| コミット | 内容 |
| --- | --- |
| `3e15e71` | 開発計画、構造調査、94 Singleton / 11通常Managerの一覧をコード修正前に記録 |
| `4676616` | GameApplicationをApplicationLayerへ移動。WinMain/project/filter参照を更新 |
| `9867942` | BasicParticleActorのEditor UIをReleaseから除外 |
| `43ed0d2` | 逆依存検査と回帰テストを追加 |

この記録の最終更新は別のdocsコミットにまとめる。リモートへのpushは行っていない。

### 後続の扱い

30.1終了時点では30.2以降は未着手だった。30.2の実施結果は本書末尾に追記する。調査で得た棚卸し・実行順・GPU所有表はその判断材料とするが、Phase 30全体の完成とは扱わない。特にGPU Leakなし、Scene反復、Debug/Release安定動作、既存Water/VFX/Physicsの維持は実機検証が必要。

## ビルド・テスト記録

検証日: 2026-08-27。Visual Studio 18 Community / MSBuild 18.9.1、x64、Python 3.12.13。生成ログは `Generated/EngineAudit` に保存し、Gitには含めない。

| 検証 | 結果 |
| --- | --- |
| 変更前DebugソリューションBuild（リンクを含む） | 成功、208警告 / 0エラー |
| GameApplication移動後Debug Build | 成功、0警告 / 0エラー |
| 最初のRelease Build | 失敗、BasicParticleActorのImGui条件分岐漏れによる4エラーを検出 |
| 修正後ReleaseソリューションBuild（リンクを含む） | 成功、32警告 / 0エラー。C4189/C4100などの残存警告は今回変更しない |
| 最終DebugソリューションBuild（リンクを含む） | 成功、0警告 / 0エラー |
| ValidateEngineModules.py | 成功。Engine→Application候補0件、各sourceの所有が一意 |
| ValidateProjectAssets.py / CheckBrokenReferences.py | ともに成功 |
| unittest discover / CIと同じテストファイル直接実行 | 7テスト成功。うち逆依存は3 Module × 8 include表記のsubtestを含む |
| 既存PowerShellスクリプトの構文解析 | BuildAssetCommon / BuildTextures / BuildMeshes / BuildFonts / PackageRelease / RunSoakTestの6本成功 |
| 起動・正常終了smoke: Debug | 3.00647秒 / 171フレーム、終了コード0、timeoutなし |
| 起動・正常終了smoke: Release | 3.00617秒 / 175フレーム、終了コード0、timeoutなし |
| 変更したソースの工程名検査 | 開発工程番号なし。既存の工程名コメントも変更対象箇所では意図を表すコメントへ変更 |

警告数は各ビルドの出力値であり、最終2回はincremental build。変更前のフルコンパイルと警告数が異なることを「全警告を除去した」とは扱わない。Releaseは最初の全体ビルドで他の翻訳単位もコンパイルされ、修正後に再コンパイル・リンクを完了した。

### 再実行コマンド

リポジトリルートを作業ディレクトリとし、MSBuild/PythonをPATHへ通した環境で実行する。

```powershell
msbuild Project/Ken4lowEngine.sln /m:2 /t:Build /p:Platform=x64 /p:Configuration=Debug
msbuild Project/Ken4lowEngine.sln /m:2 /t:Build /p:Platform=x64 /p:Configuration=Release
python Project/Tools/Scripts/ValidateEngineModules.py
python Project/Tools/Scripts/ValidateProjectAssets.py
python Project/Tools/Scripts/CheckBrokenReferences.py
python -m unittest discover -s Project/Tests -p "test_*.py" -v
python Project/Tests/test_engine_modules.py -v
```

今回のsmokeは `Generated/EngineAudit/RuntimeDebug` / `RuntimeRelease` を作業場所にし、Resources/Config/ExternalsはProjectから参照、Editor layoutはコピーを使った。既存の `KEN4LOW_SOAK_SECONDS=3` と `KEN4LOW_RELIABILITY_CSV` による通常終了を使い、テスト後に環境変数を復元した。CSVと `runtime-smoke-results.json` が結果。`RunSoakTest.ps1` のbudget判定や長時間stressを実行したという意味ではない。

### 残課題・未実施

- DEP-02: FrameworkのCore分類、Runtime→Editor循環、VFX Graph Editor等の所有分類。
- UPD-01〜03: Scene間のPhysics更新差、GPU更新とPIE停止/stepの統一、SampleSceneのEditor二重更新。
- LIFE-01〜03: GPU最終参照と解放、Singleton終了/再初期化、例外時の部分初期化cleanup。
- 開始Scene overrideの扱い、残存ビルド警告、回帰テストの拡充。
- GPU debug layerのメッセージ採取/ライブオブジェクト検査、Scene切替反復、Editor操作、Frames in Flight ON、例外注入、Water/VFX/Physicsの目視比較、長時間stressは未実施。

短時間smokeの成功だけでGPU Leakなし・長時間安定・全機能維持とは判定しない。

## 30.2 Manager / Singleton棚卸し

実施日: 2026-08-27。

- 基準: 前回の変更が取り込まれたmaster `752d48f`。originをfetchし、masterをfast-forward確認（Already up to date）してから`codex/service-lifetime-audit`を作成。
- 調査成果: [サービス所有・寿命の棚卸し](EngineServiceLifetimeAudit.md)。[宣言一覧](EngineServiceInventory.md)の94 GetInstance公開型 / 11通常Manager、105型をすべて確認。
- 各型に所有者、Initialize/Finalize担当、Lazy Initialize、再初期化、借用ポインタ/handle/callback、Thread利用、Scene跨ぎ、Singleton必要性を記録。候補の変更リスクと効果も記載。
- 本体の初回生成と資源の遅延初期化、単一共有サービスの必要性とglobal singletonの必要性、コード上の再初期化経路と実測済み動作を区別した。
- 状態: 今回の棚卸し・最小修正・下記検証を完了。候補の通常所有化/統合/削除、Engine全体の再初期化保証は未実施で、Phase 30全体の完了ではない。

### 分類結果

| 分類 | Singleton一覧の型 | 通常Manager | 合計 |
| --- | ---: | ---: | ---: |
| 維持 | 36 | 9 | 45 |
| 通常所有へ変更候補 | 41 | 0 | 41 |
| 統合候補 | 6 | 0 | 6 |
| 削除候補 | 0 | 1 | 1 |
| 今回判断保留 | 11 | 1 | 12 |
| 合計 | 94 | 11 | 105 |

候補は将来の検証対象であり、採用済み件数ではない。宣言一覧と調査の型名集合、重複、項目の欠落、参照リンクを機械照合し105/105一致を確認した。

### 実施した最小修正

[Framework.cpp](../Project/Engine/Core/Application/Framework.cpp)にAudioManagerのincludeと既存Finalize呼出を追加（空行・意図コメントを含め4行）。

基準版ではAudioManagerは再生時に遅延起動するが、通常終了からFinalizeが呼ばれていなかった。GameApplicationによるScene/Component破棄とFrameworkによる非同期処理停止の後、WinAppのCoUninitializeより前にVoice/PCM/XAudio2/Media Foundationを終了する。未初期化時のFinalizeは既存のno-opを使い、起動時に音声を強制初期化しない。

- Actor/Component、既存Update/FixedUpdate/Render順、Water/VFX/Physicsの実装・呼出順は変更なし。
- Manager/Singletonの追加・削減: 0件。新ServiceLocatorや所有rootの追加なし。
- Legacy削除: なし。LevelObjectManagerは削除候補に留め、CollisionManager等は置換しない。
- 変更したコードに処理意図の1行コメントを追加。工程番号は追加せず、変更ファイルの工程名検査も一致0件。

### コミット

| コミット | 内容 |
| --- | --- |
| `4afdb5a` | コード変更前に105型の所有・寿命・分類・リスクと効果を記録 |
| `bc1b6af` | Scene/Job終了後、COM終了前に既存AudioManager::Finalizeを接続 |

本進捗・検証結果の更新は別のdocsコミットにまとめる。masterへのmergeおよびリモートへのpushは行っていない。

### ビルド・既存回帰テスト

環境: Visual Studio 18 Community / MSBuild 18.9.1、x64、Python 3.12.13、Windows SDK DXC 10.0.26100.0。ログ・一時検証プログラムは`Generated/ServiceInventory`内（Git対象外）。

| 検証 | 結果 / 証跡 |
| --- | --- |
| DebugソリューションBuild（リンク含む） | 成功、0警告 / 0エラー、15.28秒。`debug-build.log` |
| ReleaseソリューションBuild（リンク含む） | 成功、2警告 / 0エラー、34.08秒。既存FrameUploadArena.hのC4189（hr未使用）。`release-build.log` |
| ValidateEngineModules.py | 成功。source所有とEngine→Application境界を確認。`engine-modules.log` |
| ValidateProjectAssets.py / CheckBrokenReferences.py | ともに成功。`project-assets.log` / `broken-references.log` |
| unittest discover / CIと同じ直接テスト実行 | 同じ既存7テストが両方式とも成功（3 Module × 8 include表記のsubtestを含む）。`unittest-discover.log` / `ci-test-file.log` |
| CIの既存PowerShell構文検査 | BuildAssetCommon / BuildTextures / BuildMeshes / BuildFonts / PackageRelease / RunSoakTest、6本成功。`ci-checks.log` |
| CIのDerived Data Cache smoke | Store/Restore/Hit/書込byte数/SHA256一致を確認、成功（17 bytes）。既存cacheを削除せず専用Generatedディレクトリで実施 |
| CIのGPU Particle HLSL検査 | 既存9 shaderを同じentry point/profileでDXCコンパイル成功。`ci-checks.log` / `Dxil/` |
| CIのPackageRelease -DryRun | 成功。実際の配布ZIPは作成していない。`package-dry-run.log` |
| 棚卸し整合性 | 105/105、重複0、各項目・分類・リンクを確認。`inventory-coverage.log` |
| Git差分・工程名 | 差分の空白エラーなし。製品コード変更はFramework.cppの4行のみ、工程番号追加なし |

両構成ともincremental Buildであり、全warning除去やclean rebuildを意味しない。既存テストはmodule/asset/参照等の回帰検査で、Water/VFX/Physicsの実機動作を網羅するものではない。

### 追加の動作検証

| 検証 | 結果 |
| --- | --- |
| 音声lifecycle: Debug | 成功、exit 0、timeoutなし。未初期化F、遅延I、二重I、無音BGM/SE、F、二重F、再Iを確認 |
| 音声lifecycle: Release | 成功、exit 0、timeoutなし。同じcycleを確認 |
| 製品実行ファイルの起動・通常終了: Debug | 成功、172フレーム / 3.00005秒、exit 0、timeoutなし |
| 製品実行ファイルの起動・通常終了: Release | 成功、182フレーム / 3.00668秒、exit 0、timeoutなし |

音声は実装のAudioManager.cpp/MFAudioDecoder.cppを一時console harnessへコンパイルして実行した。各構成で2 cycle、実際のVoiceを2つ生成し、Finalize後のGetMemoryStatsでactiveVoiceCount/cachedClipCount/decodedPcmBytesがすべて0、旧handleが非再生であることを確認。無音WAVと音量0を使用した。OS全体の音声リーク検査やAPIの並列利用試験ではなく、Engine全体の再起動保証でもない。harnessは恒久CIへ追加していない。

初回の一時音声試験はfixtureを`silence.wav`だけで指定し、既存NormalizePathによりResources/Soundsへ解決されて失敗した。harnessを`./silence.wav`へ修正して両構成を再実行し成功。製品コードのpath規則は変更していない。証跡は`audio-results-initial.json`、`audio-results.json`、`audio-*.log`。

製品smokeは前回用意した隔離runtime directory（Resources/Config/Externals参照とEditor layoutのコピー）を再利用し、今回のCSV/結果は`Generated/ServiceInventory`へ保存。既存の`KEN4LOW_SOAK_SECONDS=3`で通常終了し、変更した環境変数は復元した。`runtime-smoke-results.json`と`Debug-smoke.csv` / `Release-smoke.csv`が証跡。RunSoakTestの長時間budget判定は実施していない。

### 再実行

リポジトリルートで、前節のMSBuild/Pythonコマンドを実行する。追加で既存CIと同じPowerShell構文/DDC/HLSL検査と`PackageRelease.ps1 -DryRun`を実行した。今回のローカル検証補助は以下（Generated内のため別checkoutには含まれない）。

```powershell
python Generated/ServiceInventory/run_regression_checks.py
./Generated/ServiceInventory/InvokeCiChecks.ps1
./Project/Tools/Scripts/PackageRelease.ps1 -ProjectRoot (Resolve-Path Project).Path -DryRun
msbuild Generated/ServiceInventory/AudioLifecycle.vcxproj /p:Platform=x64 /p:Configuration=Debug
msbuild Generated/ServiceInventory/AudioLifecycle.vcxproj /p:Platform=x64 /p:Configuration=Release
./Generated/ServiceInventory/InvokeAudioChecks.ps1
./Generated/ServiceInventory/InvokeRuntimeSmoke.ps1
```

### 残課題

- LightManagerのI/F担当がObject3DCommon/AnimationPipelineBuilderで重複。共有light/parameter/GPU順序を検証してから担当を整理する。
- PlanarReflectionの終了担当・保持target、EnvironmentMapの借用handle失効、PostEffect RTのdescriptor返却と再初期化。GPU待機を含む検証が必要。
- EditorのSceneManager/selection/Undo/gizmo借用、Level/Scene遅延要求のreset、AssetBuild worker終了とlog寿命。
- VfxGraphRuntime/GpuParticleEffectRuntime/EffectSystemの全体終了とbackend handle整合。CueのFinalizeがあるだけで全VFX終了が保証されるわけではない。
- ParameterManagerのcallback解除、特にLegacy CollisionManagerのthis capture。GameplayEventRouterの実運用scopeと購読解除契約。
- 通常所有候補の41型、統合候補の6型、削除候補の1型は未変更。候補ごとに回帰範囲を確認して小単位で進める。
- Scene切替反復、PIE/Undo/drag中の破棄、GPU live object、Frames in Flight/resize/capture、例外注入、Water/VFX/Physicsの目視比較、長時間stress、全105サービスの反復I/F・並列競合テストは未実施。

短時間smoke・音声資源統計・ビルド成功から、全機能の動作維持やGPU Leakなしまで確認できたとは扱わない。

## 30.3 Update / FixedUpdate / Render順序整理

実施日: 2026-08-27。

- 基準: 前回の棚卸し・Audio終了修正が取り込まれたmaster `823c3d4`。originをfetch、masterのfast-forward確認（Already up to date）の後、`codex/frame-execution-audit`を作成した。
- 調査成果: [EngineFrameExecutionAudit.md](EngineFrameExecutionAudit.md)。Framework::RunからUpdate、Scene/Actor/Component、CPU Physics、GPU各Simulation、VFX、Editor/PIE、PostEffect、Render/Presentまでの実際の呼出順を記録。
- 重点確認: Scene別Physicsの差、共通FixedUpdateの不在、GPU更新がSceneより先であること、Draw中のFluid、PIE停止/step、Editor二重更新、同一frame複数Drawのguardと限界。
- 状態: 今回の調査、最小修正、下記検証を完了。CPU/GPUの停止契約統一や固定刻み化は未実施。Phase 30全体の完了ではない。

### 調査結果と分類

12項目を`維持 / 明確なバグ / 設計改善候補 / 今回変更しない`に分類した。バグ判定と今回修正するかは分け、影響の大きいものは変更していない。

| 分類 | 件数 | 主な内容 |
| --- | ---: | --- |
| 維持 | 2 | 既存Run/Render順、Fluidの同一World・frame slot/fenceによる重複防止と内部pass順 |
| 明確なバグ | 3 | SampleSceneのEditor二重更新、PIE停止/stepのCPU/GPU不一致、MainWorldRender再呼出時のSPH interaction重複防止不足 |
| 設計改善候補 | 5 | Scene別Physics、Scene前のGPU更新、Draw依存のFluid、Timer/Streaming入力時刻、Scene差替えと未送信GPU参照 |
| 今回変更しない | 2 | 共通FixedUpdate API導入、Water/VFX/PostEffectの表示用時計変更 |

特に確認した点:

- DebugSceneはPhysicsWorldを値所有し、Initializeで`SetUseFixedStep(false)`を指定。Actor Update → transform確定 → Physics → PostPhysics → transform確定の既存scheduleを使う。DataDrivenScene/SampleSceneはActorWorld.UpdateだけでPhysics Stepを持たない。
- GPU Particle / Liquid Pre / SPH / Liquid PostはScene Updateより前。Fluid 2D/3DはActorWorld.Draw内で最新のEmitter/Colliderを収集して進む。
- PIE PauseはScene Runtimeを止めるが、GPU Particle・SPH・Fluid・VFXを共通には止めない。SPH/Fluidの独自Pause/StepはDiagnosticsPanel由来で、PIE Stepと連動していない。WaterSurfaceもUpdateEditorで時間を進める。
- 現在のShadow/Probe/Planarは専用描画でありActorWorld.Drawの再帰実行ではない。Fluidには同じWorld/slot/fenceで重複を避ける実装がある。一方、Draw冒頭のSPH–Rigidbody interactionには同じgateがなく、MainWorldRenderを同じframeで複数回呼ぶ将来の経路では、再dispatch・未送信readback消費の危険がある。
- `GameTimer::EndFrame`のUpdate末尾呼出は内部でEndUpdate相当としてreturnする。Draw末尾の呼出と合わせてフレームを二重確定しているわけではない。

### 実施した最小修正

[SampleScene.h](../Project/ApplicationLayer/Scene/SampleScene/SampleScene.h)の`UpdateEditor`を、DataDrivenSceneと同様にScene固有処理だけのhook（現在はno-op）へ変更した。

基準版ではSceneManagerが`scene_->UpdateEditor`を呼んだ後、`RefreshEditorVisualState`から同じWorldを更新していた。SampleSceneのhook内のWorld更新だけを取り除き、World更新の担当を既存のSceneManagerに揃えた。製品コードは2行追加 / 2行削除（未使用引数表記と処理意図の1行コメントを含む）。

- Actor/Component設計、Runtime Update、Physics Step、Render順、GPU Particle/SPH/Liquid/Fluid/VFX実装は変更なし。共通FixedUpdate API、Manager、Singletonの追加なし。
- SampleSceneのEdit/PauseではComponent更新が2回から1回になる。Water等のEditor previewも二重加算されなくなるが、Component自身の時間計算・挙動は変更していない。他Sceneの更新回数は維持。
- Legacy削除: なし。削除したのは重複していたWorld更新呼出のみ。
- Phase番号はコード・コメント・ImGui・GPU Debug Nameに追加していない。変更した製品ファイルの工程名検査は一致0件。

### コミット

| コミット | 内容 |
| --- | --- |
| `1fa1bee` | コード変更前に実行順、PIE/複数Drawの到達条件、分類とリスクを記録 |
| `2ff3891` | SampleSceneのEditor World二重更新だけを除去 |

検証結果と本進捗の更新は別のdocsコミットにまとめる。masterへのmergeとリモートへのpushは行っていない。

### Debug / Release Build・既存テスト

環境: Visual Studio 18 Community / MSBuild 18.9.1、x64、Python 3.12.13、Windows SDK DXC 10.0.26100.0。ログと一時検証補助は`Generated/FrameExecution`内（Git対象外）。

| 検証 | 結果 / 証跡 |
| --- | --- |
| DebugソリューションBuild（リンク含む） | 成功、0警告 / 0エラー、13.45秒。`debug-build.log` |
| ReleaseソリューションBuild（リンク含む） | 成功、2警告 / 0エラー、72.12秒。既存FrameUploadArena.hのC4189（hr未使用）。`release-build.log` |
| ValidateEngineModules.py | 成功。`engine-modules.log` |
| ValidateProjectAssets.py / CheckBrokenReferences.py | ともに成功。`project-assets.log` / `broken-references.log` |
| unittest discover / CIと同じ直接テスト実行 | 同じ既存7テストが両方式とも成功。`unittest-discover.log` / `ci-test-file.log` |
| CIのPowerShell構文検査 | BuildAssetCommon / BuildTextures / BuildMeshes / BuildFonts / PackageRelease / RunSoakTest、6本成功。`ci-checks.log` |
| CIのDerived Data Cache smoke | Store/Restore/Hit/SHA256一致、17 bytes成功。既存cacheを削除せず専用ディレクトリを使用 |
| CIのGPU Particle HLSL検査 | 既存9 shaderを同じentry point/profileでDXCコンパイル成功。`ci-checks.log` / `Dxil/` |
| CIのPackageRelease -DryRun | 成功。配布ZIPは作成していない。`package-dry-run.log` |
| 前回棚卸しの整合性 | 94 Singleton / 11通常Manager、105/105、重複0、分類・項目・リンク整合。`inventory-coverage.log` |
| 今回の文書・差分 | 調査の12分類行とリンク先存在を確認。Git差分の空白エラーなし。製品変更はSampleScene.hのみ |

Buildは両構成ともincrementalでありclean rebuildや全警告除去ではない。既存7テストはmodule境界等の検証で、Simulationの時間積分や画面を網羅するテストではない。

### 実際のScene更新経路による追加検証

一時console harness `SceneUpdateContract.cpp`から実物のSampleScene/DataDrivenSceneを生成し、製品のSceneManager → ActorWorld → Actor → 計数用Componentを通して検証した。Engine実装は既存Buildのobjectをリンクし、WinMain/GameApplication/製品SceneFactoryを除外。SampleSceneは変更したheaderをharnessで再コンパイルし、期待する更新順を別実装やmockで代用していない。

| 検証 | 結果 |
| --- | --- |
| 修正前Debug | **二重更新を再現**。SampleSceneのEdit 3 tickでComponent更新6回、Pause 1 tickで2回。計数assert 5件失敗、exit 1。DataDrivenSceneは期待どおり。`native-baseline.log/json` |
| 修正後Debug | 成功、28項目一致、exit 0、timeoutなし。Edit 3 tickで3回、Pause 1 tickで1回。Play、Single StepでRuntimeが1回だけ増加、Step後Pause維持、翌tickはEditorのみ、非Active Actorのskip、暗黙PostPhysicsなしを両Sceneで確認。`native-fixed-debug.log/json` |
| 修正後Release | 成功、8項目一致、exit 0、timeoutなし。両Sceneで3 tickにRuntime 3回、Editor 0回、暗黙PostPhysicsなし、非Active Actorのskip。`native-fixed-release.log/json` |

PIEは空Worldのsnapshotで実際に開始してからRuntime Worldへ計数用Componentを付け、実物のPause/Step要求を消費する。Componentのserialization代替実装は使わない。検証中はGPU資源を持つActorを生成せず、画面やGPU停止はこのharnessの確認対象ではない。終了はSceneManager::Finalize経路であり、Stopボタンによる編集内容復元の試験とは区別する。

harness Buildも成功。Debugは最適化リンクと既存objectのEdit-and-Continue指定の組合せによるLNK4075が1件、Releaseは0警告。製品Buildの警告とは別。今回恒久CIのnative test基盤は追加しておらず、harnessと生成vcxprojはGeneratedに残す。基準版でのassert失敗は不具合再現の記録であり、修正後の検証失敗ではない。

### 短時間Runtime smoke

両構成のBuild完了を確認した後、実行バイナリの更新時刻とSHA256を記録するsmokeを再実行し、以下を最終結果とした。

| 構成 | 製品exeの結果 |
| --- | --- |
| Debug | 成功、176フレーム / 3.0091秒、exit 0、timeoutなし |
| Release | 成功、182フレーム / 3.00789秒、exit 0、timeoutなし |

証跡: `runtime-smoke-confirmed-results.json`、`Debug-smoke-confirmed.csv`、`Release-smoke-confirmed.csv`。開始時刻は両exeの更新時刻より後である。先行smokeのログも別名で保持し、最終結果には混ぜていない。

作業ディレクトリは既存の`Generated/EngineAudit/RuntimeDebug` / `RuntimeRelease`。Resources/Config/Externalsを参照し、Editor layoutはコピーを使用。`KEN4LOW_SOAK_SECONDS=3`で既存の通常終了経路を使い、環境変数は復元した。RunSoakTestの長時間budget判定、GPU debug layer採取、Water/VFX/Physicsの目視比較は実施していない。

### 再実行

リポジトリルートで実行する（MSBuild/PythonをPATHへ設定した場合）。

```powershell
msbuild Project/Ken4lowEngine.sln /m:2 /t:Build /p:Platform=x64 /p:Configuration=Debug
msbuild Project/Ken4lowEngine.sln /m:2 /t:Build /p:Platform=x64 /p:Configuration=Release
python Project/Tools/Scripts/ValidateEngineModules.py
python Project/Tools/Scripts/ValidateProjectAssets.py
python Project/Tools/Scripts/CheckBrokenReferences.py
python -m unittest discover -s Project/Tests -p "test_*.py" -v
python Project/Tests/test_engine_modules.py -v
./Project/Tools/Scripts/PackageRelease.ps1 -ProjectRoot (Resolve-Path Project).Path -DryRun
```

ローカル検証補助（Generated内のため別checkoutには含まれない）:

```powershell
./Generated/FrameExecution/InvokeCiChecks.ps1
msbuild Generated/FrameExecution/SceneUpdateContract.vcxproj /p:Platform=x64 /p:Configuration=Debug
msbuild Generated/FrameExecution/SceneUpdateContract.vcxproj /p:Platform=x64 /p:Configuration=Release
python Generated/FrameExecution/run_native_check.py Debug fixed-debug
python Generated/FrameExecution/run_native_check.py Release fixed-release
./Generated/FrameExecution/InvokeRuntimeSmoke.ps1 -RunLabel rerun
```

### 残課題

- FRAME-07: PIEのCPU/GPU停止・Single Step契約。Editor previewとRuntimeを区別し、accumulator、Ocean coupling、SPH reaction/readbackまで含めて設計する必要がある。
- FRAME-08: SPH–Rigidbody interactionの同一GPU frame再入防止とreadbackの送信/完了確認。現行MainWorldRenderは1回だが、複数Drawへの一般的な安全性は保証しない。
- FRAME-03/05/06: Scene別Physics所有・固定刻み、Scene前のGPU入力、Drawに依存したFluid更新。今回これらの順序は変えていない。
- FRAME-12と前回の寿命課題: Scene切替時の未送信GPU参照、World pointer再利用、複数World、Finalize/reconfigure/resizeとslot安全性。
- GPUを有効にしたPIE Pause/Step、Frames in Flight ON/OFF、同一frameの複数MainWorldRender、Water/VFXの目視、GPU live object、長時間stress、native検証の恒久CI化は未実施。

短時間smokeとCPU更新回数の一致だけで、GPU Simulation全体の停止保証や全機能の動作維持を確認できたとは扱わない。
