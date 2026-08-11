import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve().parents[2] / "Tools" / "Scripts" / "BuildAssetManifest.py"
SPEC = importlib.util.spec_from_file_location("build_asset_manifest", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class AssetManifestTests(unittest.TestCase):
    def make_project(self, root: Path, create_output: bool = True, build_key: str | None = None) -> Path:
        project = root / "Project"
        source = project / "Resources" / "Models" / "Sources" / "Sample" / "cube.gltf"
        output = project / "Resources" / "Models" / "Compiled" / "Sample" / "cube.kmesh"
        meta = Path(str(output) + ".buildmeta.json")
        source.parent.mkdir(parents=True, exist_ok=True)
        output.parent.mkdir(parents=True, exist_ok=True)
        source.write_text("{}", encoding="utf-8")
        if create_output:
            output.write_bytes(b"KMESH")

        source_hash = MODULE.hashlib.sha256(source.read_bytes()).hexdigest()
        build_meta = {
            "BuildVersion": 1,
            "AssetType": "Mesh",
            "SourcePath": "Resources/Models/Sources/Sample/cube.gltf",
            "OutputPath": "Resources/Models/Compiled/Sample/cube.kmesh",
            "DependencyFingerprint": source_hash,
            "Dependencies": [
                {
                    "Path": "Resources/Models/Sources/Sample/cube.gltf",
                    "SizeBytes": source.stat().st_size,
                    "Sha256": source_hash,
                }
            ],
        }
        if build_key is not None:
            build_meta["BuildKey"] = build_key
        meta.write_text(json.dumps(build_meta), encoding="utf-8")
        return project

    def test_manifest_is_deterministic_and_builds_reverse_dependency_graph(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = self.make_project(Path(temporary))
            first = MODULE.build_manifest(project)
            second = MODULE.build_manifest(project)

            self.assertEqual(first, second)
            self.assertEqual(first["AssetCount"], 1)
            self.assertEqual(first["MissingOutputCount"], 0)
            asset = first["Assets"][0]
            self.assertEqual(asset["AssetType"], "Mesh")
            self.assertEqual(asset["LogicalKey"], "Resources/Models/Sources/Sample/cube.gltf")
            self.assertEqual(len(asset["AssetId"]), 16)
            self.assertEqual(len(asset["BuildKey"]), 64)
            # Reverse dependency lookup is the foundation for incremental rebuild invalidation.
            self.assertEqual(
                first["DependencyGraph"]["Reverse"]["Resources/Models/Sources/Sample/cube.gltf"],
                [asset["AssetId"]],
            )

    def test_missing_outputs_are_reported(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = self.make_project(Path(temporary), create_output=False)
            manifest = MODULE.build_manifest(project)
            self.assertEqual(manifest["MissingOutputCount"], 1)
            self.assertEqual(
                manifest["Assets"][0]["MissingOutputs"],
                ["Resources/Models/Compiled/Sample/cube.kmesh"],
            )

    def test_declared_ddc_build_key_is_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            expected = "ab" * 32
            project = self.make_project(Path(temporary), build_key=expected)
            manifest = MODULE.build_manifest(project)
            self.assertEqual(manifest["Assets"][0]["BuildKey"], expected)

    def test_invalid_declared_build_key_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = self.make_project(Path(temporary), build_key="not-a-sha256")
            with self.assertRaises(ValueError):
                MODULE.build_manifest(project)


if __name__ == "__main__":
    unittest.main()
