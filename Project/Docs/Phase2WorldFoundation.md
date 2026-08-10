# Phase 2 — World基盤

## 完了項目

- [x] ActorId / ComponentId
- [x] Component ordered-cache
- [x] Physics登録をevent-driven化
- [x] ActorWorldからEditorコードを分離
- [x] Actor参照Handle

## Runtime ID

`ActorId` / `ComponentId` は `ActorWorld` が単調増加で発行する実行時IDです。`0` は無効値です。  
IDはJSONへ永続化せず、Duplicate / JSON Spawn / Component再生成では新しいRuntime IDを割り当てます。  
ActorWorldから外れた時点でlookup tableから削除するため、古いIDは解決できません。

## ActorHandle

`ActorHandle` はActorやActorWorldを所有せず、`ActorId` のみを保持します。  
`ActorWorld::ResolveActor()` で必要な瞬間に解決し、Destroy予約済み・World外のActorは無効になります。  
ActorComponentから所有Actorへの短寿命な内部参照は従来どおりnon-owning raw pointerを維持します。

## Component ordered-cache

Update / PostPhysics / EditorUpdateはUpdate順キャッシュ、Draw / DrawShadowはDraw順キャッシュを利用します。  
Component追加・削除・`UpdateOrder` / `DrawOrder` 変更時だけdirtyにして再構築します。  
通常フレームでは一時vector生成と`stable_sort`を繰り返しません。

## Physics event-driven

通常フレームの`ActorWorld::Update()` / `UpdateEditor()`からPhysics Componentの再登録走査を外しました。  
以下の状態変化でActor単位のPhysics構成を同期します。

- Component追加
- Component初期化完了
- Component削除
- Component Active変更
- Actor Active変更
- Actor Spawn / Destroy / JSON Reload
- PhysicsWorld差し替え

イベント時はCollider / Rigidbodyの最新構成を同期しますが、状態変化がないフレームでは登録走査を行いません。

## Editor分離

旧ActorWorld直結のEditor実装はRuntime Coreディレクトリから以下へ移動しました。

- `Engine/Editor/Legacy/ActorWorld_ImGui.cpp`
- `Engine/Editor/Legacy/ActorWorld_Prefab.cpp`
- `Engine/Editor/Legacy/ActorWorld_ComponentEdit.cpp`

`DebugScene`から`ActorWorld::DrawImGui()`を直接呼ぶ経路を外し、通常Editor導線は既存の`ActorWorldEditorBridge`を正規入口とします。  
ActorWorldのRuntime初期化からPrefabファイル列挙も外し、Prefab一覧の更新はEditor側の操作時だけ行います。  
既存Editor機能との互換のためActorWorld上の薄いEditor APIは残しますが、Editor実装本体はRuntime Coreから分離しています。  
またEditorObject IDは生ポインタ値ではなく`ActorId` / `ComponentId`を優先します。

## Phase 2完了条件

- Runtime ID lookupがSpawn / Destroy / Component追加削除に追従する
- Destroy後のActorHandleが解決されない
- Actor Update / Drawで毎フレームsortしない
- Physics登録走査が毎フレーム実行されない
- 旧ActorWorld Editor実装がRuntime Coreディレクトリから分離されている
- Runtime側から旧ActorWorld ImGuiを直接呼ばない
- Debug / ReleaseでC++コンパイルできる
