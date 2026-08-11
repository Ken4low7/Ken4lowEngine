#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
from typing import Any

import BuildAssetManifest
import BuildAssetPackages

VALIDATOR_VERSION = 1
COOKER_SCRIPTS = ("BuildFonts.ps1", "BuildTextures.ps1", "BuildMeshes.ps1")


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


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"Expected JSON object: {path}")
    return value


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_forced_cooker(project_dir: Path, configuration: str, script_name: str) -> None:
    if os.name != "nt":
        raise RuntimeError("Deterministic cook execution requires Windows.")
    script_path = project_dir / "Tools" / "Scripts" / script_name
    if not script_path.is_file():
        raise FileNotFoundError(f"Cooker script not found: {script_path}")

    command = [
        "powershell.exe",
        "-ExecutionPolicy",
        "Bypass",
        "-NoProfile",
        "-File",
        str(script_path),
        "-ProjectDir",
        str(project_dir),
        "-Configuration",
        configuration,
        "-Force",
        "-DisableDdc",
    ]
    print(f"[DeterministicCook] Run: {script_name}")
    result = subprocess.run(command, cwd=project_dir, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"{script_name} failed with exit code {result.returncode}")


def rebuild_runtime_content(
    project_dir: Path,
    configuration: str,
    manifest_path: Path,
    chunk_config_path: Path,
    package_root: Path,
) -> None:
    # Font atlases are texture sources, so deterministic full cooks always run Font before Texture.
    for script_name in COOKER_SCRIPTS:
        run_forced_cooker(project_dir, configuration, script_name)

    manifest = BuildAssetManifest.write_manifest(project_dir, manifest_path)
    if int(manifest.get("MissingOutputCount", 0)) > 0:
        raise RuntimeError("AssetManifest contains missing cooked outputs after forced cook.")
    chunk_config = BuildAssetPackages.load_chunk_config(chunk_config_path)
    BuildAssetPackages.build_packages(project_dir, manifest, chunk_config, package_root)


def capture_signature(project_dir: Path, manifest_path: Path, package_root: Path) -> dict[str, Any]:
    if not manifest_path.is_file():
        raise FileNotFoundError(f"Asset manifest not found: {manifest_path}")
    package_manifest_path = package_root / "PackageManifest.json"
    if not package_manifest_path.is_file():
        raise FileNotFoundError(f"Package manifest not found: {package_manifest_path}")

    manifest = load_json(manifest_path)
    package_manifest = load_json(package_manifest_path)
    assets: list[dict[str, Any]] = []
    for asset in manifest.get("Assets", []) or []:
        if not isinstance(asset, dict):
            continue
        outputs: list[dict[str, Any]] = []
        for output_text in sorted({str(value) for value in asset.get("OutputPaths", []) or []}, key=str.lower):
            output_path = project_dir / output_text
            if not output_path.is_file():
                raise FileNotFoundError(f"Cooked output missing during determinism capture: {output_path}")
            outputs.append(
                {
                    "Path": output_text.replace("\\", "/"),
                    "SizeBytes": output_path.stat().st_size,
                    "Sha256": sha256_file(output_path),
                }
            )
        assets.append(
            {
                "AssetId": str(asset.get("AssetId", "")),
                "AssetType": str(asset.get("AssetType", "Unknown")),
                "LogicalKey": str(asset.get("LogicalKey", "")),
                "BuildKey": str(asset.get("BuildKey", "")),
                "Outputs": outputs,
            }
        )
    assets.sort(key=lambda value: (value["AssetType"].lower(), value["LogicalKey"].lower(), value["AssetId"]))

    chunks: list[dict[str, Any]] = []
    for chunk in package_manifest.get("Chunks", []) or []:
        if not isinstance(chunk, dict):
            continue
        package_path = package_root.parent / str(chunk.get("PackagePath", ""))
        chunk_manifest_path = package_root.parent / str(chunk.get("ManifestPath", ""))
        if not package_path.is_file() or not chunk_manifest_path.is_file():
            raise FileNotFoundError(f"Package output missing during determinism capture: {chunk.get('ChunkId')}")
        actual_package_hash = sha256_file(package_path)
        declared_package_hash = str(chunk.get("PackageSha256", ""))
        if declared_package_hash and declared_package_hash != actual_package_hash:
            raise RuntimeError(f"Package hash mismatch for chunk {chunk.get('ChunkId')}")
        chunks.append(
            {
                "ChunkId": str(chunk.get("ChunkId", "")),
                "PackageSizeBytes": package_path.stat().st_size,
                "PackageSha256": actual_package_hash,
                "ManifestSha256": sha256_file(chunk_manifest_path),
            }
        )
    chunks.sort(key=lambda value: value["ChunkId"].lower())

    payload = {
        "ValidatorVersion": VALIDATOR_VERSION,
        "ManifestSha256": sha256_file(manifest_path),
        "PackageManifestSha256": sha256_file(package_manifest_path),
        "Assets": assets,
        "Chunks": chunks,
    }
    payload["SignatureSha256"] = sha256_bytes(canonical_json_bytes(payload))
    return payload


def diff_values(first: Any, second: Any, path: str = "") -> list[str]:
    if type(first) is not type(second):
        return [f"{path or '<root>'}: type {type(first).__name__} != {type(second).__name__}"]
    if isinstance(first, dict):
        differences: list[str] = []
        for key in sorted(set(first) | set(second), key=str):
            child = f"{path}.{key}" if path else str(key)
            if key not in first:
                differences.append(f"{child}: missing in pass 1")
            elif key not in second:
                differences.append(f"{child}: missing in pass 2")
            else:
                differences.extend(diff_values(first[key], second[key], child))
        return differences
    if isinstance(first, list):
        if len(first) != len(second):
            return [f"{path}: length {len(first)} != {len(second)}"]
        differences: list[str] = []
        for index, (left, right) in enumerate(zip(first, second)):
            differences.extend(diff_values(left, right, f"{path}[{index}]"))
        return differences
    if first != second:
        return [f"{path}: {first!r} != {second!r}"]
    return []


def validate_two_pass(
    project_dir: Path,
    configuration: str,
    manifest_path: Path,
    chunk_config_path: Path,
    package_root: Path,
    report_root: Path,
) -> dict[str, Any]:
    signatures: list[dict[str, Any]] = []
    for pass_index in (1, 2):
        print(f"[DeterministicCook] ===== PASS {pass_index} =====")
        rebuild_runtime_content(project_dir, configuration, manifest_path, chunk_config_path, package_root)
        signature = capture_signature(project_dir, manifest_path, package_root)
        signatures.append(signature)
        write_json(report_root / f"DeterminismPass{pass_index}.json", signature)
        print(f"[DeterministicCook] PASS {pass_index} Signature={signature['SignatureSha256']}")

    differences = diff_values(signatures[0], signatures[1])
    report = {
        "ValidatorVersion": VALIDATOR_VERSION,
        "Configuration": configuration,
        "Deterministic": not differences,
        "Pass1Signature": signatures[0]["SignatureSha256"],
        "Pass2Signature": signatures[1]["SignatureSha256"],
        "DifferenceCount": len(differences),
        "Differences": differences,
    }
    write_json(report_root / "DeterminismReport.json", report)
    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Force-cook the same Phase 8 inputs twice and verify bit-identical runtime content.")
    parser.add_argument("--project-dir", default=".", help="Path to the Project directory.")
    parser.add_argument("--configuration", default="Debug", help="Debug or Release cooker configuration.")
    parser.add_argument("--manifest", default=None, help="Optional AssetManifest.json path.")
    parser.add_argument("--chunk-config", default=None, help="Optional AssetChunks.json path.")
    parser.add_argument("--package-root", default=None, help="Optional package output directory.")
    parser.add_argument("--report-root", default=None, help="Optional determinism report directory.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_dir = Path(args.project_dir).resolve()
    generated_root = project_dir.parent / "Generated"
    manifest_path = Path(args.manifest).resolve() if args.manifest else generated_root / "AssetPipeline" / "AssetManifest.json"
    chunk_config_path = Path(args.chunk_config).resolve() if args.chunk_config else project_dir / "Config" / "AssetChunks.json"
    package_root = Path(args.package_root).resolve() if args.package_root else generated_root / "Packages"
    report_root = Path(args.report_root).resolve() if args.report_root else generated_root / "AssetPipeline"

    report = validate_two_pass(project_dir, args.configuration, manifest_path, chunk_config_path, package_root, report_root)
    if report["Deterministic"]:
        print(f"[DeterministicCook] PASS. Signature={report['Pass1Signature']}")
        print(f"[DeterministicCook] Report={report_root / 'DeterminismReport.json'}")
        return 0

    print(f"[DeterministicCook] FAILED. Differences={report['DifferenceCount']}")
    for difference in report["Differences"][:50]:
        print(f"[DeterministicCook] DIFF: {difference}")
    print(f"[DeterministicCook] Report={report_root / 'DeterminismReport.json'}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
