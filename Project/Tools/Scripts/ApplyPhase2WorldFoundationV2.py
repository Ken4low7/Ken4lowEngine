from __future__ import annotations

import sys
from pathlib import Path

import ApplyPhase2WorldFoundation as base

ROOT = Path(__file__).resolve().parents[3]


def p(rel: str) -> Path:
    return ROOT / rel


def read(rel: str) -> str:
    return p(rel).read_text(encoding="utf-8-sig")


def write(rel: str, text: str) -> None:
    target = p(rel)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8", newline="\n")


def replace_once(rel: str, old: str, new: str) -> None:
    text = read(rel)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"replace target not found: {rel}: {old[:140]}")
    write(rel, text.replace(old, new, 1))


def move_file(old_rel: str, new_rel: str) -> None:
    old_path = p(old_rel)
    new_path = p(new_rel)
    if new_path.exists():
        return
    if not old_path.exists():
        raise RuntimeError(f"move source not found: {old_rel}")
    new_path.parent.mkdir(parents=True, exist_ok=True)
    old_path.rename(new_path)


def apply_editor_separation() -> None:
    # ActorWorldのRuntime API互換を壊さず、旧ImGui/Prefab/Component編集実装だけをEditor側へ物理分離する。
    moves = {
        "Project/Engine/Scene/Actor/Core/ActorWorld_ImGui.cpp":
            "Project/Engine/Editor/Legacy/ActorWorld_ImGui.cpp",
        "Project/Engine/Scene/Actor/Core/ActorWorld_Prefab.cpp":
            "Project/Engine/Editor/Legacy/ActorWorld_Prefab.cpp",
        "Project/Engine/Scene/Actor/Core/ActorWorld_ComponentEdit.cpp":
            "Project/Engine/Editor/Legacy/ActorWorld_ComponentEdit.cpp",
    }
    for old_rel, new_rel in moves.items():
        move_file(old_rel, new_rel)

    # Visual Studioの登録も新しいEditor側パスへ追従させる。
    for project_rel in ["Project/Ken4lowEngine.vcxproj", "Project/Ken4lowEngine.vcxproj.filters"]:
        text = read(project_rel)
        for old_rel, new_rel in moves.items():
            old_win = old_rel.removeprefix("Project/").replace("/", "\\")
            new_win = new_rel.removeprefix("Project/").replace("/", "\\")
            text = text.replace(old_win, new_win)
        write(project_rel, text)

    # RuntimeのDebugSceneから旧ActorWorld直結ImGuiを呼ばない。
    debug_rel = "Project/ApplicationLayer/Scene/DebugScene/DebugScene.cpp"
    text = read(debug_rel)
    text = text.replace(
        "\tactorWorld_.DrawImGui();\n",
        "\t// ActorWorldのEditor UIはEngine/Editor側のBridgeを正規導線とし、Runtime Worldから直接描画しない。\n")
    write(debug_rel, text)

    # EditorObjectの識別にはアドレスではなくPhase 2のRuntime IDを使う。
    bridge_rel = "Project/Engine/Editor/ActorWorldEditorBridge.cpp"
    text = read(bridge_rel)
    replacements = {
        "static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(actor))":
            "static_cast<unsigned long long>(actor ? actor->GetId().value : 0)",
        "std::to_string(reinterpret_cast<uintptr_t>(actor))":
            "std::to_string(actor ? actor->GetId().value : 0)",
        "static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(component))":
            "static_cast<unsigned long long>(component ? component->GetId().value : 0)",
        "std::to_string(reinterpret_cast<uintptr_t>(component))":
            "std::to_string(component ? component->GetId().value : 0)",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    write(bridge_rel, text)

    # 新規Runtime IDヘッダーをVisual Studio上でも追えるよう登録する。
    proj_rel = "Project/Ken4lowEngine.vcxproj"
    proj = read(proj_rel)
    if "Engine\\Scene\\Actor\\Core\\WorldObjectId.h" not in proj:
        anchor = '    <ClInclude Include="Engine\\Scene\\Actor\\Core\\Actor.h" />\n'
        if anchor not in proj:
            raise RuntimeError("Actor.h ClInclude anchor not found")
        proj = proj.replace(
            anchor,
            anchor
            + '    <ClInclude Include="Engine\\Scene\\Actor\\Core\\ActorHandle.h" />\n'
            + '    <ClInclude Include="Engine\\Scene\\Actor\\Core\\WorldObjectId.h" />\n',
            1,
        )
        write(proj_rel, proj)


def apply_docs() -> None:
    write(
        "Project/Docs/Phase2WorldFoundation.md",
        """# Phase 2 — World基盤

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
`ActorWorld::ResolveActor()` で必要な瞬間に解決し、Destroy済み・World外のActorは無効になります。  
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
""",
    )


def validate_sources() -> None:
    actor_cpp = read("Project/Engine/Scene/Actor/Core/Actor.cpp")
    world_cpp = read("Project/Engine/Scene/Actor/Core/ActorWorld.cpp")
    world_h = read("Project/Engine/Scene/Actor/Core/ActorWorld.h")
    physics_cpp = read("Project/Engine/Scene/Actor/Core/ActorWorld_Pysics.cpp")
    component_h = read("Project/Engine/Scene/Actor/Core/ActorComponent.h")

    phase2_markers = [
        "ActorWorld::FindActorById",
        "ActorWorld::ResolveActor",
        "ActorWorld::OnComponentAdded",
        "Actor::GetUpdateOrderedComponents",
        "Actor::GetDrawOrderedComponents",
        "NotifyOwnerRuntimeStateChanged",
        "ActorHandle",
    ]
    joined = actor_cpp + world_cpp + world_h + component_h
    missing = [item for item in phase2_markers if item not in joined]
    if missing:
        raise RuntimeError(f"missing Phase2 markers: {missing}")

    # 前回の事故再発防止: Runtime APIがEditor分離で消えていないことを明示検証する。
    runtime_api_markers = [
        "void Initialize();",
        "void Update(float deltaTime);",
        "void UpdateEditor(float deltaTime);",
        "void PostPhysicsUpdate(float deltaTime);",
        "void Finalize();",
        "T& SpawnActor(Args&&... args)",
        "Actor* SpawnActorFromJson(",
        "Actor* SpawnActorFromJsonData(",
        "const std::vector<std::unique_ptr<Actor>>& GetActors() const",
        "void SetPhysicsWorld(PhysicsWorld* physicsWorld);",
        "void Draw();",
        "void DrawShadow();",
        "void PrepareRenderState();",
        "void SetSelectedEditorObject(Actor* actor, ActorComponent* component)",
        "void DrawSelectedInspectorContent();",
    ]
    missing_api = [item for item in runtime_api_markers if item not in world_h]
    if missing_api:
        raise RuntimeError(f"ActorWorld runtime/compat API was removed: {missing_api}")

    update_start = world_cpp.find("void ActorWorld::Update(float deltaTime)")
    update_end = world_cpp.find("void ActorWorld::UpdateEditor", update_start)
    update_body = world_cpp[update_start:update_end]
    if "RegisterPhysicsComponents(*actor)" in update_body or "UnregisterPhysicsComponents(*actor)" in update_body:
        raise RuntimeError("per-frame physics registration remains in ActorWorld::Update")

    editor_update_start = world_cpp.find("void ActorWorld::UpdateEditor(float deltaTime)")
    editor_update_end = world_cpp.find("void ActorWorld::PostPhysicsUpdate", editor_update_start)
    editor_update_body = world_cpp[editor_update_start:editor_update_end]
    if "RegisterPhysicsComponents(*actor)" in editor_update_body or "UnregisterPhysicsComponents(*actor)" in editor_update_body:
        raise RuntimeError("per-frame physics registration remains in ActorWorld::UpdateEditor")

    if 'actor.IsPhysicsRegistered() && actor.GetLayer() == "WorldStatic"' in physics_cpp:
        raise RuntimeError("old WorldStatic physics early-out remains")

    moved_files = [
        "Project/Engine/Editor/Legacy/ActorWorld_ImGui.cpp",
        "Project/Engine/Editor/Legacy/ActorWorld_Prefab.cpp",
        "Project/Engine/Editor/Legacy/ActorWorld_ComponentEdit.cpp",
    ]
    old_files = [
        "Project/Engine/Scene/Actor/Core/ActorWorld_ImGui.cpp",
        "Project/Engine/Scene/Actor/Core/ActorWorld_Prefab.cpp",
        "Project/Engine/Scene/Actor/Core/ActorWorld_ComponentEdit.cpp",
    ]
    for rel in moved_files:
        if not p(rel).exists():
            raise RuntimeError(f"moved editor source missing: {rel}")
    for rel in old_files:
        if p(rel).exists():
            raise RuntimeError(f"old Runtime Core editor source still exists: {rel}")

    debug_scene = read("Project/ApplicationLayer/Scene/DebugScene/DebugScene.cpp")
    if "actorWorld_.DrawImGui();" in debug_scene:
        raise RuntimeError("DebugScene still directly invokes ActorWorld::DrawImGui")

    project = read("Project/Ken4lowEngine.vcxproj")
    for rel in old_files:
        win = rel.removeprefix("Project/").replace("/", "\\")
        if win in project:
            raise RuntimeError(f"vcxproj still references old editor path: {win}")
    for rel in moved_files:
        win = rel.removeprefix("Project/").replace("/", "\\")
        if win not in project:
            raise RuntimeError(f"vcxproj missing moved editor path: {win}")

    bridge = read("Project/Engine/Editor/ActorWorldEditorBridge.cpp")
    if "reinterpret_cast<uintptr_t>(actor)" in bridge or "reinterpret_cast<uintptr_t>(component)" in bridge:
        raise RuntimeError("ActorWorldEditorBridge still uses raw pointer values as actor/component IDs")

    print("Phase 2 source audit: OK")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: ApplyPhase2WorldFoundationV2.py <ids|cache|physics|editor|docs|validate>")

    stage = sys.argv[1]
    if stage == "ids":
        base.apply_ids_and_handles()
    elif stage == "cache":
        base.apply_order_cache()
    elif stage == "physics":
        base.apply_event_driven_physics()
    elif stage == "editor":
        apply_editor_separation()
    elif stage == "docs":
        apply_docs()
    elif stage == "validate":
        validate_sources()
    else:
        raise SystemExit(f"unknown stage: {stage}")


if __name__ == "__main__":
    main()
