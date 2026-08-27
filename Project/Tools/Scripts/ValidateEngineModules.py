from __future__ import annotations

import json
import posixpath
import re
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".inl"}
INCLUDE_PATTERN = re.compile(r"^\s*#\s*include\s*([<\"])([^>\"]+)[>\"]", re.MULTILINE)


def load_manifest(project_root: Path) -> dict:
    with (project_root / "Build/Modules/EngineModules.json").open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def normalize(value: str) -> str:
    return posixpath.normpath(value.replace("\\", "/")).strip("/")


def owner_for(relative_path: str, modules: list[dict]) -> list[str]:
    path = normalize(relative_path).casefold()
    owners: list[str] = []
    for module in modules:
        for root in module.get("Roots", []):
            root = normalize(root).casefold()
            if path == root or path.startswith(root + "/"):
                owners.append(module["Name"])
                break
    return owners


def collect_sources(project_root: Path) -> list[Path]:
    files: list[Path] = []
    for root_name in ("Engine", "ApplicationLayer"):
        root = project_root / root_name
        if not root.is_dir():
            continue
        files.extend(path for path in root.rglob("*") if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES)
    return sorted(files)


def build_include_index(relative_paths: list[str]) -> dict[str, set[str]]:
    index: dict[str, set[str]] = {}
    for relative in relative_paths:
        parts = relative.split("/")
        # フラットなinclude pathでもApplication依存を見落とさないよう短い表記も索引化する。
        for offset in range(len(parts)):
            alias = "/".join(parts[offset:]).casefold()
            index.setdefault(alias, set()).add(relative)
    return index


def resolve_include(
    source: str,
    delimiter: str,
    include: str,
    paths: dict[str, str],
    index: dict[str, set[str]],
) -> set[str]:
    normalized = normalize(include).casefold()
    if delimiter == '"':
        local = normalize(posixpath.join(posixpath.dirname(source), include)).casefold()
        if local in paths:
            return {paths[local]}
    if normalized in paths:
        return {paths[normalized]}
    # 同名候補を任意に一つへ絞らず、Applicationを含む曖昧なincludeも検査対象にする。
    return index.get(normalized, set())


def validate(project_root: Path = PROJECT_ROOT) -> list[str]:
    manifest = load_manifest(project_root)
    modules = manifest.get("Modules", [])
    errors: list[str] = []
    if manifest.get("Format") != "Ken4lowEngineModules" or manifest.get("Version") != 1:
        return ["EngineModules.jsonのFormat/Versionが不正です"]

    module_by_name = {module.get("Name"): module for module in modules}
    for module in modules:
        name = module.get("Name")
        if not name:
            errors.append("Nameが空のModuleがあります")
            continue
        for dependency in module.get("MayDependOn", []):
            if dependency not in module_by_name:
                errors.append(f"{name}: 未定義Moduleへ依存しています: {dependency}")

    sources = collect_sources(project_root)
    relative_paths = [source.relative_to(project_root).as_posix() for source in sources]
    paths = {relative.casefold(): relative for relative in relative_paths}
    include_index = build_include_index(relative_paths)

    for source, relative in zip(sources, relative_paths):
        owners = owner_for(relative, modules)
        if len(owners) != 1:
            errors.append(f"{relative}: Module所有者が{len(owners)}件です ({', '.join(owners)})")
            continue

        owner = owners[0]
        # ゲーム固有の生成・登録処理はApplication側へ置き、Engineを再利用可能に保つ。
        if owner != "Application":
            try:
                text = source.read_text(encoding="utf-8-sig", errors="replace")
            except OSError as error:
                errors.append(f"{relative}: ソースを読み取れません: {error}")
                continue
            for delimiter, include in INCLUDE_PATTERN.findall(text):
                normalized_include = normalize(include).casefold()
                targets = resolve_include(relative, delimiter, include, paths, include_index)
                application_targets = sorted(target for target in targets if "Application" in owner_for(target, modules))
                if normalized_include.startswith("applicationlayer/") or application_targets:
                    details = f" (候補: {', '.join(application_targets)})" if application_targets else ""
                    errors.append(f"{relative}: {owner}からApplicationへの逆依存は禁止です: {include}{details}")

    return errors


def main() -> int:
    errors = validate()
    if errors:
        print(f"Engine module validation failed: {len(errors)} issue(s)")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("Engine module validation passed: source ownership and Engine-to-Application include boundaries checked.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
