#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
from typing import Any, Iterable

import BuildAssetManifest
import BuildAssetPackages

SNAPSHOT_VERSION = 1
REPORT_VERSION = 1
COOKER_ORDER = ("Font", "Texture", "Mesh")
PACKAGE_CONFIG_PATH = "Config/AssetChunks.json"
SOURCE_ROOTS = (
    ("Resources/Fonts/Sources", "Font"),
    ("Resources/Fonts/Charsets", "Font"),
    ("Resources/Textures/Sources", "Texture"),
    ("Resources/Models/Sources", "Mesh"),
)
COOKER_BATCH_FILES = {
    "Font": "RunBuildFonts.bat",
    "Texture": "RunBuildTextures.bat",
    "Mesh": "RunBuildMeshes.bat",
}


def normalize_path(value: str) -> str:
    return value.replace("\\", "/").lstrip("./")


def project_relative_path(project_dir: Path, value: str | Path) -> str:
    path = Path(value)
    if not path.is_absolute():
        return normalize_path(path.as_posix())
    try:
        return normalize_path(path.resolve().relative_to(project_dir.resolve()).as_posix())
    except ValueError:
        return normalize_path(path.as_posix())


def sha256_file(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def file_state(project_dir: Path, relative_path: str) -> dict[str, Any]:
    absolute = project_dir / relative_path
    if not absolute.is_file():
        return {"Exists": False, "SizeBytes": 0, "Sha256": ""}
    return {
        "Exists": True,
        "SizeBytes": absolute.stat().st_size,
        "Sha256": sha256_file(absolute),
    }


def infer_asset_type(relative_path: str) -> str | None:
    normalized = normalize_path(relative_path).lower()
    for root, asset_type in SOURCE_ROOTS:
        normalized_root = normalize_path(root).lower().rstrip("/") + "/"
        if normalized == normalized_root[:-1] or normalized.startswith(normalized_root):
            return asset_type
    return None


def discover_source_paths(project_dir: Path) -> set[str]:
    paths: set[str] = set()
    for root_text, _ in SOURCE_ROOTS:
        root = project_dir / root_text
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if path.is_file():
                paths.add(normalize_path(path.relative_to(project_dir).as_posix()))
    package_config = project_dir / PACKAGE_CONFIG_PATH
    if package_config.is_file():
        paths.add(PACKAGE_CONFIG_PATH)
    return paths


def tracked_paths(manifest: dict[str, Any], project_dir: Path) -> list[str]:
    paths = set(discover_source_paths(project_dir))
    reverse = manifest.get("DependencyGraph", {}).get("Reverse", {})
    paths.update(normalize_path(path) for path in reverse.keys())
    return sorted(paths, key=str.lower)


def build_snapshot(project_dir: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    files = {path: file_state(project_dir, path) for path in tracked_paths(manifest, project_dir)}
    return {
        "SnapshotVersion": SNAPSHOT_VERSION,
        "Files": files,
    }


def load_json(path: Path) -> dict[str, Any] | None:
    if not path.is_file():
        return None
    with path.open("r", encoding="utf-8-sig") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"Expected JSON object: {path}")
    return value


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def diff_snapshots(previous: dict[str, Any] | None, current: dict[str, Any]) -> list[str]:
    current_files = current.get("Files", {})
    if previous is None:
        return sorted(current_files.keys(), key=str.lower)

    previous_files = previous.get("Files", {})
    changed: list[str] = []
    for path in sorted(set(previous_files) | set(current_files), key=str.lower):
        if previous_files.get(path) != current_files.get(path):
            changed.append(path)
    return changed


def asset_index(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {
        str(asset.get("AssetId")): asset
        for asset in manifest.get("Assets", [])
        if isinstance(asset, dict) and asset.get("AssetId")
    }


def resolve_affected_asset_ids(manifest: dict[str, Any], changed_paths: Iterable[str]) -> list[str]:
    reverse = manifest.get("DependencyGraph", {}).get("Reverse", {})
    assets = asset_index(manifest)
    pending = [normalize_path(path) for path in changed_paths]
    visited_paths: set[str] = set()
    affected: set[str] = set()

    # Propagate changed dependencies through cooked outputs so derived assets rebuild transitively.
    while pending:
        path = pending.pop()
        if path in visited_paths:
            continue
        visited_paths.add(path)
        for asset_id in reverse.get(path, []):
            asset_id = str(asset_id)
            if asset_id in affected:
                continue
            affected.add(asset_id)
            asset = assets.get(asset_id, {})
            for output_path in asset.get("OutputPaths", []) or []:
                pending.append(normalize_path(str(output_path)))

    return sorted(affected)


def build_plan(manifest: dict[str, Any], changed_paths: Iterable[str]) -> dict[str, Any]:
    normalized_changed = sorted({normalize_path(path) for path in changed_paths if str(path).strip()}, key=str.lower)
    assets = asset_index(manifest)
    affected_ids = resolve_affected_asset_ids(manifest, normalized_changed)
    affected_assets = [assets[asset_id] for asset_id in affected_ids if asset_id in assets]

    cooker_types = {str(asset.get("AssetType", "")) for asset in affected_assets}
    untracked_changed: list[str] = []
    package_config_changed = PACKAGE_CONFIG_PATH in normalized_changed
    for path in normalized_changed:
        direct_ids = manifest.get("DependencyGraph", {}).get("Reverse", {}).get(path, [])
        if direct_ids:
            continue
        inferred = infer_asset_type(path)
        if inferred:
            cooker_types.add(inferred)
            untracked_changed.append(path)

    cookers = [asset_type for asset_type in COOKER_ORDER if asset_type in cooker_types]
    return {
        "ReportVersion": REPORT_VERSION,
        "ChangedPaths": normalized_changed,
        "AffectedAssetCount": len(affected_assets),
        "AffectedAssets": [
            {
                "AssetId": asset.get("AssetId", ""),
                "AssetType": asset.get("AssetType", "Unknown"),
                "LogicalKey": asset.get("LogicalKey", ""),
                "BuildKey": asset.get("BuildKey", ""),
                "OutputPaths": asset.get("OutputPaths", []),
            }
            for asset in sorted(
                affected_assets,
                key=lambda value: (str(value.get("AssetType", "")).lower(), str(value.get("LogicalKey", "")).lower()),
            )
        ],
        "Cookers": cookers,
        "PackageConfigChanged": package_config_changed,
        "RequiresPackaging": bool(cookers) or package_config_changed,
        "UntrackedChangedPaths": sorted(untracked_changed, key=str.lower),
    }


def load_or_build_manifest(project_dir: Path, manifest_path: Path) -> dict[str, Any]:
    manifest = load_json(manifest_path)
    if manifest is not None:
        return manifest
    return BuildAssetManifest.write_manifest(project_dir, manifest_path)


def run_batch(project_dir: Path, configuration: str, batch_name: str) -> None:
    if os.name != "nt":
        raise RuntimeError("Cooker execution requires Windows; use planning mode on other platforms.")
    batch_path = project_dir / "Tools" / "Scripts" / batch_name
    if not batch_path.is_file():
        raise FileNotFoundError(f"Build batch not found: {batch_path}")
    command = ["cmd.exe", "/d", "/s", "/c", str(batch_path), str(project_dir), configuration, "--no-pause"]
    result = subprocess.run(command, cwd=project_dir, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"{batch_name} failed with exit code {result.returncode}")


def execute_plan(project_dir: Path, configuration: str, plan: dict[str, Any]) -> None:
    for cooker in plan.get("Cookers", []):
        batch_name = COOKER_BATCH_FILES.get(str(cooker))
        if not batch_name:
            raise RuntimeError(f"Unsupported cooker type: {cooker}")
        print(f"[IncrementalAssets] Run cooker: {cooker}")
        run_batch(project_dir, configuration, batch_name)


def rebuild_packages(project_dir: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    config_path = project_dir / PACKAGE_CONFIG_PATH
    config = BuildAssetPackages.load_chunk_config(config_path)
    output_root = project_dir.parent / "Generated" / "Packages"
    # Incremental cook keeps distributable chunks synchronized with the newly written manifest.
    return BuildAssetPackages.build_packages(project_dir, manifest, config, output_root)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Resolve and optionally build assets affected by changed dependencies.")
    parser.add_argument("--project-dir", default=".", help="Path to the Project directory.")
    parser.add_argument("--configuration", default="Debug", help="Debug or Release cooker configuration.")
    parser.add_argument("--manifest", default=None, help="Optional AssetManifest.json path.")
    parser.add_argument("--snapshot", default=None, help="Optional dependency snapshot path.")
    parser.add_argument("--report", default=None, help="Optional incremental report path.")
    parser.add_argument("--changed", action="append", default=[], help="Explicit changed project-relative path. Repeatable.")
    parser.add_argument("--execute", action="store_true", help="Run only the cooker categories required by the plan.")
    parser.add_argument("--write-snapshot", action="store_true", help="Save the current snapshot even in planning mode.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_dir = Path(args.project_dir).resolve()
    generated_root = project_dir.parent / "Generated" / "AssetPipeline"
    manifest_path = Path(args.manifest).resolve() if args.manifest else generated_root / "AssetManifest.json"
    snapshot_path = Path(args.snapshot).resolve() if args.snapshot else generated_root / "DependencySnapshot.json"
    report_path = Path(args.report).resolve() if args.report else generated_root / "IncrementalBuildReport.json"

    manifest = load_or_build_manifest(project_dir, manifest_path)
    previous_snapshot = load_json(snapshot_path)
    current_snapshot = build_snapshot(project_dir, manifest)
    detected_changed = diff_snapshots(previous_snapshot, current_snapshot)
    explicit_changed = [project_relative_path(project_dir, value) for value in args.changed]
    changed_paths = sorted(set(detected_changed) | set(explicit_changed), key=str.lower)
    plan = build_plan(manifest, changed_paths)
    plan["DetectedChangedPaths"] = detected_changed
    plan["ExplicitChangedPaths"] = sorted(set(explicit_changed), key=str.lower)
    plan["SnapshotWasMissing"] = previous_snapshot is None
    package_manifest_path = project_dir.parent / "Generated" / "Packages" / "PackageManifest.json"
    if not package_manifest_path.is_file():
        plan["RequiresPackaging"] = True
        plan["PackageOutputWasMissing"] = True
    else:
        plan["PackageOutputWasMissing"] = False
    write_json(report_path, plan)

    print(
        f"[IncrementalAssets] Changed={len(changed_paths)} AffectedAssets={plan['AffectedAssetCount']} "
        f"Cookers={','.join(plan['Cookers']) or 'none'} Package={plan['RequiresPackaging']}"
    )
    for path in changed_paths:
        print(f"[IncrementalAssets] Changed: {path}")
    for asset in plan["AffectedAssets"]:
        print(f"[IncrementalAssets] Affected: {asset['AssetType']} {asset['LogicalKey']} ({asset['AssetId']})")

    if args.execute:
        execute_plan(project_dir, args.configuration, plan)
        # Rebuild topology after cook because generated outputs can introduce downstream dependencies.
        manifest = BuildAssetManifest.write_manifest(project_dir, manifest_path)
        if plan["RequiresPackaging"]:
            package_manifest = rebuild_packages(project_dir, manifest)
            print(f"[IncrementalAssets] Packages rebuilt: {package_manifest['ChunkCount']} chunks")
        current_snapshot = build_snapshot(project_dir, manifest)
        write_json(snapshot_path, current_snapshot)
        print(f"[IncrementalAssets] Snapshot updated: {snapshot_path}")
    elif args.write_snapshot:
        write_json(snapshot_path, current_snapshot)
        print(f"[IncrementalAssets] Snapshot written: {snapshot_path}")

    print(f"[IncrementalAssets] Report={report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
