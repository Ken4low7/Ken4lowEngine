import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve().parents[2] / "Tools" / "Scripts" / "BuildAssetPackages.py"
SPEC = importlib.util.spec_from_file_location("build_asset_packages", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class AssetPackageTests(unittest.TestCase):
    def make_project(self, root: Path) -> tuple[Path, dict, dict]:
        project = root / "Project"
        outputs = {
            "font": "Resources/Fonts/Compiled/UI/font.bin",
            "ui": "Resources/Textures/Compiled/UI/hud.dds",
            "stage": "Resources/Models/Compiled/Stages/arena.kmesh",
            "core": "Resources/Textures/Compiled/World/stone.dds",
        }
        for name, relative in outputs.items():
            path = project / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((name + "-payload").encode("utf-8"))

        def asset(asset_id: str, asset_type: str, logical_key: str, output: str) -> dict:
            return {
                "AssetId": asset_id,
                "AssetType": asset_type,
                "LogicalKey": logical_key,
                "BuildKey": (asset_id * 64)[:64],
                "OutputPaths": [output],
                "MissingOutputs": [],
            }

        manifest = {
            "ManifestVersion": 1,
            "MissingOutputCount": 0,
            "Assets": [
                asset("a", "Font", "Resources/Fonts/Sources/UI/font.ttf#JP", outputs["font"]),
                asset("b", "Texture", "Resources/Textures/Sources/UI/hud.png", outputs["ui"]),
                asset("c", "Mesh", "Resources/Models/Sources/Stages/arena.gltf", outputs["stage"]),
                asset("d", "Texture", "Resources/Textures/Sources/World/stone.png", outputs["core"]),
            ],
        }
        config = {
            "ChunkConfigVersion": 1,
            "DefaultChunk": "core",
            "Rules": [
                {"Chunk": "ui", "AssetTypes": ["Font"]},
                {"Chunk": "ui", "LogicalKeyPrefixes": ["Resources/Textures/Sources/UI/"]},
                {"Chunk": "world", "LogicalKeyPrefixes": ["Resources/Models/Sources/Stages/"]},
            ],
            "ChunkDependencies": {"ui": ["core"], "world": ["core"]},
            "OwnerBindings": [{"Owner": "TestWorld:Cell_0_0", "Chunks": ["core", "world"]}],
        }
        return project, manifest, config

    def test_chunk_assignment_preserves_asset_ids_and_dependencies(self):
        with tempfile.TemporaryDirectory() as temporary:
            project, manifest, config = self.make_project(Path(temporary))
            output_root = Path(temporary) / "Generated" / "Packages"
            package_manifest = MODULE.build_packages(project, manifest, config, output_root)

            # AssetId remains manifest-owned and never depends on the physical package path.
            self.assertEqual(package_manifest["AssetToChunk"]["a"], "ui")
            self.assertEqual(package_manifest["AssetToChunk"]["b"], "ui")
            self.assertEqual(package_manifest["AssetToChunk"]["c"], "world")
            self.assertEqual(package_manifest["AssetToChunk"]["d"], "core")
            chunks = {entry["ChunkId"]: entry for entry in package_manifest["Chunks"]}
            self.assertEqual(chunks["ui"]["Dependencies"], ["core"])
            self.assertEqual(chunks["world"]["Dependencies"], ["core"])
            self.assertEqual(package_manifest["OwnerBindings"][0]["Chunks"], ["core", "world"])

    def test_package_bytes_are_deterministic_and_contain_runtime_outputs_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            project, manifest, config = self.make_project(Path(temporary))
            output_root = Path(temporary) / "Generated" / "Packages"

            first = MODULE.build_packages(project, manifest, config, output_root)
            first_manifest_bytes = (output_root / "PackageManifest.json").read_bytes()
            first_hashes = {chunk["ChunkId"]: chunk["PackageSha256"] for chunk in first["Chunks"]}

            second = MODULE.build_packages(project, manifest, config, output_root)
            second_manifest_bytes = (output_root / "PackageManifest.json").read_bytes()
            second_hashes = {chunk["ChunkId"]: chunk["PackageSha256"] for chunk in second["Chunks"]}

            self.assertEqual(first_manifest_bytes, second_manifest_bytes)
            self.assertEqual(first_hashes, second_hashes)

            with zipfile.ZipFile(output_root / "Chunks" / "ui.kpak", "r") as archive:
                names = sorted(archive.namelist())
                self.assertIn("ChunkManifest.json", names)
                self.assertIn("Content/Resources/Fonts/Compiled/UI/font.bin", names)
                self.assertIn("Content/Resources/Textures/Compiled/UI/hud.dds", names)
                self.assertFalse(any("Sources" in name for name in names))
                chunk_manifest = json.loads(archive.read("ChunkManifest.json").decode("utf-8"))
                self.assertEqual(chunk_manifest["ChunkId"], "ui")
                self.assertEqual(chunk_manifest["AssetCount"], 2)

    def test_dependency_cycle_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            project, manifest, config = self.make_project(Path(temporary))
            config["ChunkDependencies"] = {"core": ["ui"], "ui": ["core"]}
            with self.assertRaises(ValueError):
                MODULE.build_packages(project, manifest, config, Path(temporary) / "Generated" / "Packages")

    def test_missing_cooked_output_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            project, manifest, config = self.make_project(Path(temporary))
            missing = project / manifest["Assets"][0]["OutputPaths"][0]
            missing.unlink()
            with self.assertRaises(FileNotFoundError):
                MODULE.build_packages(project, manifest, config, Path(temporary) / "Generated" / "Packages")


if __name__ == "__main__":
    unittest.main()
