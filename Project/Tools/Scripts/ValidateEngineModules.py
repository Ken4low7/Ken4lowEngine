from __future__ import annotations

import json
import re
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = PROJECT_ROOT / "Build/Modules/EngineModules.json"
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".inl"}
INCLUDE_PATTERN = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", re.MULTILINE)


def load_manifest() -> dict:
    with MANIFEST_PATH.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def normalize(value: str) -> str:
    return value.replace("\\", "/").strip("/")


def owner_for(relative_path: str, modules: list[dict]) -> list[str]:
    path = normalize(relative_path)
    owners: list[str] = []
    for module in modules:
        for root in module.get("Roots", []):
            root = normalize(root)
            if path == root or path.startswith(root + "/"):
                owners.append(module["Name"])
                break
    return owners


def collect_sources() -> list[Path]:
    files: list[Path] = []
    for root_name in ("Engine", "ApplicationLayer"):
        root = PROJECT_ROOT / root_name
        if not root.is_dir():
            continue
        files.extend(path for path in root.rglob("*") if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES)
    return files


def validate() -> list[str]:
    manifest = load_manifest()
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

    for source in collect_sources():
        relative = source.relative_to(PROJECT_ROOT).as_posix()
        owners = owner_for(relative, modules)
        if len(owners) != 1:
            errors.append(f"{relative}: Module所有者が{len(owners)}件です ({', '.join(owners)})")
            continue

        owner = owners[0]
        # Application層を下位Engine Moduleから直接参照する逆依存だけはPhase 5から禁止する。
        if owner != "Application":
            try:
                text = source.read_text(encoding="utf-8-sig", errors="replace")
            except OSError:
                continue
            for include in INCLUDE_PATTERN.findall(text):
                normalized_include = normalize(include)
                if normalized_include.startswith("ApplicationLayer/") or normalized_include.startswith("ApplicationLayer\\"):
                    errors.append(f"{relative}: {owner}からApplicationへの逆依存は禁止です: {include}")

    return errors


def main() -> int:
    errors = validate()
    if errors:
        print(f"Engine module validation failed: {len(errors)} issue(s)")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("Engine module validation passed: Core / Runtime / Editor / Application ownership is complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
