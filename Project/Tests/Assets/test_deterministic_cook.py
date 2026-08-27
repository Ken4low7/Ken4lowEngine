import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPTS_DIR = Path(__file__).resolve().parents[2] / "Tools" / "Scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))
SCRIPT_PATH = SCRIPTS_DIR / "ValidateDeterministicCook.py"
SPEC = importlib.util.spec_from_file_location("validate_deterministic_cook", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)

import BuildAssetPackages


class DeterministicCookTests(unittest.TestCase):
    def make_runtime_state(self, root: Path) -> tuple[Path, Path, Path, dict]:
        project = root / "Project"
        output_text = "Resources/Textures/Compiled/UI/hud.dds"
        output_path = project / output_text
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(b"HUD-RUNTIME-DATA")

        manifest = {
            "ManifestVersion": 1,
            "AssetCount": 1,
            "MissingOutputCount": 0,
            "Assets": [
                {
                    "AssetId": "asset-hud",
                    "AssetType": "Texture",
                    "LogicalKey": "Resources/Textures/Sources/UI/hud.png",
                    "BuildVersion": 1,
                    "BuildKey": "a" * 64,
                    "OutputPaths": [output_text],
                    "MissingOutputs": [],
                }
            ],
            "DependencyGraph": {"Forward": {}, "Reverse": {}},
        }
        generated = root / "Generated"
        manifest_path = generated / "AssetPipeline" / "AssetManifest.json"
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

        config = {
            "ChunkConfigVersion": 1,
            "DefaultChunk": "core",
            "Rules": [{"Chunk": "ui", "LogicalKeyPrefixes": ["Resources/Textures/Sources/UI/"]}],
            "ChunkDependencies": {"ui": ["core"]},
            "OwnerBindings": [],
        }
        package_root = generated / "Packages"
        BuildAssetPackages.build_packages(project, manifest, config, package_root)
        return project, manifest_path, package_root, manifest

    def test_same_runtime_state_produces_same_signature(self):
        with tempfile.TemporaryDirectory() as temporary:
            project, manifest_path, package_root, _ = self.make_runtime_state(Path(temporary))
            first = MODULE.capture_signature(project, manifest_path, package_root)
            second = MODULE.capture_signature(project, manifest_path, package_root)

            # The signature intentionally contains only project-relative identities and content hashes.
            self.assertEqual(first, second)
            self.assertEqual(MODULE.diff_values(first, second), [])
            self.assertEqual(first["SignatureSha256"], second["SignatureSha256"])

    def test_changed_cooked_bytes_are_reported(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            project, manifest_path, package_root, _ = self.make_runtime_state(root)
            first = MODULE.capture_signature(project, manifest_path, package_root)
            cooked = project / "Resources" / "Textures" / "Compiled" / "UI" / "hud.dds"
            cooked.write_bytes(b"HUD-RUNTIME-DATA-CHANGED")
            second = MODULE.capture_signature(project, manifest_path, package_root)

            differences = MODULE.diff_values(first, second)
            self.assertTrue(any("Assets" in value and "Sha256" in value for value in differences))
            self.assertNotEqual(first["SignatureSha256"], second["SignatureSha256"])

    def test_build_key_change_is_part_of_deterministic_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            project, manifest_path, package_root, manifest = self.make_runtime_state(root)
            first = MODULE.capture_signature(project, manifest_path, package_root)
            manifest["Assets"][0]["BuildKey"] = "b" * 64
            manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            BuildAssetPackages.build_packages(
                project,
                manifest,
                {
                    "ChunkConfigVersion": 1,
                    "DefaultChunk": "core",
                    "Rules": [{"Chunk": "ui", "LogicalKeyPrefixes": ["Resources/Textures/Sources/UI/"]}],
                    "ChunkDependencies": {"ui": ["core"]},
                    "OwnerBindings": [],
                },
                package_root,
            )
            second = MODULE.capture_signature(project, manifest_path, package_root)

            differences = MODULE.diff_values(first, second)
            self.assertTrue(any("BuildKey" in value for value in differences))
            self.assertNotEqual(first["SignatureSha256"], second["SignatureSha256"])

    def test_full_cook_order_builds_font_before_texture(self):
        self.assertEqual(MODULE.COOKER_SCRIPTS, ("BuildFonts.ps1", "BuildTextures.ps1", "BuildMeshes.ps1"))


if __name__ == "__main__":
    unittest.main()
