#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
from typing import Any

import BuildAssetManifest

PACKAGE_VERSION = 1
CHUNK_MANIFEST_VERSION = 1
CHUNK_CONFIG_VERSION = 1
FIXED_ZIP_TIME = (1980, 1, 1, 0, 0, 0)


def normalize_path(value: str) -> str:
    return value.replace("\\", "/").lstrip("./")


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_file(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def load_json(path: Path) -> dict[str, Any] | None:
    if not path.is_file():
        return None
    with path.open("r", encoding="utf-8-sig") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"Expected JSON object: {path}")
    return value


def default_chunk_config() -> dict[str, Any]:
    return {
        "ChunkConfigVersion": CHUNK_CONFIG_VERSION,
        "DefaultChunk": "core",
        "Rules": [],
        "ChunkDependencies": {},
        "OwnerBindings": [],
    }


def load_chunk_config(path: Path | None) -> dict[str, Any]:
    if path is None or not path.is_file():
        return default_chunk_config()
    config = load_json(path)
    assert config is not None
    if int(config.get("ChunkConfigVersion", 0)) != CHUNK_CONFIG_VERSION:
        raise ValueError(f"Unsupported ChunkConfigVersion: {config.get('ChunkConfigVersion')}")
    if not str(config.get("DefaultChunk", "")).strip():
        raise ValueError("DefaultChunk must not be empty")
    config.setdefault("Rules", [])
    config.setdefault("ChunkDependencies", {})
    config.setdefault("OwnerBindings", [])
    return config


def safe_chunk_id(value: str) -> str:
    chunk = value.strip().lower().replace(" ", "-")
    if not chunk or any(character not in "abcdefghijklmnopqrstuvwxyz0123456789-_." for character in chunk):
        raise ValueError(f"Invalid chunk id: {value}")
    return chunk


def rule_matches(asset: dict[str, Any], rule: dict[str, Any]) -> bool:
    asset_id = str(asset.get("AssetId", ""))
    asset_type = str(asset.get("AssetType", ""))
    logical_key = normalize_path(str(asset.get("LogicalKey", "")))
    outputs = [normalize_path(str(path)) for path in asset.get("OutputPaths", []) or []]

    if rule.get("AssetIds") and asset_id in {str(value) for value in rule.get("AssetIds", [])}:
        return True
    if rule.get("AssetTypes") and asset_type in {str(value) for value in rule.get("AssetTypes", [])}:
        return True
    if any(logical_key.startswith(normalize_path(str(prefix))) for prefix in rule.get("LogicalKeyPrefixes", []) or []):
        return True
    if any(
        output.startswith(normalize_path(str(prefix)))
        for output in outputs
        for prefix in rule.get("OutputPathPrefixes", []) or []
    ):
        return True
    return False


def assign_chunk(asset: dict[str, Any], config: dict[str, Any]) -> str:
    for rule in config.get("Rules", []) or []:
        if not isinstance(rule, dict) or not rule.get("Chunk"):
            continue
        if rule_matches(asset, rule):
            return safe_chunk_id(str(rule["Chunk"]))
    return safe_chunk_id(str(config.get("DefaultChunk", "core")))


def validate_chunk_dependencies(config: dict[str, Any], chunk_ids: set[str]) -> dict[str, list[str]]:
    dependencies: dict[str, list[str]] = {}
    for raw_chunk, raw_dependencies in (config.get("ChunkDependencies", {}) or {}).items():
        chunk = safe_chunk_id(str(raw_chunk))
        values = sorted({safe_chunk_id(str(value)) for value in raw_dependencies or []})
        if chunk in values:
            raise ValueError(f"Chunk cannot depend on itself: {chunk}")
        dependencies[chunk] = values
        chunk_ids.add(chunk)
        chunk_ids.update(values)

    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(chunk: str) -> None:
        if chunk in visiting:
            raise ValueError(f"Chunk dependency cycle detected at: {chunk}")
        if chunk in visited:
            return
        visiting.add(chunk)
        for dependency in dependencies.get(chunk, []):
            visit(dependency)
        visiting.remove(chunk)
        visited.add(chunk)

    for chunk in sorted(chunk_ids):
        visit(chunk)
    return dependencies


def normalize_owner_bindings(config: dict[str, Any], chunk_ids: set[str]) -> list[dict[str, Any]]:
    bindings: list[dict[str, Any]] = []
    for entry in config.get("OwnerBindings", []) or []:
        if not isinstance(entry, dict) or not str(entry.get("Owner", "")).strip():
            continue
        chunks = sorted({safe_chunk_id(str(value)) for value in entry.get("Chunks", []) or []})
        unknown = [chunk for chunk in chunks if chunk not in chunk_ids]
        if unknown:
            raise ValueError(f"Owner binding references unknown chunks: {entry.get('Owner')} -> {unknown}")
        bindings.append({"Owner": str(entry["Owner"]), "Chunks": chunks})
    return sorted(bindings, key=lambda value: value["Owner"].lower())


def zip_info(name: str) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(filename=normalize_path(name), date_time=FIXED_ZIP_TIME)
    info.compress_type = zipfile.ZIP_STORED
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    return info


def write_deterministic_package(package_path: Path, entries: dict[str, bytes]) -> None:
    package_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = package_path.with_suffix(package_path.suffix + ".tmp")
    if temporary.exists():
        temporary.unlink()
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED, allowZip64=True) as archive:
        # Package entry order and timestamps are fixed so identical cooked content produces identical bytes.
        for name in sorted(entries, key=str.lower):
            archive.writestr(zip_info(name), entries[name])
    temporary.replace(package_path)


def collect_chunk_assets(manifest: dict[str, Any], config: dict[str, Any]) -> dict[str, list[dict[str, Any]]]:
    chunks: dict[str, list[dict[str, Any]]] = {}
    for asset in manifest.get("Assets", []) or []:
        if not isinstance(asset, dict):
            continue
        if asset.get("MissingOutputs"):
            raise RuntimeError(f"Cannot package asset with missing outputs: {asset.get('LogicalKey')}")
        chunk = assign_chunk(asset, config)
        chunks.setdefault(chunk, []).append(asset)
    for assets in chunks.values():
        assets.sort(key=lambda value: (str(value.get("AssetType", "")).lower(), str(value.get("LogicalKey", "")).lower()))
    return dict(sorted(chunks.items()))


def build_chunk(project_dir: Path, output_root: Path, chunk_id: str, assets: list[dict[str, Any]], dependencies: list[str]) -> dict[str, Any]:
    files: dict[str, dict[str, Any]] = {}
    package_entries: dict[str, bytes] = {}
    packaged_assets: list[dict[str, Any]] = []

    for asset in assets:
        asset_files: list[str] = []
        for output_text in asset.get("OutputPaths", []) or []:
            source_path_text = normalize_path(str(output_text))
            source_path = project_dir / source_path_text
            if not source_path.is_file():
                raise FileNotFoundError(f"Cooked output missing during packaging: {source_path}")
            package_path = normalize_path(f"Content/{source_path_text}")
            payload = source_path.read_bytes()
            payload_hash = sha256_bytes(payload)
            existing = files.get(package_path)
            if existing and existing["Sha256"] != payload_hash:
                raise RuntimeError(f"Package path collision with different content: {package_path}")
            files[package_path] = {
                "SourcePath": source_path_text,
                "PackagePath": package_path,
                "SizeBytes": len(payload),
                "Sha256": payload_hash,
            }
            package_entries[package_path] = payload
            asset_files.append(package_path)

        packaged_assets.append(
            {
                "AssetId": str(asset.get("AssetId", "")),
                "AssetType": str(asset.get("AssetType", "Unknown")),
                "LogicalKey": str(asset.get("LogicalKey", "")),
                "BuildKey": str(asset.get("BuildKey", "")),
                "Files": sorted(asset_files, key=str.lower),
            }
        )

    file_records = [files[name] for name in sorted(files, key=str.lower)]
    chunk_manifest = {
        "ChunkManifestVersion": CHUNK_MANIFEST_VERSION,
        "ChunkId": chunk_id,
        "Dependencies": sorted(dependencies),
        "AssetCount": len(packaged_assets),
        "FileCount": len(file_records),
        "TotalBytes": sum(int(record["SizeBytes"]) for record in file_records),
        "Assets": packaged_assets,
        "Files": file_records,
    }
    chunk_manifest_bytes = canonical_json_bytes(chunk_manifest)
    package_entries["ChunkManifest.json"] = chunk_manifest_bytes

    package_path = output_root / "Chunks" / f"{chunk_id}.kpak"
    write_deterministic_package(package_path, package_entries)
    manifest_path = output_root / "Chunks" / f"{chunk_id}.manifest.json"
    manifest_path.write_bytes(chunk_manifest_bytes)

    return {
        "ChunkId": chunk_id,
        "Dependencies": sorted(dependencies),
        "AssetCount": len(packaged_assets),
        "FileCount": len(file_records),
        "TotalBytes": chunk_manifest["TotalBytes"],
        "PackagePath": normalize_path(package_path.relative_to(output_root.parent).as_posix()),
        "ManifestPath": normalize_path(manifest_path.relative_to(output_root.parent).as_posix()),
        "PackageSizeBytes": package_path.stat().st_size,
        "PackageSha256": sha256_file(package_path),
    }


def build_packages(project_dir: Path, manifest: dict[str, Any], config: dict[str, Any], output_root: Path) -> dict[str, Any]:
    chunks = collect_chunk_assets(manifest, config)
    chunk_ids = set(chunks)
    chunk_dependencies = validate_chunk_dependencies(config, chunk_ids)
    for chunk_id in chunk_ids:
        chunks.setdefault(chunk_id, [])

    if output_root.exists():
        shutil.rmtree(output_root)
    (output_root / "Chunks").mkdir(parents=True, exist_ok=True)

    chunk_records = [
        build_chunk(project_dir, output_root, chunk_id, chunks.get(chunk_id, []), chunk_dependencies.get(chunk_id, []))
        for chunk_id in sorted(chunk_ids)
    ]
    asset_to_chunk = {
        str(asset.get("AssetId", "")): chunk_id
        for chunk_id, assets in sorted(chunks.items())
        for asset in assets
        if asset.get("AssetId")
    }
    owner_bindings = normalize_owner_bindings(config, chunk_ids)
    config_hash = sha256_bytes(canonical_json_bytes(config))
    package_manifest = {
        "PackageVersion": PACKAGE_VERSION,
        "SourceManifestVersion": int(manifest.get("ManifestVersion", 0)),
        "ChunkConfigHash": config_hash,
        "ChunkCount": len(chunk_records),
        "AssetCount": len(asset_to_chunk),
        "FileCount": sum(int(chunk["FileCount"]) for chunk in chunk_records),
        "TotalCookedBytes": sum(int(chunk["TotalBytes"]) for chunk in chunk_records),
        "TotalPackageBytes": sum(int(chunk["PackageSizeBytes"]) for chunk in chunk_records),
        "Chunks": chunk_records,
        "AssetToChunk": dict(sorted(asset_to_chunk.items())),
        "OwnerBindings": owner_bindings,
    }
    (output_root / "PackageManifest.json").write_bytes(canonical_json_bytes(package_manifest))
    return package_manifest


def load_or_build_manifest(project_dir: Path, manifest_path: Path) -> dict[str, Any]:
    manifest = load_json(manifest_path)
    if manifest is not None:
        return manifest
    return BuildAssetManifest.write_manifest(project_dir, manifest_path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build deterministic runtime chunk packages from the Phase 8 asset manifest.")
    parser.add_argument("--project-dir", default=".", help="Path to the Project directory.")
    parser.add_argument("--manifest", default=None, help="Optional AssetManifest.json path.")
    parser.add_argument("--config", default=None, help="Optional AssetChunks.json path.")
    parser.add_argument("--output-root", default=None, help="Optional package output directory.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_dir = Path(args.project_dir).resolve()
    generated_root = project_dir.parent / "Generated"
    manifest_path = Path(args.manifest).resolve() if args.manifest else generated_root / "AssetPipeline" / "AssetManifest.json"
    config_path = Path(args.config).resolve() if args.config else project_dir / "Config" / "AssetChunks.json"
    output_root = Path(args.output_root).resolve() if args.output_root else generated_root / "Packages"

    manifest = load_or_build_manifest(project_dir, manifest_path)
    if int(manifest.get("MissingOutputCount", 0)) > 0:
        raise RuntimeError("AssetManifest contains missing cooked outputs; package build aborted.")
    config = load_chunk_config(config_path)
    package_manifest = build_packages(project_dir, manifest, config, output_root)
    print(
        f"[AssetPackages] Chunks={package_manifest['ChunkCount']} Assets={package_manifest['AssetCount']} "
        f"Files={package_manifest['FileCount']} CookedBytes={package_manifest['TotalCookedBytes']} "
        f"PackageBytes={package_manifest['TotalPackageBytes']}"
    )
    for chunk in package_manifest["Chunks"]:
        print(
            f"[AssetPackages] Chunk={chunk['ChunkId']} Assets={chunk['AssetCount']} Files={chunk['FileCount']} "
            f"Bytes={chunk['PackageSizeBytes']} Sha256={chunk['PackageSha256']}"
        )
    print(f"[AssetPackages] Output={output_root / 'PackageManifest.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
