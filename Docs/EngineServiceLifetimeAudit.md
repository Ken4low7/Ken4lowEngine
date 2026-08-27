# サービス所有・寿命の棚卸し

調査日: 2026-08-27。基準: master `752d48f`、作業ブランチ `codex/service-lifetime-audit`。
[宣言一覧](EngineServiceInventory.md)の94 GetInstance公開型 / 11通常Manager、合計105型を対象とする。数を減らすことは目的にしない。

## 判定の読み方

- **所有者**は現在の実装を記す。関数内staticの本体はC++実行環境が保持し、Framework等はその本体を所有せず、資源の初期化・終了を指示する。通常所有への移行先は現状と区別して「候補」と記す。
- **I/F**はInitialize / Finalize、または同等の登録・解除・破棄を指す。「なし」は通常終了の呼出を検索して見つけなかった場合も明記する。デストラクタでComPtrが破棄されることと、適切な時点でGPU/OS資源を終了することは別。
- **Lazy**は資源の遅延初期化を記す。94型のGetInstance本体は初回アクセスで構築されるが、これだけで資源も初期化されるとは限らない。EditorOutputLogの実データは別途staticメンバーで保持される。
- **再初期化**はコード上の経路の評価。可/条件付きも、全サービスの反復起動を実測した意味ではない。GPU資源は最終GPU参照の完了・descriptor所有者の返却・Device生存が前提。Initializeの二重呼出とFinalize後の起動を区別する。
- **Thread: Main**は現在のUpdate/Draw/UI等からの呼出を確認したもの。明記のない状態には同期がなく、他スレッドから呼べるという保証ではない。GPU dispatch/readbackはCPU worker利用とは区別する。mutexがあっても初期化・終了との競合まで安全とは判定しない。
- **借用**にはraw pointerだけでなく、参照、callbackのcapture、descriptor handle、mapped pointerを含む。Audio Voiceのraw pointerはDestroyVoiceする所有資源であり借用ではない。
- **維持**は今回の構成を維持する判断で、Singleton以外に実装できないという意味ではない。**通常所有へ変更候補 / 統合候補 / 削除候補**は将来の検証対象であり、変更許可や削除確定ではない。**今回判断保留**は寿命・互換性の検証が不足するもの。
- 各項のリスクは所有・構成を変更する場合の退行リスク、効果はその変更によって得られるもの。高リスクな候補を今回実装しない。

## 今回の判断と優先順位

| 分類 | GetInstance公開型 | 通常Manager | 合計 |
| --- | ---: | ---: | ---: |
| 維持 | 36 | 9 | 45 |
| 通常所有へ変更候補 | 41 | 0 | 41 |
| 統合候補 | 6 | 0 | 6 |
| 削除候補 | 0 | 1 | 1 |
| 今回判断保留 | 11 | 1 | 12 |
| 合計 | 94 | 11 | 105 |

候補の採用件数ではなく、現時点の分類数。今回のSingleton削減・Manager追加・Legacy削除はいずれも0件。

| 対象 | 確認した問題 / 効果 | リスクと判断 |
| --- | --- | --- |
| AudioManagerの通常終了 | 再生から遅延起動する一方、既存Finalizeの外部呼出がない。Voice/PCM/XAudio2/MFをCOM終了前に明示解放できる | 低。Scene破棄・Job停止後のFramework終了へ既存Finalize呼出だけ追加。起動・再生・Updateは変更しない |
| LightManagerのI/F担当 | Object3DCommonとAnimationPipelineBuilderの両方がInitialize/Finalizeする | 中〜高。共有Light/Shadow/Parameter登録の担当を一つにする効果はあるが、描画初期化・再起動の検証を先に行う |
| PlanarReflectionManager / EnvironmentMapManager | 前者はComponent解除後もRTを保持し、通常終了のFinalize呼出がない。後者はTextureの借用handleを明示リセットしない | 高。Reflection/WaterとGPU fence・descriptor寿命を合わせて検証する。単純なFinalize追加では完了としない |
| PostEffectRenderTargetManager | FinalizeはRTV/SRV/DSV indexを返却しない。heap終了時には解消するが、同じheapでの再初期化には不十分 | 高。画面RT・Scene Depth・resizeとの整合が必要。今回は変更しない |
| Editorの借用とasync | WindowManagerのSceneManagerは終了時null化されるが、LevelService/SceneDeferredの借用は別。AssetBuildのfutureはFinalizeEditorServicesでjoinされない | 中〜高。Editor全体の終了契約を定めてから通常所有へ移す |
| VFXの複数runtime | CueにはFinalizeがある一方、Graph/ParticleEffect/旧Effectには全体終了の対称APIがない | 高。Loop停止、preview、backend再起動の互換性確認が必要。VFX動作を変えない |
| 通常所有の小規模候補 | AssetRegistry、diagnostics、Editor panel、Forward bridge等は既存の親へ所有を寄せられる | 低〜高（個別記載）。今回の終了漏れ修正より効果が小さく、移行はしない |
| Legacy候補 | LevelObjectManagerに現行Engine/Applicationの利用箇所が見つからない | 中。公開ヘッダー・project参照・Legacy衝突との関係を確認後に別途判断。今回は削除なし |

### 責務と変更しない契約

EngineはDevice/音声/Job/Assetと再利用できるWorld機構を提供し、Application/GameplayはScene選択・Actor/Componentによるゲーム内容を所有する。Editorは編集状態・履歴・previewを扱い、Gameplay本体を所有する新Managerは作らない。RuntimeからEditorへの直接依存は既存課題として残す。

ActorWorld/SceneManager、SystemScheduler、Framework::Update/Run、GameApplication::Update/Draw、Forward bucket順、Water/VFX/Physicsの処理は変更対象にしない。GPU関連の解放順の全面変更、Singleton一括変換、新たなServiceLocatorの導入もしない。

主なI/F呼出元: [Framework.cpp](../Project/Engine/Core/Application/Framework.cpp)、[GameApplication.cpp](../Project/ApplicationLayer/GameApplication.cpp)、[DirectXCommon.cpp](../Project/Engine/Graphics/Device/Facade/DirectXCommon.cpp)、[ActorWorld.cpp](../Project/Engine/Scene/Actor/Core/ActorWorld.cpp)、[ActorWorld_Draw.cpp](../Project/Engine/Scene/Actor/Core/ActorWorld_Draw.cpp)、[SceneManager.cpp](../Project/Engine/Scene/Management/SceneManager.cpp)。各型の宣言リンクから隣接cpp/inlも照合した。

## Core / Platform（8型）

### FrameMemory

**維持**。リスク: 中。効果: Frame単位のscratch所有を維持し、個別Sceneへの分散を避ける。[宣言](../Project/Engine/Core/Memory/FrameMemory.h)

- 所有者 / I/F: static本体がbuffer・overflow blockを所有。FrameworkがI/F、デストラクタもF。
- Lazy / 再初期化: BeginFrame/allocateでも未起動ならI。IはFしてから再確保。既存scratch参照がない時だけ再初期化可。
- 借用 / Thread: upstream memory_resourceを借用し、利用側へscratch参照を渡す。Main、allocator自体の並列利用保証なし。
- Scene跨ぎ / Singleton必要性: Frame基盤として跨ぐ。単一Frame allocator共有は有用だが、Framework通常所有でも表現可能。

### GameTimer

**維持**。リスク: 中。効果: 共通clockと計測区間を維持する。[宣言](../Project/Engine/Core/Time/Core/GameTimer.h)

- 所有者 / I/F: static本体が時刻・統計を所有。GameApplicationのBeginFrameからI。公開Fの外部呼出はなし。
- Lazy / 再初期化: BeginFrameで遅延I。F後のBeginFrameで起動できるが、targetFPS等の設定継承はcold起動と別。
- 借用 / Thread: 保存する外部ポインタなし。Main、同期なし。
- Scene跨ぎ / Singleton必要性: clockは跨ぐ。共通時刻は必要だがglobalアクセス自体は必須ではない。Update順と一緒に扱う。

### JobSystem

**維持**。リスク: 高。効果: worker poolの所有とjoinを一か所に保つ。[宣言](../Project/Engine/Core/Concurrency/JobSystem.h)

- 所有者 / I/F: static本体がjthread・queue・JobStateを所有。FrameworkがI/F、StreamingManager/SystemScheduler側にも起動経路がある。
- Lazy / 再初期化: 利用時の未起動確認からI。起動済みIは抑止。Fは停止・join・queue整理を行い、producer停止後なら再起動経路あり。
- 借用 / Thread: Job callbackが外部状態をcaptureし得る。Mainとworker、mutex/CV/atomicを使用。callbackの対象寿命は呼出側責任。
- Scene跨ぎ / Singleton必要性: 共通poolは跨ぐ。単一pool運用を維持。単なるSceneメンバー化はしない。

### ProjectSettings

**維持**。リスク: 低。効果: Project全体のpath設定を共通化したまま保つ。[宣言](../Project/Engine/Core/Project/ProjectSettings.h)

- 所有者 / I/F: static本体が設定値を所有。利用側のEnsureLoaded/Loadで読み込み、Fなし。
- Lazy / 再初期化: 設定の遅延読み込みあり。Loadで再読込可能だが、関連Asset cacheの一括再初期化ではない。
- 借用 / Thread: 外部ポインタ保持なし。現在Main、Loadと参照の並列同期なし。
- Scene跨ぎ / Singleton必要性: Project設定は跨ぐ。読み取り共有が目的で、Singletonそのものは必須ではない。

### ReliabilityTelemetry

**通常所有へ変更候補**。リスク: 低〜中。効果: FrameAllocationTracker/Frameworkの計測sessionに出力file寿命を合わせられる。[宣言](../Project/Engine/Core/Diagnostics/ReliabilityTelemetry.h)

- 所有者 / I/F: static本体がofstream・集計値を所有。RecordCurrentFrameが環境変数からI、FrameAllocationTrackerとデストラクタがF。
- Lazy / 再初期化: 初回記録時にI。InitializeFromEnvironmentは先にF。記録を止めてから出力先を再設定できる。
- 借用 / Thread: 他サービスは記録時だけ参照、保存する外部ポインタなし。Main、streamへの並列書込同期なし。
- Scene跨ぎ / Singleton必要性: 計測sessionは跨ぐ。global singletonである必要はなく既存計測ownerへの内包候補。

### ResolutionManager

**通常所有へ変更候補**。リスク: 低〜中。効果: Windowのsizeと座標変換の所有を対応させられる。[宣言](../Project/Engine/Platform/Windows/ResolutionManager.h)

- 所有者 / I/F: static本体がsize/aspect等の値を所有。I/Fなし。Frameworkが起動・resize時にSetScreenSize。
- Lazy / 再初期化: 資源Iなし。値の再設定可、Window再生成との同期は呼出側担当。
- 借用 / Thread: 外部ポインタなし。Main、同期なし。
- Scene跨ぎ / Singleton必要性: Windowに従い跨ぐ。単一Window前提のglobal値でありWinApp等の通常所有候補。

### StreamingManager

**維持**。リスク: 高。効果: request取消・worker完了・Main反映の契約を維持する。[宣言](../Project/Engine/Core/Streaming/StreamingManager.h)

- 所有者 / I/F: static本体がrequest state、callback、payload queueを所有。FrameworkがI/F、デストラクタもF。IはJobSystemを起動。
- Lazy / 再初期化: Requestで遅延I。Fは受付停止・cancel・JobSystem::WaitIdle後にqueueを破棄。並行producer停止後の再起動は可能。
- 借用 / Thread: callbackのcaptureに借用があり得る。workerでload、Main Updateでcompletion。mutex/atomicがあり、I/FとRequestの競合を許す契約ではない。
- Scene跨ぎ / Singleton必要性: loaderは跨ぐ。Sceneごとの対象はcancel/世代で無効化する。共有pool接続を維持する。

### WinApp

**維持**。リスク: 高。効果: Window/Message/COMの同一thread寿命を保つ。[宣言](../Project/Engine/Platform/Windows/WinApp.h)

- 所有者 / I/F: static本体がHWND/class登録等を管理。FrameworkがCreateMainWindow/Fを担当し、FでCoUninitialize。
- Lazy / 再初期化: Windowの遅延Iなし。I/Fの対応と同一threadが必要。Fの反復呼出やOS状態を含む再起動は保証しない。
- 借用 / Thread: HWNDを利用側に貸す。native handleは管理対象であって外部Actorの借用ではない。Main/Window message thread。
- Scene跨ぎ / Singleton必要性: Windowは跨ぐ。複数Window対応がない現状は維持するが、global storageは本質的要件ではない。

## Editor（宣言一覧のEditor分類、25型）

### EditorActorStateRegistry

**通常所有へ変更候補**。リスク: 中。効果: Actor編集状態をEditorのWorld寿命へ限定できる。[宣言](../Project/Engine/Editor/EditorActorStateRegistry.h)

- 所有者 / I/F: static mapがvisibility等を所有。I/Fなし。EditorContextのResetTransientState、ActorWorld終了でClear。
- Lazy / 再初期化: 登録時のmap確保のみ。Clear後再登録可、Worldを区別しない一括Clearに注意。
- 借用 / Thread: Actor*をkeyとして借用。Main、同期なし。
- Scene跨ぎ / Singleton必要性: UI設定は共有できるがActor keyは跨がせない。EditorContext/World単位の通常所有候補。Runtimeの参照箇所も同時に整理が必要。

### EditorCommandHistory

**通常所有へ変更候補**。リスク: 高。効果: Undo callbackを編集documentの寿命に閉じ込められる。[宣言](../Project/Engine/Editor/EditorCommandHistory.h)

- 所有者 / I/F: static本体がunique_ptr<IEditorCommand>とtransactionを所有。I/Fなし。Scene/Level/PIE切替、編集操作でClear。
- Lazy / 再初期化: command登録時だけ確保。Clearで履歴・transactionを再開できる。
- 借用 / Thread: command内functionがActor/Component等をcaptureし得る。Main/UI専用、同期なし。
- Scene跨ぎ / Singleton必要性: 旧Sceneの履歴は跨がせない。Editor session/document所有候補だが全Clear経路とUndoの互換性確認が先。

### EditorContentBrowserPanel

**通常所有へ変更候補**。リスク: 低〜中。効果: panelのregistryと閲覧状態をEditorShellへ所有させられる。[宣言](../Project/Engine/Editor/EditorContentBrowserPanel.h)

- 所有者 / I/F: static本体がasset registry・path履歴を所有。Draw等のInitializeIfNeededがI、明示Fなし。
- Lazy / 再初期化: 初回表示時にregistry I。initialized_を戻す全体Fがなく、再走査と本体再起動は別。
- 借用 / Thread: 可視entryのpointerはregistryの借用、Scene/selectionは操作時に参照。Main/UI、同期なし。
- Scene跨ぎ / Singleton必要性: browserの閲覧状態は跨ぐ。Shell通常所有で足りるが、registry更新中の参照保持に注意。

### EditorContext

**通常所有へ変更候補**。リスク: 高。効果: selectionと編集document情報のrootを明確化できる。[宣言](../Project/Engine/Editor/EditorContext.h)

- 所有者 / I/F: static本体がselection・placement・Level名/dirtyを所有。I/Fなし。Scene/Level/PIEがResetTransientState。
- Lazy / 再初期化: 資源Iなし。transient resetはあるが、すべての編集設定をcold状態へ戻すAPIではない。
- 借用 / Thread: selection内EditorObjectInfoのActor/Component pointer・編集callbackを借用。Main/UI、同期なし。
- Scene跨ぎ / Singleton必要性: UI rootは跨ぐがselectionは解除必須。Editor通常所有候補。既存Runtime直接参照とWindowManagerの参照メンバーの移行が必要。

### EditorDiagnosticsPanel

**通常所有へ変更候補**。リスク: 低。効果: UI filter/profiler履歴をEditorに閉じ込められる。[宣言](../Project/Engine/Editor/EditorDiagnosticsPanel.h)

- 所有者 / I/F: static本体がUI状態とprofilerを所有。I/Fなし、EditorLevelOverlayからDraw。履歴はUIでReset。
- Lazy / 再初期化: 描画・sample時に履歴が増える。全体resetなし、個別の履歴resetのみ。
- 借用 / Thread: EditorOutputLog*を保持。Main/UI、log取得はlog側のlock付きsnapshot。
- Scene跨ぎ / Singleton必要性: 診断履歴は跨ぐと有用。WindowManager/Shellの通常所有でよい。

### EditorGpuPickingManager

**通常所有へ変更候補**。リスク: 中〜高。効果: Editor viewportのGPU picking寿命をownerに合わせられる。[宣言](../Project/Engine/Editor/EditorGpuPickingManager.h)

- 所有者 / I/F: static本体がObjectId/depth/readback資源・descriptorを所有。GameApplicationがI/F、ObjectIdPipelineのI/Fもここから。
- Lazy / 再初期化: explicit Iに加え利用側の初期化確認あり。Fでpendingと資源を解除、再IはGPU完了・heap生存が前提。部分生成失敗のcleanupは要確認。
- 借用 / Thread: DirectXCommon*、picking時のScene/EditorObjectInfoを借用。Mainで記録・readback、独立CPU workerなし。
- Scene跨ぎ / Singleton必要性: GPU bufferは再利用できるがpick要求は旧Sceneへ残せない。Editor viewport通常所有候補。

### EditorHierarchyPanel

**通常所有へ変更候補**。リスク: 中。効果: Outliner/Inspectorの一時参照をEditor sessionへ限定できる。[宣言](../Project/Engine/Editor/EditorHierarchyPanel.h)

- 所有者 / I/F: static本体がobjects_とInspector編集状態を所有。I/Fなし、EditorShell::Drawで利用。
- Lazy / 再初期化: 描画時にobject一覧を収集。独立した全体resetなし。
- 借用 / Thread: EditorObjectInfoのActor/Component・編集callbackを保持する。Main/UI、同期なし。
- Scene跨ぎ / Singleton必要性: panel設定は跨ぐがobject一覧は現在World専用。Shell所有候補、編集中のScene破棄を検証する。

### EditorLevelDeferredController

**統合候補**。リスク: 高。効果: Level/Sceneの遅延要求と確認状態の担当を一つにできる。[宣言](../Project/Engine/Editor/EditorLevelDeferredController.h)

- 所有者 / I/F: static本体がpending操作・path・dialog状態を所有。I/Fなし。EditorModeController::UpdateのProcessSafePointで実行。
- Lazy / 再初期化: 要求時に値を保持。個別request消費はあるが全体F/resetなし。
- 借用 / Thread: LevelService等を実行時に参照し、永続Scene pointerは持たない。Main/UIの予約→Update safe point。
- Scene跨ぎ / Singleton必要性: 要求は遷移を跨ぐが永続globalでなくてもよい。SceneDeferredとの統合候補。GPU完了後の実行地点を動かさない。

### EditorLevelService

**通常所有へ変更候補**。リスク: 高。効果: Level保存/読込と借用SceneManagerの終了をEditor側へ集約できる。[宣言](../Project/Engine/Editor/EditorLevelService.h)

- 所有者 / I/F: static本体がLevel path・recent・autosave状態を所有。EnsureInitializedで一覧等を読み込む。Fなし、OverlayがSetSceneManager。
- Lazy / 再初期化: 初回利用時にI。再scan可能だがinitialized_等を含む全体再起動契約はない。
- 借用 / Thread: SceneManager*を保持。WindowManagerとは別の借用で、通常終了時の明示null化は見つからない。Main/UIと同期file I/O。
- Scene跨ぎ / Singleton必要性: Levelサービスは跨ぐがSceneManagerの寿命内に制限すべき。Editor owner候補、autosaveとpending dialogの互換性が必要。

### EditorModeController

**今回判断保留**。リスク: 高。効果: Runtimeへ漏れたEditor mode判定の境界整理が必要。[宣言](../Project/Engine/Editor/EditorModeController.h)

- 所有者 / I/F: static本体がmode値を所有。GameApplicationがI、Fなし。I/mode変更はInput/Camera/履歴等へ副作用を持つ。
- Lazy / 再初期化: 資源Iなし。Iでビルド別既定modeを再適用するが、Editor全体をresetするわけではない。
- 借用 / Thread: 保存する外部pointerなし。Main、他サービスを直接操作。
- Scene跨ぎ / Singleton必要性: modeは跨ぐ。SceneManager/描画/衝突も参照するため、単なるEditor内包では逆依存が残る。方針を保留。

### EditorOutputLog

**統合候補**。リスク: 中。効果: singleton窓口とWindowManagerの通常メンバーが同じstatic storeを使う二重表現を整理できる。[宣言](../Project/Engine/Editor/EditorOutputLog.h)

- 所有者 / I/F: GetInstance本体とWindowManager::outputLog_が存在するがentries/issues/mutexはstatic共有。I/Fなし。UIからClear/ClearIssues。
- Lazy / 再初期化: GetInstance以外でも共有storeへ書ける。Clearはserial等を全resetしない。再利用可能だが新しいlog sessionとは別。
- 借用 / Thread: 外部object pointer保持なし。EditorAssetBuildのworkerとUIが利用し、storeはmutex・snapshotで保護。
- Scene跨ぎ / Singleton必要性: log履歴は跨ぐ。共有storeは必要、二種類のインスタンス表現は必須でない。worker終了とstatic破棄順を先に確認する。

### EditorPlayController

**今回判断保留**。リスク: 高。効果: PIE状態とInput captureの責務を明確化する余地がある。[宣言](../Project/Engine/Editor/EditorPlayController.h)

- 所有者 / I/F: static本体がplay/input状態と要求を所有。I/Fなし、SceneManagerが要求を処理しCommitStopped等で遷移。
- Lazy / 再初期化: 資源Iなし。停止・入力返却はあるがEngine全体再起動のreset契約はない。
- 借用 / Thread: 保存する外部pointerなし。Main/UI、Input/履歴への副作用あり。
- Scene跨ぎ / Singleton必要性: Scene再生成を含むPIEでは跨ぐ。Runtime更新条件からも参照されるため、Editor rootへ移す前に境界整理が必要。

### EditorPlaySessionManager

**通常所有へ変更候補**。リスク: 高。効果: PIE snapshotの寿命をSceneManagerのplay sessionへ対応させられる。[宣言](../Project/Engine/Editor/EditorPlaySessionManager.h)

- 所有者 / I/F: static本体がActor JSON・Camera/Light等のsnapshotを所有。SceneManagerがBegin/EndPlaySession、終了/遷移でCancelSessionWithoutRestore。
- Lazy / 再初期化: Begin時にsnapshot確保。End/Cancel後に新session可。失敗時は既存World維持のtransaction経路を使う。
- 借用 / Thread: BaseScene/ActorWorldは操作中だけ借用、snapshotは値中心。Main、同期file I/Oあり。
- Scene跨ぎ / Singleton必要性: Editor→Runtime→復元の間だけ必要。SceneManagerと寿命を揃える候補だが復元・Keep Changesの検証が必要。

### EditorProfilerPanel

**通常所有へ変更候補**。リスク: 低。効果: 表示設定をEditorの通常panelへまとめられる。[宣言](../Project/Engine/Editor/EditorProfilerPanel.h)

- 所有者 / I/F: static本体がpanel状態を所有。I/Fなし、EditorLevelOverlayがDraw。
- Lazy / 再初期化: 資源Iなし、全体resetなし。表示ごとに現在の統計を読む。
- 借用 / Thread: 引数のSceneManager*、PerformanceMonitor*はDraw中だけ借用。Main/UI、各サービスの統計snapshot契約に従う。
- Scene跨ぎ / Singleton必要性: 表示設定は跨ぐがsingletonは不要。Shell/WindowManager所有候補。

### EditorSceneDeferredController

**統合候補**。リスク: 高。効果: LevelDeferredとScene遷移要求の寿命・safe pointを統一できる。[宣言](../Project/Engine/Editor/EditorSceneDeferredController.h)

- 所有者 / I/F: static本体がstate・pending Scene/Level情報を所有。I/Fなし。OverlayからSetSceneManager/Update、LevelDeferredへ要求。
- Lazy / 再初期化: 資源Iなし。個別state遷移はあるが全体resetなし。
- 借用 / Thread: SceneManager*を保持し、終了時null化の直接呼出は見つからない。Main/UI、Scene切替自体は既存遅延経路。
- Scene跨ぎ / Singleton必要性: 要求は遷移を跨ぐ。LevelDeferredとの統合候補だがSceneManager破棄と未消費requestの契約が先。

### EditorSelectionOutlineManager

**通常所有へ変更候補**。リスク: 中〜高。効果: viewport用RT/descriptorをEditor ownerへ対応させられる。[宣言](../Project/Engine/Editor/EditorSelectionOutlineManager.h)

- 所有者 / I/F: static本体がmask/depth/outline textureとpipelineを所有。GameApplicationがI/F。
- Lazy / 再初期化: 利用時のI確認もある。Iは起動済みを抑止し部分失敗時F。F後再IはGPU/heap条件付き。
- 借用 / Thread: DirectXCommon*を保持、選択objectは描画時参照。MainでGPU記録、CPU workerなし。
- Scene跨ぎ / Singleton必要性: GPU資源はviewportで再利用、選択内容は跨がせない。Editor owner候補、pickingのpipelineとの共有も確認する。

### EditorShell

**通常所有へ変更候補**。リスク: 中。効果: panel群と入力操作状態の寿命をEditor入口へ揃えられる。[宣言](../Project/Engine/Editor/EditorShell.h)

- 所有者 / I/F: static本体が表示・配置・navigation UI状態を所有。I/Fなし。ImGuiManagerがDraw、GameApplicationがOverlayを描く。
- Lazy / 再初期化: 資源Iなし。cursor解放経路はあるがShell全体resetなし。
- 借用 / Thread: 他サービス/Camera/viewportは操作時借用。Main/UI、ImGui contextの生存が前提。
- Scene跨ぎ / Singleton必要性: shellは跨ぐ。ApplicationのEditor部分または既存WindowManager所有候補。Drawの順は維持する。

### EditorTransformGizmo

**通常所有へ変更候補**。リスク: 中〜高。効果: 編集途中のtarget/callbackをselectionの寿命へ限定できる。[宣言](../Project/Engine/Editor/EditorTransformGizmo.h)

- 所有者 / I/F: static本体がtransform command状態を所有。I/Fなし。ShellのDrawでBegin/EndTransformCommand。
- Lazy / 再初期化: 資源Iなし。command終了はあるが全体resetなし。
- 借用 / Thread: transformCommandTarget_のEditorObjectInfoが対象pointer/callbackを保持。Main/UI。
- Scene跨ぎ / Singleton必要性: tool自体は跨ぐが編集中targetは跨がせない。EditorContext/Shell所有候補。ドラッグ中のScene切替検証が必要。

### EditorViewportController

**通常所有へ変更候補**。リスク: 低〜中。効果: 表示mode・gizmo設定をviewportへ所有させられる。[宣言](../Project/Engine/Editor/EditorViewportController.h)

- 所有者 / I/F: static本体がmode・操作設定値を所有。I/Fなし、Shell/Gizmo等がsetterで変更。
- Lazy / 再初期化: 資源Iなし。個別設定を戻せるが一括resetなし。
- 借用 / Thread: 保存する外部pointerなし。Main/UI、同期なし。
- Scene跨ぎ / Singleton必要性: 表示設定は跨ぐ。単一viewportの通常メンバーで足りる。

### EditorWindowManager

**通常所有へ変更候補**。リスク: 高。効果: Editor service・preview・build workerの終了担当を明確にできる。[宣言](../Project/Engine/Editor/EditorWindowManager.h)

- 所有者 / I/F: static本体がBrowser/TexturePreview/AssetBuild/Monitor等を値所有。Draw/AddOutputLogからInitializeEditorServices、FrameworkからFinalizeEditorServices。GameApplicationがSceneManager設定/解除。
- Lazy / 再初期化: Editor serviceは遅延I。Fはpreview cacheをClearするがAssetBuildのfuture停止/joinや全UI resetではない。build稼働中の再Iは保証しない。
- 借用 / Thread: SceneManager*、EditorContextのselection参照を保持。UIはMain、子AssetBuildはstd::asyncでthis/logをcaptureする。
- Scene跨ぎ / Singleton必要性: Editor sessionは跨ぐ。通常owner候補だが、子workerの終了とlog/static破棄順を先に確立する。

### GpuFluidDiagnosticsPanel

**通常所有へ変更候補**。リスク: 低。効果: Fluid/SPHのUI設定をEditorへ限定する。[宣言](../Project/Engine/Editor/GpuFluidDiagnosticsPanel.h)

- 所有者 / I/F: static本体がUIのgrid入力値等を所有。I/Fなし、EditorLevelOverlayからDraw。
- Lazy / 再初期化: 初回表示で編集値を同期。runtime resetとpanelの値resetは別、全体Fなし。
- 借用 / Thread: Fluid/SPH ManagerはDraw中のみ借用。Main/UI、GPU資源は所有しない。
- Scene跨ぎ / Singleton必要性: UI設定以外に跨ぐ必要なし。Editor panelとして通常所有候補。simulation更新を移さない。

### GpuSphAdvancedDiagnosticsPanel

**通常所有へ変更候補**。リスク: 低。効果: SPH設定UIのglobalアクセスを減らせる。[宣言](../Project/Engine/Editor/GpuSphAdvancedDiagnosticsPanel.h)

- 所有者 / I/F: static panel、I/Fなし、EditorLevelOverlayがDraw。
- Lazy / 再初期化: 資源Iなし。runtimeの初期化済み状態を確認して表示する。
- 借用 / Thread: GpuSphManagerはDraw時借用。Main/UI、GPU dispatchの担当ではない。
- Scene跨ぎ / Singleton必要性: panelをEditor sessionに保持すれば十分。Shell/WindowManager所有候補。

### GpuSphRigidbodyInteractionDiagnosticsPanel

**通常所有へ変更候補**。リスク: 低。効果: SPH/剛体診断のUI寿命をEditorへ限定する。[宣言](../Project/Engine/Editor/GpuSphRigidbodyInteractionDiagnosticsPanel.h)

- 所有者 / I/F: static panel、I/Fなし、EditorLevelOverlayがDraw。
- Lazy / 再初期化: 資源Iなし。Interactionの現在値を表示・編集する。
- 借用 / Thread: InteractionはDraw中借用。Main/UI、readback bufferは所有しない。
- Scene跨ぎ / Singleton必要性: UI以外の継続性は不要。既存Editor ownerの通常panel候補。

### GpuVolumetricFluidDiagnosticsPanel

**通常所有へ変更候補**。リスク: 低。効果: Volume UI状態をEditorへ限定する。[宣言](../Project/Engine/Editor/GpuVolumetricFluidDiagnosticsPanel.h)

- 所有者 / I/F: static本体がgrid編集値等を所有。I/Fなし、EditorLevelOverlayがDraw。
- Lazy / 再初期化: 編集値の初回同期あり。runtimeを有効化する操作はあるがpanelはGPUを初期化・所有しない。全体resetなし。
- 借用 / Thread: Volumetric Manager/Depth統計をDraw時借用。Main/UI。
- Scene跨ぎ / Singleton必要性: 編集設定以外の跨ぎは不要。Editor通常所有候補、default-OFFのruntime契約を保つ。

### VfxTimelineEditor

**通常所有へ変更候補**。リスク: 中。効果: preview cueの停止をEditor終了に対応させる。[宣言](../Project/Engine/Vfx/Editor/VfxTimelineEditor.h)

- 所有者 / I/F: static本体が編集cue/path/preview handleを所有。GameApplicationのEditor経路がI/F、FでStopPreview。
- Lazy / 再初期化: Iでfile読込、表示側にもI経路。Fはinitializedを戻すが編集値の全cold resetではない。再IはCue runtime生存が前提。
- 借用 / Thread: Cue再生handleを保持、Actorを所有しない。Main/UI、同期file I/O。
- Scene跨ぎ / Singleton必要性: 編集documentは跨げるがpreviewは停止必須。Editor ownerの通常所有候補。

## Runtime（宣言一覧のRuntime分類、61型）

### AnimationPipelineBuilder

**維持**。リスク: 高。効果: animation用PSOと既存描画契約を保つ。[宣言](../Project/Engine/Graphics/Renderer/Animation/Pipeline/AnimationPipelineBuilder.h)

- 所有者 / I/F: static本体がgraphics/compute PSO・root signatureを所有。FrameworkがI/F。内部からLightManagerのI/Fも行う。
- Lazy / 再初期化: explicit I。起動済みIの抑止なし。F後の再IはGPU条件付きで、LightManagerの二重担当も考慮が必要。
- 借用 / Thread: DirectXCommon*、scope利用側へのbuilder参照。Main、GPU命令記録に利用。
- Scene跨ぎ / Singleton必要性: PSO共有は跨ぐ。renderer所有でも可能だがLight共有とbackend依存が大きいため維持。

### AssetRegistry

**通常所有へ変更候補**。リスク: 低〜中。効果: 登録表の寿命を唯一の直接利用元AssetSystemに合わせられる。[宣言](../Project/Engine/Graphics/Resource/Asset/AssetRegistry.h)

- 所有者 / I/F: static本体がrecord/ID mapを所有。I/Fなし、AssetSystem::FinalizeがClear。
- Lazy / 再初期化: 登録時にCPU確保。Clear後再利用可能、nextAssetIdは保持し古いIDの再利用を避ける。
- 借用 / Thread: Asset metadata/handle中心でGPU pointerを所有しない。mapはmutexで保護、利用元は現在AssetSystem。
- Scene跨ぎ / Singleton必要性: Asset登録は跨ぐ。AssetSystemの値/unique_ptr所有で表現でき、global singletonは不要。

### AssetSystem

**維持**。リスク: 高。効果: async CPU load→MainでGPU反映→遅延解放の橋渡しを保つ。[宣言](../Project/Engine/Graphics/Resource/Asset/AssetSystem.h)

- 所有者 / I/F: static本体がfuture/CPU payloadを所有。FrameworkがI/F。IはFを呼び、RegistryとGpuDeferredReleaseQueueの寿命も管理。
- Lazy / 再初期化: explicit I、load要求は必要時。Fはpending asyncを完了させ、Registry/queueを整理。再Iはproducer停止・Device/heap生存が前提。
- 借用 / Thread: DirectXCommon*を保持。Mainでcache/GPU反映、std::asyncでCPU読込。公開API全体の並列同期はない。
- Scene跨ぎ / Singleton必要性: Asset sessionは跨ぐ。現在はTexture/Model直接利用も残るため、未使用として消したり一括統合しない。

### AudioManager

**維持**。リスク: 所有変更は中、今回の終了呼出追加は低。効果: 既存再生を保ち、終了担当だけを明確化する。[宣言](../Project/Engine/System/Audio/Manager/AudioManager.h) / [実装](../Project/Engine/System/Audio/Manager/AudioManager.cpp)

- 所有者 / I/F: static本体がXAudio2、master/source Voice、PCM clipを所有。Iは再生APIから。基準版ではFの外部呼出なし・デストラクタはdefault。今回Frameworkの通常終了へFを接続する。
- Lazy / 再初期化: EnsureInitializedによる遅延I、既存Iは重複を抑止。FはVoice→clip→master→XAudio2→MFを終了し、未起動Fはno-op。再I経路はあるが旧AudioHandleの継続使用は不可。
- 借用 / Thread: Voice raw pointerは所有、再生bufferはshared clipで保持。利用側はAudioHandle。Mainからの再生とXAudio2内部thread。mutexはあるがEnsureInitializedの非lock読取・StopBGM等を含むAPI全体の並列安全は保証しない。
- Scene跨ぎ / Singleton必要性: BGM/カテゴリ設定の継続には有用。共有audio serviceは維持し、Scene/Component終了後、Job停止後、WinAppのCOM終了前に明示終了する。

### BladeTrailRenderer

**維持**。リスク: 中〜高。効果: Component間の共有pipelineと既存Acquire/Release契約を維持する。[宣言](../Project/Engine/Graphics/Renderer/BladeTrail/BladeTrailRenderer.h)

- 所有者 / I/F: static本体がPSO/root signatureを所有。BladeTrailComponentのAcquireがI、最後のReleaseがF。
- Lazy / 再初期化: 最初のAcquireで遅延I、referenceCountでF。再Acquire可能だがGPUが旧PSOを使い終える時点の保証は別途必要。
- 借用 / Thread: DirectXCommon*を保持。Main、refcountはatomicでない。
- Scene跨ぎ / Singleton必要性: 使用Component間の共有で足り、恒久的に跨ぐ必要はない。既存共有方式を保ち、GPU解放を伴う所有移行は保留する。

### BlendStateFactory

**統合候補**。リスク: 中。効果: blend記述とpipeline生成方針を既存RenderState/PipelineFactory側へまとめられる。[宣言](../Project/Engine/Graphics/RenderState/Blend/BlendStateFactory.h)

- 所有者 / I/F: static本体がblend description配列・custom mapを所有。FrameworkがI/F。Fはcustom mapをClear。
- Lazy / 再初期化: explicit I、GPU資源なし。Iの再実行だけではcustom mapのcold resetにならず、Fとの組合せが必要。
- 借用 / Thread: 外部pointer保持なし。MainのPSO構築から利用、同期なし。
- Scene跨ぎ / Singleton必要性: blend定義は跨いで共有できるがmutable singletonは必須ではない。custom blend互換性を確認後に統合候補。

### CameraManager

**維持**。リスク: 高。効果: main/debug/一時capture viewと音声listenerの共通選択を維持する。[宣言](../Project/Engine/Graphics/Camera/Manager/CameraManager.h)

- 所有者 / I/F: static本体がview override等の値を所有。FrameworkがI/F、defaultCameraはFramework、ComponentのCameraは各Componentが所有。
- Lazy / 再初期化: explicit I。IだけではmainCameraを解除しない。Fでmain/debug pointer・overrideを解除し、再I後にCamera再登録が必要。
- 借用 / Thread: main Camera*とDebugCamera*を保持。Main、capture scopeも同じthreadのstack契約。
- Scene跨ぎ / Singleton必要性: active viewサービスは跨ぐがScene Camera借用は解除が必要。描画・入力・audio依存が広いため維持。

### CullingDiagnostics

**通常所有へ変更候補**。リスク: 低〜中。効果: culling/draw統計をrender passの寿命に揃える。[宣言](../Project/Engine/Graphics/Culling/CullingDiagnostics.h)

- 所有者 / I/F: static本体がcounterを所有。I/Fなし、Object3DCommonのBeginMainPassでframe統計をreset。
- Lazy / 再初期化: GPU Iなし、BeginMainPassで再利用可能。累積/表示値の全cold resetとは別。
- 借用 / Thread: 保存する外部pointerなし。Main render thread、counter更新に並列同期なし。
- Scene跨ぎ / Singleton必要性: 直近frame統計以外の跨ぎは不要。Object3DCommon/render contextの通常所有候補。

### DSVManager

**維持**。リスク: 高。効果: device全体のDSV allocatorとresource所有の分担を保つ。[宣言](../Project/Engine/Graphics/Descriptor/DSV/DSVManager.h)

- 所有者 / I/F: static本体がdescriptor heap/free indexを所有。DirectXCommonがI/F。生成したdepth resourceのComPtr所有は呼出元へ渡す。
- Lazy / 再初期化: explicit I、重複Iの抑止なし。Fはheap/queue/indexをreset。再Iは全利用者が旧indexを返却しGPU完了した後のみ。
- 借用 / Thread: DirectXCommon*を保持、外部resourceをdescriptor作成時に借用。割当はmutex、command利用・I/FはMainで直列化。
- Scene跨ぎ / Singleton必要性: device heapは跨ぐ。DirectXCommon所有にもできるが呼出・heap寿命の変更リスクが大きく維持。

### DebugCamera

**通常所有へ変更候補**。リスク: 中。効果: debug viewの所有を既存CameraManager/Editor側へ明示できる。[宣言](../Project/Engine/Graphics/Camera/DebugCamera/DebugCamera.h)

- 所有者 / I/F: static本体がWorldTransformとview/projectionを値所有。WorldTransform内部はper-frame GPU upload bufferを所有。FrameworkがI/F。
- Lazy / 再初期化: explicit IでWorldTransformのbufferも生成、Fで状態を戻す。再IはGPU完了とCameraManagerへの登録を合わせる必要がある。
- 借用 / Thread: 入力・viewportはUpdate時に借用、内部mapped pointerは自所有bufferのview。利用側CameraManagerが本体を借用。Main/UI。
- Scene跨ぎ / Singleton必要性: Editor navigationは跨ぐと便利。global singletonである必要はなく通常所有候補。

### DirectXCommon

**維持**。リスク: 高。効果: device・command・fence・swapchainのroot寿命を保つ。[宣言](../Project/Engine/Graphics/Device/Facade/DirectXCommon.h)

- 所有者 / I/F: static facadeがdevice/swapchain/compiler/command/fence/RTをunique_ptr等で所有。FrameworkがI/F、RTV/DSV/SRVのI/Fはここから。
- Lazy / 再初期化: explicit I、live状態での二重Iは契約なし。FはGPU待機してdeviceを終了。再Iは全外部サービスの解除・再登録を含むため未保証。
- 借用 / Thread: WinAppはIの引数として借用、GetDevice等は所有COM資源をrawで貸す。現在Main command recording、workerからの全面利用保証なし。
- Scene跨ぎ / Singleton必要性: 一つのdeviceは跨ぐ。global storageは必須ではないが広範な参照元があり、今回root所有を変更しない。

### EffectSystem

**統合候補**。リスク: 高。効果: 旧effect名→emitter経路とGpuParticleEffectRuntimeの重複を減らす余地。[宣言](../Project/Engine/Graphics/Renderer/GpuParticle/Manager/GpuParticleManager.h)

- 所有者 / I/F: GpuParticleManagerと同じfileの別static本体。definition/loop/handle mapを所有。constructorで既定effect登録、全体Fなし。
- Lazy / 再初期化: 初回GetInstanceで既定登録、emitterは再生時準備。Stop系はあるがbackend F後の全map reset契約はない。
- 借用 / Thread: backend emitter名/handleを保持、実GPU資源はGpuParticleManager所有。Main、同期なし。
- Scene跨ぎ / Singleton必要性: 定義は共有可能だがloopは対象寿命に依存。同cppのImGuiから利用されており「外部利用0」だけで削除しない。既定effectの互換性確認後に統合候補。

### EnvironmentMapManager

**今回判断保留**。リスク: 高。効果: Scene環境設定とdevice handleの寿命を切り分ける必要がある。[宣言](../Project/Engine/Graphics/Renderer/Environment/EnvironmentMapManager.h)

- 所有者 / I/F: static本体がpath/revision/override stackを所有。Texture実体はTextureManager所有。I/F/reset APIなし、SkyBoxや明示overrideから設定。
- Lazy / 再初期化: GetEnvironmentMapHandle等でfallback textureを遅延load。handleが非zeroなら再取得しないためTextureManager再起動に自動追従しない。
- 借用 / Thread: TextureManager由来GPU descriptor handleを保持、scopeが本体を借用。Main描画、同期なし。
- Scene跨ぎ / Singleton必要性: fallback共有は可能だがScene固有SkyBox/overrideを持ち越す要否は別。Reflection/Waterとの契約確認まで判断保留。

### ForwardRenderQueue

**通常所有へ変更候補**。リスク: 高。効果: queue payloadを一回のWorld drawへ限定できる。[宣言](../Project/Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h)

- 所有者 / I/F: static本体がbucket/並び順/統計を所有。I/Fなし、ActorWorld::DrawがBeginFrame/EndFrameを担当。
- Lazy / 再初期化: Submit時にCPU確保、次BeginFrameでbucketをClear。EndFrameは統計とactive状態を確定するがpayload配列は次回まで残る。
- 借用 / Thread: void* payloadとdraw callbackを保持し対象Renderer/Componentを所有しない。Main、同期なし。
- Scene跨ぎ / Singleton必要性: payloadはdraw区間だけ必要。render context通常所有候補だがReflectionを含む複数drawとbucket順の検証が先。

### FrameAllocationTracker

**維持**。リスク: 高。効果: process共通CRT hookの設定・復元を一か所に保つ。[宣言](../Project/Engine/DebugTools/Performance/FrameAllocationTracker.h)

- 所有者 / I/F: static本体がcounter/hook登録状態を所有。FrameworkがI/F、FからReliabilityTelemetryを終了する。ReleaseではCRT hookなし。
- Lazy / 再初期化: explicit I、起動済みIは抑止。F後再登録経路はあるが履歴counter等のcold resetは別途確認が必要。
- 借用 / Thread: 以前のCRT hook関数pointerを保存・復元する。hookは任意thread、counterはatomic、I/Fとframe集計はMain。
- Scene跨ぎ / Singleton必要性: process全体のhookで跨ぐ。単一登録の必要性が強く、現方式を維持。

### GameplayAbilityDiagnostics

**通常所有へ変更候補**。リスク: 低。効果: Ability統計をWorld/Gameplay実行sessionへ限定できる。[宣言](../Project/Engine/Gameplay/Diagnostics/GameplayAbilityDiagnostics.h)

- 所有者 / I/F: static本体がstatsを所有。I/Fなし、GameplayAbilityComponentが記録。ResetStatsを公開。
- Lazy / 再初期化: 資源Iなし、ResetStats後再利用可。Scene終了で自動resetされる保証はない。
- 借用 / Thread: 外部pointer保持なし。MainのAbility更新/診断、同期なし。
- Scene跨ぎ / Singleton必要性: 累積診断以外は跨ぐ必要なし。既存World/診断ownerへの通常所有候補。

### GameplayEventRouter

**今回判断保留**。リスク: 中〜高。効果: subscriberの対象寿命と利用範囲を明確化する必要がある。[宣言](../Project/Engine/Gameplay/Events/GameplayEventRouter.h)

- 所有者 / I/F: static本体がsubscriber/function mapを所有。I/Fなし、Subscribe/Unsubscribe/Clear。現行外部参照はAbility診断のGetStatsで、実配送の利用拡大は確認できない。
- Lazy / 再初期化: 資源Iなし。Clear後再登録可能だがID/累積統計は保持。通常終了のClear呼出なし。
- 借用 / Thread: eventはWorldId付きActorHandle、callbackは外部pointerをcapture可能。Publishは同期・callback snapshot配送で、thread-safe queueではない。
- Scene跨ぎ / Singleton必要性: World単位かprocess共有か未確定。登録中に解除しても現在配送には反映しない契約を維持し、不要と即断しない。

### GpuDeferredReleaseQueue

**維持**。リスク: 高。効果: GPU完了までresource/descriptorの寿命を延長する契約を保つ。[宣言](../Project/Engine/Graphics/Resource/Asset/GpuDeferredReleaseQueue.h)

- 所有者 / I/F: static本体がpending ComPtr/shared keep-aliveを所有。AssetSystemがI/F、I/FともWaitAndFlushを使う。
- Lazy / 再初期化: explicit I、Enqueue時にentry確保。再Iは旧fence待機を終え、Device/SRVが生存していることが前提。
- 借用 / Thread: DirectXCommon*/SRVManager*を保持。pending listはmutex、fence/descriptorとI/FはMainで直列化する必要がある。
- Scene跨ぎ / Singleton必要性: Scene破棄後もGPU参照の間は必要。AssetSystem内包も可能だがTexture/Model等の利用と同期を伴うため維持。

### GpuFluidForwardRenderBridge

**通常所有へ変更候補**。リスク: 中〜高。効果: draw packet所有をGpuFluidManagerのrenderer寿命へ合わせる。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Renderer/GpuFluidForwardRenderBridge.h)

- 所有者 / I/F: static本体がdeque<RenderPacket>と統計を所有。I/Fなし、GpuFluidManager::SubmitForwardから使用。
- Lazy / 再初期化: Submitでpacket確保、新queue serialでClear。GPU Iは担当しない。
- 借用 / Thread: packetがrenderer*/grid*を保持し、ForwardRenderQueueへpacket addressを貸す。Main、dequeでframe中のaddressを保持。
- Scene跨ぎ / Singleton必要性: packetは一draw区間だけ必要。Manager通常所有候補だが、capture drawとqueue serialの互換性を検証する。

### GpuFluidManager

**維持**。リスク: 高。効果: 2D grid/pass/rendererと既存fixed-step動作を保つ。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.h)

- 所有者 / I/F: static本体がgrid/pass/rendererを値所有。FrameworkがI/F、ActorWorld::DrawからWorld同期・更新・Submit。
- Lazy / 再初期化: 起動時I。IはFして生成、失敗時もF。reconfigure/reset経路はあるがGPU待機条件を含むため無条件の再Iは不可。
- 借用 / Thread: activeWorld_を借用しemitter/obstacleは値収集。Mainでcompute記録、CPU workerなし。
- Scene跨ぎ / Singleton必要性: backend再利用は跨ぐがWorldのsourceは毎回同期。World単位化は挙動変更を伴うため維持。

### GpuParticleEffectRuntime

**今回判断保留**。リスク: 高。効果: compiled effect/loopとbackend emitterの寿命を対称化する必要。[宣言](../Project/Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h)

- 所有者 / I/F: static本体がcompiled effect・parameter・loop/emitter名mapを所有。Load/Register/Playから構成、全体I/Fなし。
- Lazy / 再初期化: effect/mesh/emitterを利用時に準備。個別StopLoop/reloadはあるがGpuParticleManager終了後の全map reset契約なし。
- 借用 / Thread: emitter名/mesh ID/再生handleをbackendへ関連付け、GPU資源は所有しない。Main、同期なし。
- Scene跨ぎ / Singleton必要性: compiled asset共有は跨ぐがactive loopは別寿命。Graph/Cue/previewとの終了整合を先に確認する。

### GpuParticleForwardRenderBridge

**通常所有へ変更候補**。リスク: 中〜高。効果: 透明/Additive packetを既存Particle rendererへ内包できる。[宣言](../Project/Engine/Graphics/Renderer/GpuParticle/Renderer/GpuParticleForwardRenderBridge.h)

- 所有者 / I/F: static本体が二つのRenderPacketとframe統計を所有。I/Fなし、ActorWorld::DrawがSubmit。
- Lazy / 再初期化: GPU Iなし、Submitでqueue serialとpacketを更新。
- 借用 / Thread: packetのGpuParticleManager*、queueへ貸すpacket address。Main、ScopedDrawPassで既存pass状態を一時変更。
- Scene跨ぎ / Singleton必要性: packetは一drawだけ必要。通常所有候補だが既存GPU sort/indirect drawとbucket順を変えない。

### GpuParticleManager

**維持**。リスク: 高。効果: emitter・pipeline・bufferの共有backendを維持する。[宣言](../Project/Engine/Graphics/Renderer/GpuParticle/Manager/GpuParticleManager.h)

- 所有者 / I/F: static本体がpipeline/buffer/renderer/emitter/mesh assetを所有。FrameworkがI/F、FはParameter登録解除とmap/resource破棄。
- Lazy / 再初期化: backendは起動時I、emitter/meshは必要時。Iの二重呼出防止なし。再IはGPU完了と上位runtimeのhandle停止が前提。
- 借用 / Thread: Frameworkのdefault Camera*、rendererから内部pipeline/bufferを借用。Mainでcompute/graphics、同期なし。
- Scene跨ぎ / Singleton必要性: backend共有は跨ぐ。Actor/Componentの効果所有と分けたまま維持し、上位runtimeを一括書換しない。

### GpuProductionLiquidManager

**維持**。リスク: 高。効果: SPH前後の品質/LOD/Secondary/Ocean制御を維持する。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Liquid/Manager/GpuProductionLiquidManager.h)

- 所有者 / I/F: static本体が設定・統計を所有。FrameworkがI/F、FからOceanCoupler/SecondaryClassifierを終了。
- Lazy / 再初期化: 本体はexplicit I、子GPU機能は遅延。I単独では既存oceanProviderを外さず、F後の再I・provider再登録が必要。
- 借用 / Thread: IGpuProductionLiquidOceanProvider*を借用。WaterSurfaceComponentが登録/解除。Main、SPH前後でGPU処理を指示。
- Scene跨ぎ / Singleton必要性: backend制御は跨ぐがWater providerはScene寿命。更新位置とWater連携を保つため維持。

### GpuProductionLiquidOceanCoupler

**通常所有へ変更候補**。リスク: 高。効果: 唯一の直接利用元ProductionLiquidManagerへGPU passを所有させられる。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Liquid/Manager/GpuProductionLiquidOceanCoupler.h)

- 所有者 / I/F: static本体がPSO/root signatureを所有。UpdateがI、ProductionLiquidManagerがF。
- Lazy / 再初期化: 最初のUpdateでI。IはFして作成し、失敗時F。再IはGPU完了・Device生存が前提。
- 借用 / Thread: DirectXCommon*、Update引数のparticle/ocean samplingデータ。Mainでcompute記録。
- Scene跨ぎ / Singleton必要性: pass cacheは跨げるがsingleton不要。既存Managerの子所有候補、Waterの反作用順を検証後に判断。

### GpuProductionLiquidSecondaryClassifier

**通常所有へ変更候補**。リスク: 高。効果: counter/readbackの寿命をProductionLiquidManagerへ対応させられる。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Liquid/Manager/GpuProductionLiquidSecondaryClassifier.h)

- 所有者 / I/F: static本体がcounter/UAV/PSO/readback slotsを所有。UpdateがI、ProductionLiquidManagerがF。
- Lazy / 再初期化: Updateで遅延I。FはUAVとmapped/readback資源を戻す。再Iはpending GPU利用完了が前提。
- 借用 / Thread: DirectXCommon*と入力particle bufferを借用。Mainでcompute/readback、CPU workerなし。
- Scene跨ぎ / Singleton必要性: shared passは跨げるが唯一の親への内包が可能。readback fenceの検証を伴うため今回は変更しない。

### GpuSphManager

**維持**。リスク: 高。効果: SPH particle/solver/固定刻みとGPU readbackを維持する。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h)

- 所有者 / I/F: static本体がparticle/scratch/hash/DFSPH/PSO/CFL readbackを所有。FrameworkがI/F。particle buffer終了からInteractionのFも呼ばれる。
- Lazy / 再初期化: 起動時I、IはFして再確保。失敗時F。resetは別経路で、再IにはGPU完了と利用者の再接続が必要。
- 借用 / Thread: DirectXCommon*、renderer/interactionへparticle bufferを貸す。Mainのcompute/readback、CPU workerなし。
- Scene跨ぎ / Singleton必要性: 現在一つのsolverを共有して跨ぐ。World別化は挙動変更のため維持。

### GpuSphRigidbodyInteraction

**今回判断保留**。リスク: 高。効果: SPH resourceとPhysics Worldの異なる寿命を明確化する必要。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Sph/Interaction/GpuSphRigidbodyInteraction.h)

- 所有者 / I/F: static本体がproxy upload/reaction/readback slotsとPSOを所有。ReflectionProbeSceneBridgeのUpdateでEnsureInitialized。GpuSphParticleBuffer::FinalizeがFを呼ぶ。
- Lazy / 再初期化: 有効なSPH/particleがあるUpdateで遅延I。frame構成変更時もF→I。再IはGPU完了条件付きで、settings/統計すべてのcold resetではない。
- 借用 / Thread: DirectXCommon*、mapped pointerは自所有bufferのview。ActorWorldはUpdate引数、反作用の対象はWorldId付きActorHandle。MainでGPU記録・readback・Physics反映。
- Scene跨ぎ / Singleton必要性: GPU cacheは共有できるが反作用対象はWorld専用。Physics順序とScene切替時のreadback検証まで判断保留。

### GpuSphScreenSpaceFluidRenderer

**通常所有へ変更候補**。リスク: 高。効果: 専用RT/descriptorを既存PostEffectの寿命へ揃えられる。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Sph/Renderer/GpuSphScreenSpaceFluidRenderer.h)

- 所有者 / I/F: static本体がdepth/thickness/scene-copy/PSOを所有。RenderからI、PostEffectManager::FinalizeがF。
- Lazy / 再初期化: 有効SPHのRender時にI、EnsureSizeでresize。F後再IはGPU/heap生存が前提。
- 借用 / Thread: DirectXCommon*、SPH particle buffer、PostEffectのscene targetを借用。Main graphics、CPU workerなし。
- Scene跨ぎ / Singleton必要性: viewport資源は跨げるがsingletonは不要。PostEffect内包候補、Water composite/Depth/resizeの互換性検証が先。

### GpuVolumetricFluidForwardRenderBridge

**通常所有へ変更候補**。リスク: 中〜高。効果: Volume draw packetを既存Managerへ内包できる。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/GpuVolumetricFluidForwardRenderBridge.h)

- 所有者 / I/F: static本体がdeque packetと統計を所有。I/Fなし、Volumetric ManagerからSubmit。
- Lazy / 再初期化: Submitでpacket確保、queue serial変更でClear。GPU Iなし。
- 借用 / Thread: renderer*/grid*を保持しqueueへpacket addressを貸す。Main、透明bucketでScene Depthを要求する。
- Scene跨ぎ / Singleton必要性: 一draw区間だけ必要。親Manager所有候補、Depth readable-stateとcapture draw契約を保つ。

### GpuVolumetricFluidManager

**維持**。リスク: 高。効果: default-OFF/lazyなTexture3D simulationとVFX連携を保つ。[宣言](../Project/Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/GpuVolumetricFluidManager.h)

- 所有者 / I/F: static本体がgrid/pass/raymarch rendererを所有。runtime有効化後のUpdate等からI、FrameworkがF。
- Lazy / 再初期化: 必要時I、起動済みなら再生成しない。ReleaseRuntimeResources/Fとreconfigure経路あり、再IはGPU条件付き。
- 借用 / Thread: activeWorld_を借用しsourceを値収集。Main compute/graphics、CPU workerなし。
- Scene跨ぎ / Singleton必要性: backend cacheは跨ぐがWorld sourceは同期が必要。VFXによる有効化と停止を含むため維持。

### ImGuiManager

**維持**。リスク: 高。効果: ImGui context/Win32/DX12 backendのI/F対応を保つ。[宣言](../Project/Engine/DebugTools/ImGui/ImGuiManager.h)

- 所有者 / I/F: static本体がcontext/backendとSRV割当記録を管理。FrameworkがI/F、USE_IMGUIなしでは実処理なし。
- Lazy / 再初期化: explicit I、二重Iは例外。Fはbackend/contextとSRVを解放しflagを戻す。再IはWindow/Device/heapの生存が前提。
- 借用 / Thread: backendにDevice/queue/heap/HWNDを渡し、descriptor callbackにthisを貸す。Main/Window message thread、並列ImGui利用不可。
- Scene跨ぎ / Singleton必要性: Editor contextは跨ぐ。一つのcontextの調停は必要で、Window messageとの接続を今回変えない。

### Input

**維持**。リスク: 高。効果: Window入力・cursor captureと現在の更新位置を保つ。[宣言](../Project/Engine/System/Input/Input.h)

- 所有者 / I/F: static本体がDirectInput/keyboard/mouseのComPtrと入力状態を所有。GameApplicationがI、F APIなし、defaultデストラクタ。
- Lazy / 再初期化: explicit I。GetAddressOfでのdevice生成等がありlive状態の二重Iは不可。Window再作成後の再I契約は未整備。
- 借用 / Thread: WinApp*を保持。Main/Window thread、同期なし。
- Scene跨ぎ / Singleton必要性: 入力は跨ぐ。通常所有は可能だがPIE/input policy/Window寿命の影響が広く現状維持。

### JsonEditorWindow

**今回判断保留**。リスク: 中〜高。効果: Runtime material読込とEditor UI/previewの寿命を分離する必要。[宣言](../Project/Engine/System/JsonAssets/JsonEditorWindow.h)

- 所有者 / I/F: static本体がJson registryとunique_ptr<Sprite> previewを所有。GameApplicationがI/Update、Fなし。
- Lazy / 再初期化: Iはasset読込・material登録、previewは選択時生成。I再実行だけでpreview/GPU資源を含む全cold resetにはならない。
- 借用 / Thread: preview Sprite内部にDevice/texture参照がある。本体はregistry/previewを所有。Main/UI・同期file I/O。
- Scene跨ぎ / Singleton必要性: asset編集は跨ぐがGPU previewをdevice終了後まで残す必要はない。ReleaseでもI/Updateされるため単純なEditor移動は保留。

### LightManager

**維持**。リスク: 高。効果: Component由来lightとglobal light、shadowの共通backendを保つ。[宣言](../Project/Engine/Graphics/Lighting/LightManager.h)

- 所有者 / I/F: static本体がLightGpuBuffer/ShadowSystemをunique_ptr所有。Object3DCommonとAnimationPipelineBuilderの両方がI/Fを担当する。
- Lazy / 再初期化: 呼出元のexplicit I。shadowは存在確認、他のI処理は再実行される。Fはparameter解除・buffer/shadow破棄。二重担当を含む再Iは要検証。
- 借用 / Thread: DirectXCommon*、内部LightParameterControllerがthisを借用。Component lightは値収集、Scene Actorを所有しない。Main、同期なし。
- Scene跨ぎ / Singleton必要性: GPU照明基盤は跨ぐがComponent lightはWorld終了で解除。Singletonの維持とI/F担当の二重化は別問題として記録する。

### MaterialRepository

**維持**。リスク: 中。効果: material共有とrevisionによる変更検知を保つ。[宣言](../Project/Engine/Graphics/Material/MaterialRepository.h)

- 所有者 / I/F: static本体がshared_ptr<MaterialAsset> mapを所有。GetInstance内の一回限りの初期化でInitializeDefaults、公開Clearはあるが通常Fなし。
- Lazy / 再初期化: 最初のGetInstanceでdefault登録。Clear後のGetInstanceだけではdefault再登録されない。再構築するならInitializeDefaultsが必要。
- 借用 / Thread: MaterialAssetはshared所有、利用側にもshared handleを返す。Main、map更新の同期なし。
- Scene跨ぎ / Singleton必要性: material assetは跨ぐ。AssetSystem配下にもできるが現行material/Json読込を維持。

### ModelManager

**維持**。リスク: 中〜高。効果: shared Model cacheとCPU loaderを保つ。[宣言](../Project/Engine/Graphics/Resource/Model/ModelManager.h)

- 所有者 / I/F: static本体がshared Model mapを所有。Iなし、Loadで構築、FrameworkがFでmapをClear。
- Lazy / 再初期化: Modelは利用時load。F後reload可だが、利用側のshared Modelが残ればcache ClearだけでGPUが消えるわけではない。
- 借用 / Thread: Modelをshared所有、内部Device参照はModel側。cache操作はMain、static CPU loaderはAssetSystem workerから使う。cache自体は同期なし。
- Scene跨ぎ / Singleton必要性: asset cacheは跨ぐ。AssetSystemとの統合よりshared所有の終了順検証を優先し維持。

### Object3DCommon

**維持**。リスク: 高。効果: static/instanced/shadow pipelineとculling passの共通契約を保つ。[宣言](../Project/Engine/Graphics/Renderer/Object3D/Object3DCommon.h)

- 所有者 / I/F: static本体がpipeline setとculling状態を値所有。FrameworkがI/F。LightManagerのI/Fも担当する。
- Lazy / 再初期化: explicit I、Iの重複抑止なし。F後再IはGPU/Light/Deviceの整合が前提。
- 借用 / Thread: DirectXCommon*、内部pipelineはfactory/compilerを初期化時に利用。Main、render/culling設定変更の並列同期なし。
- Scene跨ぎ / Singleton必要性: render基盤は跨ぐ。renderer context通常所有でも可能だが既存pass順とLight共有を維持。

### ObjectIdPipeline

**通常所有へ変更候補**。リスク: 中〜高。効果: picking pipelineの寿命をEditorGpuPickingManagerに揃えられる。[宣言](../Project/Engine/Graphics/Renderer/Object3D/ObjectIdPipeline.h)

- 所有者 / I/F: static本体がstatic/instanced PipelineBundleを所有。EditorGpuPickingManagerがI/F。
- Lazy / 再初期化: BindStatic/BindInstancedでも未起動ならI。Fでbundle/Device借用/flagをreset、再IはGPU/Device条件付き。
- 借用 / Thread: DirectXCommon*、Bind引数のcommand list。Main、CPU workerなし。
- Scene跨ぎ / Singleton必要性: pickingのGPU cacheは跨げる。Editor owner候補だがObject3D/Animationのruntime側Bind呼出も移行対象。

### ParameterManager

**今回判断保留**。リスク: 高。効果: 設定値と対象objectへのcallbackの寿命を分ける必要。[宣言](../Project/Engine/System/Parameters/ParameterManager.h)

- 所有者 / I/F: static本体がgroup/value/custom draw/applier mapを所有。GameApplicationがLoadFiles、利用側が登録/解除。全体Fなし。
- Lazy / 再初期化: group登録/load時に構成。LoadFilesは全登録を解除するresetではない。対象ごとのUnregisterが必要。
- 借用 / Thread: applierのconst void* owner keyとfunction captureが借用。Light/Particleは解除経路あり、Legacy CollisionManagerの解除は見つからない。Main/UI、同期なし。
- Scene跨ぎ / Singleton必要性: 保存値は跨げるがScene object callbackは跨がせない。ProjectSettingsとは責務が違い、単純統合は保留。

### PlanarReflectionCaptureDiagnostics

**統合候補**。リスク: 中。効果: Actor別診断entryの終了担当をPlanarReflectionManager/Component寿命に合わせられる。[宣言](../Project/Engine/Graphics/Renderer/Reflection/PlanarReflectionCaptureDiagnostics.h)

- 所有者 / I/F: static本体がActor→stats mapを所有。SceneBridgeがRecord、Component InspectorがGet。I/F/Clear/Unregisterなし。
- Lazy / 再初期化: Record時にentry確保。全体reset経路がなく、Actor破棄後もkey/統計が残る。
- 借用 / Thread: const Actor*をkeyとして保持、mapはActorを所有しない。Main、同期なし。
- Scene跨ぎ / Singleton必要性: 旧Actorの統計を跨ぐ必要はない。既存Reflection診断へ統合候補、pointer再利用による古い統計の混入も検証する。

### PlanarReflectionManager

**今回判断保留**。リスク: 高。効果: Scene surfaceと共有RTの寿命・終了を明確化する必要。[宣言](../Project/Engine/Graphics/Renderer/Reflection/PlanarReflectionManager.h) / [実装](../Project/Engine/Graphics/Renderer/Reflection/PlanarReflectionManager.inl)

- 所有者 / I/F: static本体がsurface RT/depth/descriptorを所有。ComponentのIからManager I。同じDeviceなら再Iを抑止。通常終了のF外部呼出なし。
- Lazy / 再初期化: Component利用で起動、capture targetは必要時生成。UnregisterSurfaceは借用を外してtargetを保持する。再Iの安全性はGPU完了・登録解除・保持targetの検証が必要。
- 借用 / Thread: DirectXCommon*、owner const void*、receiver Actor*、draw binding/Camera scopeを借用。Main render、同期なし。
- Scene跨ぎ / Singleton必要性: RT再利用は可能だがScene ownerは跨がせない。現Fの処理とretired target設計を含むGPU寿命確認まで保留し、安易な即時解放をしない。

### PostEffectManager

**維持**。リスク: 高。効果: effect chain/scene RT/SSFRの既存順を保つ。[宣言](../Project/Engine/Graphics/PostEffect/Manager/PostEffectManager.h)

- 所有者 / I/F: static本体がPipelineBuilder/Registry/Chain/RuntimeState/RTManager/Executor/EditorPanelをunique_ptr所有。FrameworkがI/F。
- Lazy / 再初期化: explicit I。FはExecuteAndWait後にSSFR/子を終了。I自体は既存子のFを行わず、同heap再IはRTManagerのdescriptor未返却問題もある。
- 借用 / Thread: DirectXCommon*、子同士のborrowed pointer。Main、GPU command recording。
- Scene跨ぎ / Singleton必要性: scene output基盤は跨ぐ。子が既に通常所有で分離されているため、facadeの数だけを減らさない。

### PrefabInstanceRegistry

**通常所有へ変更候補**。リスク: 中〜高。効果: Actor→prefab参照をWorld寿命へ限定できる。[宣言](../Project/Engine/Scene/Actor/Serialization/PrefabInstanceRegistry.h)

- 所有者 / I/F: static mapがpathを所有。I/Fなし、Level/Prefab読込でRegister、ActorWorldの解放経路でUnregister。
- Lazy / 再初期化: 登録時のCPU確保のみ。全体Clearはなく各Actorの解除で再利用。
- 借用 / Thread: const Actor*のidentity key。map操作はmutex、Actorの生存保証やWorld並列操作を提供するものではない。
- Scene跨ぎ / Singleton必要性: 各Worldの登録だけ必要。ActorWorld等への通常所有候補だがLevelLoader/Streaming/Editorからの参照を維持する必要がある。

### RTVManager

**維持**。リスク: 高。効果: device共通RTV heapの割当/返却を保つ。[宣言](../Project/Engine/Graphics/Descriptor/RTV/RTVManager.h)

- 所有者 / I/F: static本体がheap/free indexを所有。DirectXCommonがI/F、render target resourceは各利用者所有。
- Lazy / 再初期化: explicit I、重複I抑止なし。Fでheap/index/queueをreset。旧descriptor利用とGPU参照が終わってから再Iする必要。
- 借用 / Thread: DirectXCommon*、descriptor生成時のresource借用。allocationはmutex、I/Fとcommand利用はMain。
- Scene跨ぎ / Singleton必要性: device heapは跨ぐ。通常Device ownerへ移せるがGPU寿命の変更が大きく維持。

### ReflectionProbeManager

**維持**。リスク: 高。効果: probe RT/retired targetとcapture overrideの既存動作を保つ。[宣言](../Project/Engine/Graphics/Renderer/Reflection/ReflectionProbeManager.h)

- 所有者 / I/F: static本体がprobe target/retired targetを所有。GameApplicationがI/F、Componentが登録/解除。
- Lazy / 再初期化: targetはcapture時生成。IはFを呼ぶため登録中に再Iするとprobeも消える。GPU完了・Component再登録が再I条件。
- 借用 / Thread: DirectXCommon*、const void* owner key、capture callback/Camera override。Main render、同期なし。
- Scene跨ぎ / Singleton必要性: GPU cacheは跨げるがowner keyはScene寿命。FがCamera overrideも解除するため、所有移行はcapture契約と合わせて扱う。

### RenderDepthContext

**今回判断保留**。リスク: 高。効果: attachment resourceと借用binding/descriptorの寿命を対応させる必要。[宣言](../Project/Engine/Graphics/RenderTarget/Depth/RenderDepthContext.h)

- 所有者 / I/F: static本体が追加DSV/SRV indexとbinding stackを所有、depth resourceはMain/各RT owner所有。I/Fなし。RT ownerがClearDefaultTarget/ReleaseAttachment。
- Lazy / 再初期化: PrepareForShaderRead時にattachmentを準備。個別Release後再生成できるが全体resetはなく、各ownerの解除が前提。
- 借用 / Thread: resource raw pointer、RTV/DSV handle、default/override/prepared bindingを保持。Main command recording、同期なし。
- Scene跨ぎ / Singleton必要性: 描画先共通contextは有用だがScene固有bindingは跨がせない。Transparent/Volume/Reflectionの状態遷移検証まで保留。

### RenderGraphVisualizer

**通常所有へ変更候補**。リスク: 低〜中。効果: 表示・cache操作UIをEditorの寿命へ揃える。[宣言](../Project/Engine/Graphics/RenderGraph/RenderGraphVisualizer.h)

- 所有者 / I/F: static本体が表示状態を所有。I/Fなし、DebugSceneのvalidation表示からDraw、EditorProfilerPanelから表示切替。
- Lazy / 再初期化: GPU Iなし。全体resetなし、Drawで現在graphを読む。
- 借用 / Thread: Draw引数のRenderPipelineController&とgraph/cacheをその場で借用。Main/UI、shader/PSO cache操作は既存の同期条件に従う。
- Scene跨ぎ / Singleton必要性: 表示設定だけ跨げばよい。Editor panel所有候補、Runtime module分類の整理は別途必要。

### SRVManager

**維持**。リスク: 高。効果: persistent/transient descriptorとheap bindingの契約を保つ。[宣言](../Project/Engine/Graphics/Descriptor/SRV/SRVManager.h)

- 所有者 / I/F: static本体がheap・free list・frame別transient状態を所有。DirectXCommonがI/F、実texture/bufferは各owner所有。
- Lazy / 再初期化: explicit I、Fでallocation状態を整理。再Iは全利用者の旧index/handle破棄とGPU完了が前提。
- 借用 / Thread: DirectXCommon*とview生成時resource。割当状態はmutex、PreDraw/heap利用やI/Fの並列安全は別。
- Scene跨ぎ / Singleton必要性: device heapは跨ぐ。Texture/ImGui/RT等が共有するため現状維持、単にUAVManagerと一つにしない。

### SkyBoxManager

**維持**。リスク: 中。効果: SkyBox/Cloud描画のPSO共有を保つ。[宣言](../Project/Engine/Graphics/Renderer/SkyBox/SkyBoxManager.h)

- 所有者 / I/F: static本体がPipelineFactory/PipelineSetを値所有。FrameworkがI/F。
- Lazy / 再初期化: explicit I、Fでpipeline/factoryを終了。再IはGPU/Device条件付き。
- 借用 / Thread: DirectXCommon*を保持。Main、GPU command recording。
- Scene跨ぎ / Singleton必要性: PSO cacheは跨ぐ。通常renderer所有も可能だが、今はSkyBox texture/Environment設定の寿命問題を優先する。

### SpriteManager

**維持**。リスク: 中。効果: UI/backgroundの共有pipelineを保つ。[宣言](../Project/Engine/Graphics/Renderer/Sprite/Core/SpriteManager.h)

- 所有者 / I/F: static本体がSpritePipelineSetを所有。FrameworkがI/F。
- Lazy / 再初期化: explicit I、FでpipelineとDevice借用を解除。再IはGPU条件付き。
- 借用 / Thread: DirectXCommon*、各Sprite/Componentは共有設定を利用。Main graphics、同期なし。
- Scene跨ぎ / Singleton必要性: UI/transitionも利用するため跨ぐ。共有pipelineは必要、singleton storageは必須でないが今回は維持。

### SubLevelManager

**通常所有へ変更候補**。リスク: 高。効果: request/generationとWorldの寿命をWorldPartitionへ揃える。[宣言](../Project/Engine/Scene/Streaming/SubLevelManager.h)

- 所有者 / I/F: static本体がreference/entry/request handle/ActorHandleを所有。WorldPartitionがConfigure/Reset、Resetでcancel・世代無効化・World解除。
- Lazy / 再初期化: Configureで構成、load時にStreaming要求。Reset→Configure可だがqueued completionのcancel確認が前提。
- 借用 / Thread: ActorWorld*、completionのthis capture。workerはfile payload読込、manager変更とActor追加はMain completion。manager本体に並列map同期なし。
- Scene跨ぎ / Singleton必要性: 同じWorldのsublevelは継続、旧Worldのentryは跨がせない。WorldPartition/World通常所有候補、callback寿命の証明が先。

### TextureManager

**維持**。リスク: 高。効果: texture/descriptor cacheとCPU decodeの分担を保つ。[宣言](../Project/Engine/Graphics/Resource/Texture/TextureManager.h)

- 所有者 / I/F: static本体がtexture ComPtr・SRV index・path indexを所有。FrameworkがI/F。FはSRV返却→resource解放→map/Device借用解除。
- Lazy / 再初期化: I時にfallback等を準備し一般textureは要求時load。live二重Iの抑止なし。再Iは借用handle破棄とGPU完了が必要。
- 借用 / Thread: DirectXCommon*、利用側にmetadata参照/GPU handleを貸す。GPU/cache操作はMain、static CPU decodeはAssetSystem workerでも利用。cache全体のlockなし。
- Scene跨ぎ / Singleton必要性: asset cacheは跨ぐ。Environment等の借用handle寿命が未整理のため現状維持。

### UAVManager

**維持**。リスク: 高。効果: compute用heapとCPU clear用heapの現契約を保つ。[宣言](../Project/Engine/Graphics/Descriptor/UAV/UAVManager.h)

- 所有者 / I/F: static本体がshader-visible/CPU-clear heapとfree indexを所有。FrameworkがI/F。
- Lazy / 再初期化: explicit I、Fでheap/index/queueをreset。I単独ではallocation状態を全resetしない。再IはGPU/利用者解除が前提。
- 借用 / Thread: DirectXCommon*、view生成時resourceを借用。割当はmutex、PreDispatch/I/FはMain直列化。
- Scene跨ぎ / Singleton必要性: compute heapは跨ぐ。SRVと用途・binding・clear契約が異なるため、数削減のための統合はしない。

### VfxCueRuntime

**維持**。リスク: 高。効果: cue/timelineから各backendへ接続する既存facadeを保つ。[宣言](../Project/Engine/Vfx/Runtime/VfxCueRuntime.h)

- 所有者 / I/F: static本体がcue/instance/adapter stateを所有。GameApplicationがI/F、FはSceneが生存する間にStopAllしてmapを解除。
- Lazy / 再初期化: Updateでも未起動ならI。F後再I経路あり、旧handleを持ち越さず各backendが生存していることが必要。
- 借用 / Thread: activeWorld_はidentity比較用、adapter内World*/ActorHandleはborrowed。World切替でAbandonWorld。Main、同期なし。
- Scene跨ぎ / Singleton必要性: cue定義は共有、再生instanceはWorld別に停止。Actor/Componentのゲーム内容所有を奪わず現状維持。

### VfxDiagnosticsWindow

**通常所有へ変更候補**。リスク: 低〜中。効果: VFX診断UIをEditor ownerへ限定する。[宣言](../Project/Engine/Vfx/Graph/Editor/VfxDiagnosticsWindow.h)

- 所有者 / I/F: static本体がUI/stress設定を所有。I/Fなし、GameApplicationのEditor描画からDraw。
- Lazy / 再初期化: GPU Iなし、stress開始時にGraphをloadする操作あり。UI全体resetなし。
- 借用 / Thread: Graph/Cue/DiagnosticsをDraw時借用。Main/UI、runtimeに対する操作は既存APIを通す。
- Scene跨ぎ / Singleton必要性: 表示設定以外の跨ぎは不要。Editor通常所有候補、stress loopの停止はruntime側と合わせる。

### VfxGraphDiagnostics

**通常所有へ変更候補**。リスク: 中〜高。効果: 計測履歴とstress loopの終了を診断sessionへ揃える。[宣言](../Project/Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h)

- 所有者 / I/F: static本体がhistory/stressLoopHandlesを所有。I/Fなし、GameApplicationがCaptureFrame、UIがResetHistory/負荷開始・停止。
- Lazy / 再初期化: 計測時/負荷開始時に構成。ResetHistoryはloop停止と同義ではなく全体終了契約なし。
- 借用 / Thread: VfxGraphPlayHandleを保持、backendを直接所有しない。Mainの計測/UI、同期なし。
- Scene跨ぎ / Singleton必要性: historyは跨げるがstress handleはruntime寿命内。Editor/計測owner候補、終了時の負荷停止を先に検証する。

### VfxGraphEditor

**通常所有へ変更候補**。リスク: 中。効果: Graph編集・previewの終了をEditorへ限定する。[宣言](../Project/Engine/Vfx/Graph/Editor/VfxGraphEditor.h)

- 所有者 / I/F: static本体がgraph/compile結果/preview handleを所有。GameApplicationのEditor経路がI/F、FはStopPreviewと編集データ解除。
- Lazy / 再初期化: explicit Iに加え表示時の未起動確認あり。Iはdefault graphを読込/compile、F後再Iはbackend生存が前提。
- 借用 / Thread: runtime再生handleを保持。Main/UI・同期file I/O、GPU自体はbackend所有。
- Scene跨ぎ / Singleton必要性: 編集documentは跨げるがpreviewは停止が必要。Editor通常所有候補、現在Runtime分類である点も残課題。

### VfxGraphRuntime

**今回判断保留**。リスク: 高。効果: compiled programとParticle/Cueを束ねるloopの終了対称性が必要。[宣言](../Project/Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h)

- 所有者 / I/F: static本体がprogram/path/active loop scalability mapを所有。Register/Load/Playで構成、GameApplicationがBeginFrame/UpdateScalability、全体I/Fなし。
- Lazy / 再初期化: Graph load/playでbackendを利用。個別StopLoopはあるが全体F後再起動の契約なし。
- 借用 / Thread: Particle handleとCue integration handleを保持。Actor/Worldの直接所有なし。Main、同期なし。
- Scene跨ぎ / Singleton必要性: compiled assetは跨げるがloopは別。Particle/Cue/Editor preview終了との整合を確認するまで保留。

### Wireframe

**維持**。リスク: 中〜高。効果: debug描画bufferと現在のDraw/Reset位置を保つ。[宣言](../Project/Engine/Graphics/Renderer/Wireframe/Core/Wireframe.h)

- 所有者 / I/F: static本体がPSO/vertex/index/instance/transform bufferを所有。FrameworkがI/F、frameのResetで要素数を戻す。
- Lazy / 再初期化: explicit I。FでUnmap/resource解放/CameraとDevice借用解除。再IはGPU完了・Camera再設定が前提。
- 借用 / Thread: DirectXCommon*/Camera*、自所有bufferのmapped pointer。Main、draw queueの並列同期なし。
- Scene跨ぎ / Singleton必要性: debug rendererは跨ぐ。Physics/Editor/Runtimeの表示で利用されるため現状維持。

### WorldPartitionManager

**通常所有へ変更候補**。リスク: 高。効果: active Worldとpartition/sublevel要求の寿命を揃える。[宣言](../Project/Engine/Scene/Streaming/WorldPartitionManager.h)

- 所有者 / I/F: static本体がsettings/reference一覧を所有。TransactionalLevelLoaderがConfigure、ActorWorld終了とFramework終了がReset。SubLevelManagerにもConfigure/Resetを委譲。
- Lazy / 再初期化: Configureで構成、Reset→Configure可。Framework::Runの既存位置でUpdate。
- 借用 / Thread: ActorWorld*を保持。Main、sublevelのasyncはStreaming側、partition自体は同期なし。
- Scene跨ぎ / Singleton必要性: partitionはWorld単位でよく旧World参照は跨がせない。ActorWorld等の通常所有候補、completionの寿命と現在の更新順を保つ必要がある。

## Singletonではない通常Manager（11型）

### CollisionManager

**今回判断保留**。リスク: 高。効果: Legacy衝突経路とParameter callbackの対象寿命を確認する必要。[宣言](../Project/Engine/Physics/Collision/Legacy/CollisionManager.h)

- 所有者 / I/F: LevelObjectManagerがunique_ptr所有してI。F/独自デストラクタなし、Resetはcollider登録/contactのClear。Stageにも登録先として受け取るAPIがある。
- Lazy / 再初期化: LevelObjectManagerが必要時に生成/I。IはParameter applier登録、Resetはその解除ではない。二重I/破棄後のcallback残存は未保証。
- 借用 / Thread: Collider*配列/contact/判定callbackを借用し、ParameterManagerへthis captureを登録。対応Unregisterが見つからない。Main、同期なし。
- Scene跨ぎ / Singleton必要性: Level内だけ必要。既に通常所有でsingleton化不要。PhysicsWorldへの機械的置換や名前だけによるLegacy削除はしない。

### DX12CommandManager

**維持**。リスク: 高。効果: Deviceに従うcommand/allocator/queue所有を保つ。[宣言](../Project/Engine/Graphics/Device/Command/DX12CommandManager.h)

- 所有者 / I/F: DirectXCommonがunique_ptr所有しI/F。自身がqueue/list/allocator/frame resourcesをComPtr所有。
- Lazy / 再初期化: explicit I、frame構成もDirectXCommonが設定。Fはsubmitted状態がないことをassert。再IはGPU完了とframe/fence再接続が前提。
- 借用 / Thread: DX12FenceManager*を保持、command list等を利用者へ貸す。Main recording、一つのlistの並列記録保証なし。
- Scene跨ぎ / Singleton必要性: deviceとともに跨ぐ。既に適切な通常ownerがありsingleton不要。

### DX12FenceManager

**維持**。リスク: 高。効果: fence/eventの寿命をDevice rootに対応させたまま保つ。[宣言](../Project/Engine/Graphics/Device/Synchronization/DX12FenceManager.h)

- 所有者 / I/F: DirectXCommonがunique_ptr所有してI/F。自身がComPtr fenceとWin32 eventを所有、FでCloseHandle。
- Lazy / 再初期化: explicit I、Fでhandle/fence/valueをreset。F後再I経路はあるが未完了GPU workを残してはならない。live二重Iはevent解放漏れの危険。
- 借用 / Thread: Device/queueは引数として借用、eventは所有。現在Mainでsignal/wait、CPU側fenceValue等の並列更新同期なし。
- Scene跨ぎ / Singleton必要性: deviceとともに跨ぐ。通常所有を維持、singleton化不要。

### DXCCompilerManager

**維持**。リスク: 中。効果: compilerとshader cacheをDevice rootへ対応させたまま保つ。[宣言](../Project/Engine/Graphics/Shader/Compiler/DXCCompilerManager.h)

- 所有者 / I/F: DirectXCommonがunique_ptr所有しI/F。自身はDXC COM interface/include handler/cache blobを所有。
- Lazy / 再初期化: compilerはexplicit I、shader compile/cacheは要求時。F後再I可だが利用中compileの停止が前提。live二重Iは契約なし。
- 借用 / Thread: compiler interfaceを利用側へrawで貸す。cacheはmutex、compiler COM自体の並列実行安全を保証するものではない。現在主にMain初期化/描画準備。
- Scene跨ぎ / Singleton必要性: compiler cacheは跨ぐ。既に通常所有のため維持。

### JsonDataManager

**維持**。リスク: 低。効果: 状態を持たないJSON操作utilityを維持する。[宣言](../Project/Engine/System/JsonAssets/JsonDataManager.h)

- 所有者 / I/F: static関数のみでservice本体・共有資源なし。入出力データは呼出元所有、I/F不要。
- Lazy / 再初期化: 呼出ごとのfile I/Oのみ。再初期化の対象なし。
- 借用 / Thread: JsonAssetEntry等の参照を呼出中借用。現在Main、同一fileへの並列書込は調停しない。
- Scene跨ぎ / Singleton必要性: 状態がないため跨ぎ寿命なし。singleton化・新owner追加は不要。

### LevelObjectManager

**削除候補**。リスク: 中。効果: 未使用のLevel/Legacy衝突経路を縮小できる可能性。[宣言](../Project/Engine/Scene/Level/LevelObjectManager.h)

- 所有者 / I/F: 現行Engine/Applicationに生成・利用元は見つからない。型自身はObject3D/AnimationModel/Collider/CollisionManagerをunique_ptr所有。公開I、Fなし。
- Lazy / 再初期化: I内でCollisionManagerを必要時生成。Iはobjects/collidersをClearする一方animationModels等の全resetではなく、反復Iの安全性は未保証。
- 借用 / Thread: 内部CollisionManagerへColliderを登録する設計、Parameterのthis captureも間接的に関係する。現行実行threadなし、利用するならMain前提の描画/衝突処理。
- Scene跨ぎ / Singleton必要性: 本来Level内だけ。singleton化不要。projectにcompile対象として残り公開headerもあるため、利用が見えないだけで今回は削除しない。

### PostEffectRenderTargetManager

**維持**。リスク: 高。効果: PostEffect配下の通常所有を保ち、descriptor寿命課題を切り分ける。[宣言](../Project/Engine/Graphics/PostEffect/Manager/PostEffectRenderTargetManager.h)

- 所有者 / I/F: PostEffectManagerがunique_ptr所有しI/F。自身がscene/ping-pong/depth resourceとdescriptor indexを所有。
- Lazy / 再初期化: explicit I、resize時再確保。FはresourceとRenderDepthContext attachmentを解除するがRTV/SRV/DSV indexを返却しない。同heap内の反復Iは安全と判定しない。
- 借用 / Thread: DirectXCommon*、executorへRT情報を貸す。Main graphics、独立workerなし。
- Scene跨ぎ / Singleton必要性: viewport/chainとともに跨ぐ。通常所有は適切、singleton化せずdescriptor返却の設計を別途検証する。

### ResourceManager

**維持**。リスク: 低。効果: resource生成と利用側所有の単純な分担を維持する。[宣言](../Project/Engine/System/Resource/ResourceManager.h)

- 所有者 / I/F: static生成関数のみ。返すComPtrは呼出元所有、Manager自身のI/Fなし。
- Lazy / 再初期化: 呼出時にbuffer生成、service再起動の対象なし。
- 借用 / Thread: ID3D12Device*を呼出中借用。現在GPU準備箇所から利用。共有cacheはなく、並列可否はDevice APIと呼出側の同期条件に従う。
- Scene跨ぎ / Singleton必要性: 本体状態なし、resource寿命は各owner次第。singletonも新Managerも不要。

### SceneManager

**維持**。リスク: 高。効果: Application→Scene→Actor/Componentの通常所有を維持する。[宣言](../Project/Engine/Scene/Management/SceneManager.h)

- 所有者 / I/F: GameApplicationがunique_ptr所有しI/F後に本体を破棄。SceneManagerがcurrent/next Scene、factory、transitionをunique_ptr所有。FはScene/transitionへ委譲するがpointer resetは本体破棄時。
- Lazy / 再初期化: 本体はApplication起動時生成、Sceneは遷移要求で生成。Iは状態flag初期化中心で、F→Iだけをcold再起動として保証しない。
- 借用 / Thread: BaseScene/Editor各サービスがSceneManager*を借用。自身はSceneを所有。Mainで遷移/Update/Draw、既存safe pointを使う。
- Scene跨ぎ / Singleton必要性: Scene管理者として跨ぐ。既存通常所有を維持しsingleton化しない。Editor依存整理もActor/Component構造を維持して行う。

### StageChunkManager

**維持**。リスク: 中。効果: Stage内のchunk/culling所有を維持する。[宣言](../Project/Engine/Scene/Level/StageChunkManager.h)

- 所有者 / I/F: Stageが値所有。I/F名のAPIはなくStageがRebuild/Clear、Stage終了でもClear。
- Lazy / 再初期化: Stageの再構築要求時にchunkを作成。Clear/Rebuildで再利用可、元Object3Dの生存が必要。
- 借用 / Thread: StageChunkMeshRefにStageのObject3D*を保持。Mainの可視判定/描画、同期なし。
- Scene跨ぎ / Singleton必要性: Stage内だけ必要。既に適切な通常所有でsingleton不要。

### StageInstancingManager

**維持**。リスク: 中。効果: Stageのinstanceとrenderer所有を維持する。[宣言](../Project/Engine/Scene/Level/StageInstancingManager.h)

- 所有者 / I/F: Stageが値所有。StageからBuild/Clear。自身はInstancedObject3DRenderer/通常fallback Object3Dをunique_ptr所有。
- Lazy / 再初期化: Buildでbatch作成、比較用通常ObjectはEnsureNormalFallbackObjectsで遅延生成。Clear→Build可だがGPU利用完了が前提。
- 借用 / Thread: input LevelDataはBuild中借用、sourceはpath/matrix等の値copy。子renderer内部にDevice/cache参照。Main graphics。
- Scene跨ぎ / Singleton必要性: Stage寿命だけ必要。現状の通常所有を維持しsingleton化しない。

## 調査の限界と次の検証

これは宣言・実装・現行呼出元を追った静的棚卸しで、全105サービスを実行してI/F反復・並列競合を検証したものではない。候補の採用には以下が必要。

- GPU owner変更: GPU debug layer/live object、Frames in Flight、resize/capture、descriptor使用数とfence完了の追跡。
- World/Editor owner変更: Scene切替反復、PIE Begin/Stop/Keep Changes、Undo/drag/picking中のWorld破棄、未完了load/buildの終了。
- VFX/Audio owner変更: loop/preview/Component破棄とbackend再起動、handle失効、同時再生/終了の競合。通常時の挙動比較を含む。
- Legacy削除: 全project/asset/公開APIの利用確認と代替経路の回帰検証。名前や呼出数だけで決めない。

今回の実装差分、分類集計、実際に実行したビルド・回帰テストの結果は[進捗記録](ProductionEngineProgress.md)に記載する。
