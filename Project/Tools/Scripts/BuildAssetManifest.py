#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

MANIFEST_VERSION = 1


def normalize_path(value: str) -> str:
    return value.replace("\\", "/").lstrip("./")


def fnv1a64(text: str) -> str:
    value = 0xCBF29CE484222325
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{value:016x}"


def canonical_sha256(value: Any) -> str:
    encoded = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def dependency_record(project_dir: Path, path_text: str, sha256: str | None = None, size_bytes: int | None = None) -> dict[str, Any]:
    normalized = normalize_path(path_text)
    absolute = project_dir / normalized
    if size_bytes is None and absolute.is_file():
        size_bytes = absolute.stat().st_size
    if sha256 is None and absolute.is_file():
        sha256 = hashlib.sha256(absolute.read_bytes()).hexdigest()
    return {
        "Path": normalized,
        "SizeBytes": int(size_bytes or 0),
        "Sha256": sha256 or "",
    }


def collect_dependencies(project_dir: Path, meta: dict[str, Any]) -> list[dict[str, Any]]:
    asset_type = str(meta.get("AssetType", "Unknown"))
    dependencies: list[dict[str, Any]] = []

    if isinstance(meta.get("Dependencies"), list):
        for entry in meta["Dependencies"]:
            if not isinstance(entry, dict) or not entry.get("Path"):
                continue
            dependencies.append(
                dependency_record(
                    project_dir,
                    str(entry["Path"]),
                    str(entry.get("Sha256", "")) or None,
                    int(entry.get("SizeBytes", 0)),
                )
            )
    elif asset_type == "Texture" and meta.get("SourcePath"):
        dependencies.append(
            dependency_record(
                project_dir,
                str(meta["SourcePath"]),
                str(meta.get("SourceSha256", "")) or None,
                int(meta.get("SourceSizeBytes", 0)),
            )
        )
    elif asset_type == "Font":
        if meta.get("FontPath"):
            dependencies.append(dependency_record(project_dir, str(meta["FontPath"]), str(meta.get("FontSha256", "")) or None))
        if meta.get("CharsetPath"):
            dependencies.append(dependency_record(project_dir, str(meta["CharsetPath"]), str(meta.get("CharsetSha256", "")) or None))

    # Dependency order is stabilized so BuildKey does not depend on filesystem enumeration order.
    unique = {entry["Path"]: entry for entry in dependencies}
    return [unique[path] for path in sorted(unique)]


def collect_outputs(meta: dict[str, Any]) -> list[str]:
    outputs: list[str] = []
    if meta.get("OutputPath"):
        outputs.append(normalize_path(str(meta["OutputPath"])))
    for path in meta.get("OutputPaths", []) or []:
        outputs.append(normalize_path(str(path)))
    return sorted(set(outputs))


def logical_key(meta_path: Path, meta: dict[str, Any], project_dir: Path) -> str:
    asset_type = str(meta.get("AssetType", "Unknown"))
    if meta.get("SourcePath"):
        base = normalize_path(str(meta["SourcePath"]))
    elif meta.get("FontPath"):
        base = normalize_path(str(meta["FontPath"]))
    else:
        base = normalize_path(meta_path.relative_to(project_dir).as_posix())

    variant = str(meta.get("Variant", "")).strip()
    return f"{base}#{variant}" if variant else base


def build_asset_record(project_dir: Path, meta_path: Path, meta: dict[str, Any]) -> dict[str, Any]:
    asset_type = str(meta.get("AssetType", "Unknown"))
    key = logical_key(meta_path, meta, project_dir)
    dependencies = collect_dependencies(project_dir, meta)
    outputs = collect_outputs(meta)

    missing_outputs = [path for path in outputs if not (project_dir / path).is_file()]
    build_key_payload = {
        "AssetType": asset_type,
        "BuildVersion": int(meta.get("BuildVersion", 0)),
        "LogicalKey": key,
        "Dependencies": dependencies,
        "Outputs": outputs,
        "BuildMeta": meta,
    }

    return {
        "AssetId": fnv1a64(f"{asset_type.lower()}:{key.lower()}"),
        "AssetType": asset_type,
        "LogicalKey": key,
        "BuildVersion": int(meta.get("BuildVersion", 0)),
        "BuildKey": canonical_sha256(build_key_payload),
        "MetaPath": normalize_path(meta_path.relative_to(project_dir).as_posix()),
        "Dependencies": dependencies,
        "OutputPaths": outputs,
        "MissingOutputs": missing_outputs,
    }


def discover_build_meta(project_dir: Path) -> list[Path]:
    resources = project_dir / "Resources"
    if not resources.is_dir():
        raise FileNotFoundError(f"Resources directory not found: {resources}")
    return sorted(resources.rglob("*.buildmeta.json"), key=lambda path: path.as_posix().lower())


def build_manifest(project_dir: Path) -> dict[str, Any]:
    records: list[dict[str, Any]] = []
    for meta_path in discover_build_meta(project_dir):
        with meta_path.open("r", encoding="utf-8-sig") as stream:
            meta = json.load(stream)
        records.append(build_asset_record(project_dir, meta_path, meta))

    records.sort(key=lambda record: (record["AssetType"].lower(), record["LogicalKey"].lower()))
    ids = [record["AssetId"] for record in records]
    if len(ids) != len(set(ids)):
        raise RuntimeError("AssetId collision detected while building manifest")

    forward: dict[str, list[str]] = {}
    reverse: dict[str, list[str]] = {}
    for record in records:
        paths = [entry["Path"] for entry in record["Dependencies"]]
        forward[record["AssetId"]] = paths
        for path in paths:
            reverse.setdefault(path, []).append(record["AssetId"])

    reverse = {path: sorted(asset_ids) for path, asset_ids in sorted(reverse.items())}
    missing_count = sum(len(record["MissingOutputs"]) for record in records)
    return {
        "ManifestVersion": MANIFEST_VERSION,
        "AssetCount": len(records),
        "MissingOutputCount": missing_count,
        "Assets": records,
        "DependencyGraph": {
            "Forward": dict(sorted(forward.items())),
            "Reverse": reverse,
        },
    }


def write_manifest(project_dir: Path, output_path: Path) -> dict[str, Any]:
    manifest = build_manifest(project_dir)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build the deterministic Ken4lowEngine asset manifest and dependency graph.")
    parser.add_argument("--project-dir", default=".", help="Path to the Project directory.")
    parser.add_argument("--output", default=None, help="Optional manifest output path.")
    parser.add_argument("--fail-on-missing-output", action="store_true", help="Return non-zero when build metadata references missing outputs.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_dir = Path(args.project_dir).resolve()
    output_path = Path(args.output).resolve() if args.output else project_dir.parent / "Generated" / "AssetPipeline" / "AssetManifest.json"
    manifest = write_manifest(project_dir, output_path)
    print(f"[AssetManifest] Assets={manifest['AssetCount']} MissingOutputs={manifest['MissingOutputCount']}")
    print(f"[AssetManifest] Output={output_path}")
    if args.fail_on_missing_output and manifest["MissingOutputCount"] > 0:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
