from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SETTINGS_PATH = PROJECT_ROOT / "Resources/JSON/ProjectSettings.json"
MODULES_PATH = PROJECT_ROOT / "Build/Modules/EngineModules.json"


@dataclass(frozen=True)
class ValidationIssue:
    source: Path
    message: str

    def format(self) -> str:
        try:
            relative = self.source.relative_to(PROJECT_ROOT)
        except ValueError:
            relative = self.source
        return f"{relative.as_posix()}: {self.message}"


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def resolve_project_path(value: str, *, source: Path | None = None) -> Path:
    raw = value.replace("\\", "/").strip()
    if not raw:
        return Path()
    path = Path(raw)
    if path.is_absolute():
        return path
    if raw.startswith("Resources/") or raw.startswith("Build/") or raw.startswith("Engine/") or raw.startswith("ApplicationLayer/"):
        return PROJECT_ROOT / path
    if source is not None:
        return source.parent / path
    return PROJECT_ROOT / path


def validate_project_settings(path: Path = SETTINGS_PATH) -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []
    if not path.is_file():
        return [ValidationIssue(path, "Project Settingsが存在しません")]
    try:
        data = load_json(path)
    except Exception as exc:  # noqa: BLE001 - CIではJSON例外をまとめて診断する。
        return [ValidationIssue(path, f"JSONを読み込めません: {exc}")]

    if not isinstance(data, dict):
        return [ValidationIssue(path, "rootはobjectである必要があります")]
    if data.get("Format") != "Ken4lowProjectSettings":
        issues.append(ValidationIssue(path, "FormatはKen4lowProjectSettingsである必要があります"))
    if data.get("Version") != 1:
        issues.append(ValidationIssue(path, "Version 1以外は現在未対応です"))

    for key in ("ProjectName", "ResourceRoot", "SceneRegistry"):
        if not isinstance(data.get(key), str) or not data[key].strip():
            issues.append(ValidationIssue(path, f"{key}は空でない文字列である必要があります"))

    registry = data.get("SceneRegistry", "")
    if isinstance(registry, str) and registry:
        resolved = resolve_project_path(registry)
        if not resolved.is_file():
            issues.append(ValidationIssue(path, f"SceneRegistryが存在しません: {registry}"))

    fallback = data.get("FallbackAssets")
    if not isinstance(fallback, dict):
        issues.append(ValidationIssue(path, "FallbackAssets objectが必要です"))
    else:
        if not isinstance(fallback.get("TextureKey"), str) or not fallback.get("TextureKey"):
            issues.append(ValidationIssue(path, "FallbackAssets.TextureKeyが必要です"))
        if not isinstance(fallback.get("ModelKey"), str) or not fallback.get("ModelKey"):
            issues.append(ValidationIssue(path, "FallbackAssets.ModelKeyが必要です"))
        if fallback.get("AudioMode") not in ("Silent",):
            issues.append(ValidationIssue(path, "FallbackAssets.AudioModeは現在Silentのみ対応です"))
    return issues


def iter_project_json() -> Iterable[Path]:
    resources = PROJECT_ROOT / "Resources"
    if not resources.is_dir():
        return []
    return resources.rglob("*.json")


def validate_all_json() -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []
    for path in iter_project_json():
        try:
            load_json(path)
        except Exception as exc:  # noqa: BLE001
            issues.append(ValidationIssue(path, f"JSON構文エラー: {exc}"))
    return issues


def _resolve_scene_asset(value: str, *, kind: str, source: Path) -> Path:
    if value.replace("\\", "/").startswith("Resources/"):
        return resolve_project_path(value)
    if kind == "bgm":
        return PROJECT_ROOT / "Resources/Sounds" / value
    return resolve_project_path(value, source=source)


def validate_scene_references(registry_path: Path | None = None) -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []
    if registry_path is None:
        try:
            settings = load_json(SETTINGS_PATH)
            registry_path = resolve_project_path(settings.get("SceneRegistry", "Resources/JSON/Scenes/SceneRegistry.json"))
        except Exception:  # noqa: BLE001
            registry_path = PROJECT_ROOT / "Resources/JSON/Scenes/SceneRegistry.json"

    if not registry_path.is_file():
        return [ValidationIssue(registry_path, "Scene Registryが存在しません")]
    try:
        registry = load_json(registry_path)
    except Exception as exc:  # noqa: BLE001
        return [ValidationIssue(registry_path, f"Scene Registryを読めません: {exc}")]

    if registry.get("Format") != "Ken4lowSceneRegistry":
        issues.append(ValidationIssue(registry_path, "FormatがKen4lowSceneRegistryではありません"))
    scenes = registry.get("Scenes", [])
    if not isinstance(scenes, list):
        return issues + [ValidationIssue(registry_path, "Scenesは配列である必要があります")]

    known_ids: set[str] = set()
    scene_documents: list[tuple[Path, dict[str, Any]]] = []
    for entry in scenes:
        if not isinstance(entry, str):
            issues.append(ValidationIssue(registry_path, "Scenesの要素は文字列である必要があります"))
            continue
        scene_path = registry_path.parent / entry
        if not scene_path.is_file():
            issues.append(ValidationIssue(registry_path, f"Scene定義が存在しません: {entry}"))
            continue
        try:
            scene = load_json(scene_path)
        except Exception as exc:  # noqa: BLE001
            issues.append(ValidationIssue(scene_path, f"Scene定義を読めません: {exc}"))
            continue
        if not isinstance(scene, dict):
            issues.append(ValidationIssue(scene_path, "Scene定義rootはobjectである必要があります"))
            continue
        scene_id = scene.get("Id")
        if not isinstance(scene_id, str) or not scene_id:
            issues.append(ValidationIssue(scene_path, "Idが必要です"))
        elif scene_id in known_ids:
            issues.append(ValidationIssue(scene_path, f"Scene Idが重複しています: {scene_id}"))
        else:
            known_ids.add(scene_id)
        scene_documents.append((scene_path, scene))

        level = scene.get("Level", "")
        if isinstance(level, str) and level:
            resolved = _resolve_scene_asset(level, kind="level", source=scene_path)
            if not resolved.is_file():
                issues.append(ValidationIssue(scene_path, f"Level参照が切れています: {level}"))
        bgm = scene.get("BGM", "")
        if isinstance(bgm, str) and bgm:
            resolved = _resolve_scene_asset(bgm, kind="bgm", source=scene_path)
            if not resolved.is_file():
                issues.append(ValidationIssue(scene_path, f"BGM参照が切れています: {bgm}"))

    for key in ("StartupScene", "DebugStartupScene"):
        scene_id = registry.get(key, "")
        if isinstance(scene_id, str) and scene_id and scene_id not in known_ids:
            issues.append(ValidationIssue(registry_path, f"{key}が未登録Sceneを参照しています: {scene_id}"))

    for scene_path, scene in scene_documents:
        for key in ("NextScene", "RetryScene"):
            scene_id = scene.get(key, "")
            if isinstance(scene_id, str) and scene_id and scene_id not in known_ids:
                issues.append(ValidationIssue(scene_path, f"{key}が未登録Sceneを参照しています: {scene_id}"))
    return issues


def _visit_level_prefabs(value: Any) -> Iterable[str]:
    if isinstance(value, dict):
        prefab = value.get("Prefab")
        if isinstance(prefab, dict) and isinstance(prefab.get("Path"), str) and prefab["Path"]:
            yield prefab["Path"]
        for child in value.values():
            yield from _visit_level_prefabs(child)
    elif isinstance(value, list):
        for child in value:
            yield from _visit_level_prefabs(child)


def validate_level_references() -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []
    for path in iter_project_json():
        try:
            data = load_json(path)
        except Exception:  # JSON構文自体はvalidate_all_jsonで報告する。
            continue
        if not isinstance(data, dict) or data.get("Format") != "Ken4lowLevel":
            continue
        version = data.get("Version", 1)
        if not isinstance(version, int) or version < 1 or version > 3:
            issues.append(ValidationIssue(path, f"未対応Level Versionです: {version}"))
        partition = data.get("WorldPartition", {})
        if partition and not isinstance(partition, dict):
            issues.append(ValidationIssue(path, "WorldPartitionはobjectである必要があります"))
        elif isinstance(partition, dict):
            load_radius = partition.get("LoadRadiusCells", 1)
            unload_radius = partition.get("UnloadRadiusCells", 2)
            cell_size = partition.get("CellSize", 128.0)
            if not isinstance(cell_size, (int, float)) or cell_size <= 0:
                issues.append(ValidationIssue(path, "WorldPartition.CellSizeは0より大きい必要があります"))
            if not isinstance(load_radius, int) or not isinstance(unload_radius, int) or load_radius < 0 or unload_radius < load_radius:
                issues.append(ValidationIssue(path, "WorldPartitionのLoad/Unload Radiusが不正です"))

        sublevels = data.get("SubLevels", [])
        if not isinstance(sublevels, list):
            issues.append(ValidationIssue(path, "SubLevelsは配列である必要があります"))
        else:
            sublevel_ids: set[str] = set()
            for index, sublevel in enumerate(sublevels):
                if not isinstance(sublevel, dict):
                    issues.append(ValidationIssue(path, f"SubLevels[{index}]がobjectではありません"))
                    continue
                sublevel_id = sublevel.get("Id")
                sublevel_path = sublevel.get("Path")
                if not isinstance(sublevel_id, str) or not sublevel_id:
                    issues.append(ValidationIssue(path, f"SubLevels[{index}].Idが必要です"))
                elif sublevel_id in sublevel_ids:
                    issues.append(ValidationIssue(path, f"SubLevel Idが重複しています: {sublevel_id}"))
                else:
                    sublevel_ids.add(sublevel_id)
                if not isinstance(sublevel_path, str) or not sublevel_path:
                    issues.append(ValidationIssue(path, f"SubLevels[{index}].Pathが必要です"))
                else:
                    resolved_sublevel = resolve_project_path(sublevel_path, source=path)
                    if not resolved_sublevel.is_file():
                        issues.append(ValidationIssue(path, f"SubLevel参照が切れています: {sublevel_path}"))

        actors = data.get("Actors")
        if not isinstance(actors, list):
            issues.append(ValidationIssue(path, "Actorsが配列ではありません"))
            continue
        ids: set[str] = set()
        for index, actor in enumerate(actors):
            if not isinstance(actor, dict):
                issues.append(ValidationIssue(path, f"Actors[{index}]がobjectではありません"))
                continue
            actor_id = actor.get("Id")
            if isinstance(actor_id, str) and actor_id:
                if actor_id in ids:
                    issues.append(ValidationIssue(path, f"Actor Idが重複しています: {actor_id}"))
                ids.add(actor_id)
        for prefab_path in _visit_level_prefabs(data):
            resolved = resolve_project_path(prefab_path, source=path)
            if not resolved.is_file():
                issues.append(ValidationIssue(path, f"Prefab参照が切れています: {prefab_path}"))
    return issues


def validate_assets() -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []
    issues.extend(validate_project_settings())
    issues.extend(validate_all_json())
    issues.extend(validate_scene_references())
    issues.extend(validate_level_references())
    return issues
