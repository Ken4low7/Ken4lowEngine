from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8-sig")


def write(rel: str, text: str) -> None:
    (ROOT / rel).write_text(text, encoding="utf-8", newline="\n")


def replace_once(rel: str, old: str, new: str) -> None:
    text = read(rel)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"target not found: {rel}")
    write(rel, text.replace(old, new, 1))


def apply() -> None:
    rel = "Project/Engine/Scene/Actor/Core/ActorWorld.cpp"
    text = read(rel)
    text = text.replace("\n\t\tRefreshActorPrefabFileList(); // Actor PrefabsのJSONファイル一覧を更新する\n", "\n")
    old = '''\tActor* ActorWorld::ResolveActor(const ActorHandle& handle)\n\t{\n\t\treturn FindActorById(handle.GetId());\n\t}\n\n\tconst Actor* ActorWorld::ResolveActor(const ActorHandle& handle) const\n\t{\n\t\treturn FindActorById(handle.GetId());\n\t}\n\n\tbool ActorWorld::IsActorHandleValid(const ActorHandle& handle) const\n\t{\n\t\tconst Actor* actor = ResolveActor(handle);\n\t\treturn actor && !actor->IsPendingDestroy();\n\t}\n'''
    new = '''\tActor* ActorWorld::ResolveActor(const ActorHandle& handle)\n\t{\n\t\tActor* actor = FindActorById(handle.GetId());\n\t\treturn actor && !actor->IsPendingDestroy() ? actor : nullptr; // Destroy予約した時点で外部Handleからは無効として扱う。\n\t}\n\n\tconst Actor* ActorWorld::ResolveActor(const ActorHandle& handle) const\n\t{\n\t\tconst Actor* actor = FindActorById(handle.GetId());\n\t\treturn actor && !actor->IsPendingDestroy() ? actor : nullptr; // World終端の実破棄待ちでも参照を再取得させない。\n\t}\n\n\tbool ActorWorld::IsActorHandleValid(const ActorHandle& handle) const\n\t{\n\t\treturn ResolveActor(handle) != nullptr;\n\t}\n'''
    if old not in text:
        raise RuntimeError("ActorHandle resolve block not found")
    write(rel, text.replace(old, new, 1))

    docs = "Project/Docs/Phase2WorldFoundation.md"
    d = read(docs)
    d = d.replace(
        "`ActorWorld::ResolveActor()` で必要な瞬間に解決し、Destroy済み・World外のActorは無効になります。  ",
        "`ActorWorld::ResolveActor()` で必要な瞬間に解決し、Destroy予約済み・World外のActorは無効になります。  ")
    marker = "`DebugScene`から`ActorWorld::DrawImGui()`を直接呼ぶ経路を外し、通常Editor導線は既存の`ActorWorldEditorBridge`を正規入口とします。  \n"
    addition = marker + "ActorWorldのRuntime初期化からPrefabファイル列挙も外し、Prefab一覧の更新はEditor側の操作時だけ行います。  \n"
    if "Runtime初期化からPrefabファイル列挙" not in d:
        if marker not in d:
            raise RuntimeError("Phase2 doc editor marker not found")
        d = d.replace(marker, addition, 1)
    write(docs, d)


def validate() -> None:
    world = read("Project/Engine/Scene/Actor/Core/ActorWorld.cpp")
    init_start = world.find("void ActorWorld::Initialize()")
    update_start = world.find("void ActorWorld::Update(float deltaTime)", init_start)
    init_body = world[init_start:update_start]
    if "RefreshActorPrefabFileList" in init_body:
        raise RuntimeError("ActorWorld::Initialize still calls editor prefab refresh")

    resolve_start = world.find("Actor* ActorWorld::ResolveActor(const ActorHandle& handle)")
    resolve_end = world.find("Actor* ActorWorld::FindActorByName", resolve_start)
    resolve_body = world[resolve_start:resolve_end]
    if "IsPendingDestroy" not in resolve_body:
        raise RuntimeError("ActorHandle resolve does not reject pending destroy actors")
    if "return ResolveActor(handle) != nullptr;" not in resolve_body:
        raise RuntimeError("ActorHandle validity is not unified with ResolveActor")

    print("Phase 2 polish audit: OK")


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2 or sys.argv[1] not in {"apply", "validate"}:
        raise SystemExit("usage: PolishPhase2WorldFoundation.py <apply|validate>")
    if sys.argv[1] == "apply":
        apply()
    else:
        validate()
