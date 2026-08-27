import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPTS_DIR = Path(__file__).resolve().parents[2] / "Tools" / "Scripts"
SCRIPT_PATH = SCRIPTS_DIR / "BuildIncrementalAssets.py"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))
SPEC = importlib.util.spec_from_file_location("build_incremental_assets", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class IncrementalInvalidationTests(unittest.TestCase):
    def make_manifest(self) -> dict:
        return {
            "ManifestVersion": 1,
            "Assets": [
                {
                    "AssetId": "font01",
                    "AssetType": "Font",
                    "LogicalKey": "Resources/Fonts/Sources/Test.ttf#JP",
                    "BuildKey": "a" * 64,
                    "OutputPaths": ["Resources/Textures/Sources/UI/Font/page0.png"],
                },
                {
                    "AssetId": "texture01",
                    "AssetType": "Texture",
                    "LogicalKey": "Resources/Textures/Sources/UI/Font/page0.png",
                    "BuildKey": "b" * 64,
                    "OutputPaths": ["Resources/Textures/Compiled/UI/Font/page0.dds"],
                },
                {
                    "AssetId": "mesh01",
                    "AssetType": "Mesh",
                    "LogicalKey": "Resources/Models/Sources/Stage/Test.gltf",
                    "BuildKey": "c" * 64,
                    "OutputPaths": ["Resources/Models/Compiled/Stage/Test.kmesh"],
                },
            ],
            "DependencyGraph": {
                "Forward": {
                    "font01": ["Resources/Fonts/Sources/Test.ttf"],
                    "texture01": ["Resources/Textures/Sources/UI/Font/page0.png"],
                    "mesh01": [
                        "Resources/Models/Sources/Stage/Test.gltf",
                        "Resources/Models/Sources/Stage/albedo.png",
                    ],
                },
                "Reverse": {
                    "Resources/Fonts/Sources/Test.ttf": ["font01"],
                    "Resources/Textures/Sources/UI/Font/page0.png": ["texture01"],
                    "Resources/Models/Sources/Stage/Test.gltf": ["mesh01"],
                    "Resources/Models/Sources/Stage/albedo.png": ["mesh01"],
                },
            },
        }

    def test_changed_dependency_propagates_through_asset_outputs(self):
        plan = MODULE.build_plan(self.make_manifest(), ["Resources/Fonts/Sources/Test.ttf"])
        self.assertEqual([asset["AssetId"] for asset in plan["AffectedAssets"]], ["font01", "texture01"])
        self.assertEqual(plan["Cookers"], ["Font", "Texture"])
        self.assertTrue(plan["RequiresPackaging"])

    def test_external_mesh_dependency_invalidates_mesh(self):
        plan = MODULE.build_plan(self.make_manifest(), ["Resources/Models/Sources/Stage/albedo.png"])
        self.assertEqual(plan["AffectedAssetCount"], 1)
        self.assertEqual(plan["AffectedAssets"][0]["AssetId"], "mesh01")
        self.assertEqual(plan["Cookers"], ["Mesh"])
        self.assertTrue(plan["RequiresPackaging"])

    def test_new_untracked_source_selects_cooker_without_manifest_record(self):
        plan = MODULE.build_plan(self.make_manifest(), ["Resources/Textures/Sources/New/new.png"])
        self.assertEqual(plan["AffectedAssetCount"], 0)
        self.assertEqual(plan["Cookers"], ["Texture"])
        self.assertEqual(plan["UntrackedChangedPaths"], ["Resources/Textures/Sources/New/new.png"])

    def test_chunk_config_change_repackages_without_running_cookers(self):
        plan = MODULE.build_plan(self.make_manifest(), [MODULE.PACKAGE_CONFIG_PATH])
        # Chunk policy changes only the package layout, so no source converter should run.
        self.assertEqual(plan["Cookers"], [])
        self.assertTrue(plan["PackageConfigChanged"])
        self.assertTrue(plan["RequiresPackaging"])

    def test_snapshot_detects_content_change_and_removed_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "Project"
            source = project / "Resources" / "Models" / "Sources" / "Stage" / "Test.gltf"
            dependency = project / "Resources" / "Models" / "Sources" / "Stage" / "albedo.png"
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text("{}", encoding="utf-8")
            dependency.write_bytes(b"A")

            manifest = self.make_manifest()
            previous = MODULE.build_snapshot(project, manifest)
            dependency.write_bytes(b"B")
            current = MODULE.build_snapshot(project, manifest)
            changed = MODULE.diff_snapshots(previous, current)
            self.assertEqual(changed, ["Resources/Models/Sources/Stage/albedo.png"])

            dependency.unlink()
            removed = MODULE.build_snapshot(project, manifest)
            changed_removed = MODULE.diff_snapshots(current, removed)
            self.assertEqual(changed_removed, ["Resources/Models/Sources/Stage/albedo.png"])

    def test_snapshot_tracks_chunk_configuration(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "Project"
            config = project / MODULE.PACKAGE_CONFIG_PATH
            config.parent.mkdir(parents=True, exist_ok=True)
            config.write_text('{"ChunkConfigVersion":1,"DefaultChunk":"core"}', encoding="utf-8")
            snapshot = MODULE.build_snapshot(project, self.make_manifest())
            self.assertIn(MODULE.PACKAGE_CONFIG_PATH, snapshot["Files"])

    def test_missing_snapshot_marks_all_current_tracked_inputs_changed(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "Project"
            source = project / "Resources" / "Fonts" / "Sources" / "Test.ttf"
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_bytes(b"FONT")
            current = MODULE.build_snapshot(project, self.make_manifest())
            changed = MODULE.diff_snapshots(None, current)
            # First-run invalidation intentionally seeds the snapshot from every tracked input.
            self.assertIn("Resources/Fonts/Sources/Test.ttf", changed)


if __name__ == "__main__":
    unittest.main()
