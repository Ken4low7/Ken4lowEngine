from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
STREAMING_DIR = PROJECT_ROOT / "Engine" / "Scene" / "Streaming"
EDITOR_DIR = PROJECT_ROOT / "Engine" / "Editor"
GRID_HEADER = STREAMING_DIR / "WorldPartitionGrid.h"
MANAGER_HEADER = STREAMING_DIR / "WorldPartitionManager.h"
MANAGER_SOURCE = STREAMING_DIR / "WorldPartitionManager.cpp"
SUBLEVEL_HEADER = STREAMING_DIR / "SubLevelManager.h"
SUBLEVEL_SOURCE = STREAMING_DIR / "SubLevelManager.cpp"
PANEL_HEADER = EDITOR_DIR / "EditorWorldPartitionPanel.h"
PREFAB_PANEL = EDITOR_DIR / "EditorPrefabDiffPanel.h"
RUNTIME_TEST = Path(__file__).with_name("WorldPartitionGridRuntimeTests.cpp")


class WorldPartitionEditorContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.grid_header = GRID_HEADER.read_text(encoding="utf-8")
        cls.manager_header = MANAGER_HEADER.read_text(encoding="utf-8")
        cls.manager_source = MANAGER_SOURCE.read_text(encoding="utf-8")
        cls.sublevel_header = SUBLEVEL_HEADER.read_text(encoding="utf-8")
        cls.sublevel_source = SUBLEVEL_SOURCE.read_text(encoding="utf-8")
        cls.panel_header = PANEL_HEADER.read_text(encoding="utf-8")
        cls.prefab_panel = PREFAB_PANEL.read_text(encoding="utf-8")

    def test_runtime_and_editor_share_cell_identity_and_residency_rules(self) -> None:
        self.assertIn("struct WorldPartitionCell", self.grid_header)
        self.assertIn("WorldToCell", self.grid_header)
        self.assertIn("ChebyshevDistance", self.grid_header)
        self.assertIn("WorldPartitionStreamingDecision", self.grid_header)
        self.assertIn("WorldPartitionGrid::WorldToCell", self.manager_source)
        self.assertIn("WorldPartitionGrid::Evaluate", self.manager_source)

    def test_manager_exposes_source_cell_and_non_destructive_editor_updates(self) -> None:
        self.assertIn("GetStreamingSourcePosition", self.manager_header)
        self.assertIn("GetStreamingSourceCell", self.manager_header)
        self.assertIn("ApplyEditorSettings", self.manager_header)
        self.assertIn("UpdateSubLevelEditorMetadata", self.manager_header)
        self.assertIn("UpdateReferenceMetadata(*found)", self.manager_source)
        self.assertIn("Update(lastStreamingSourcePosition_)", self.manager_source)

    def test_sublevel_metadata_edit_preserves_streaming_state(self) -> None:
        self.assertIn("UpdateReferenceMetadata", self.sublevel_header)
        self.assertIn("found->second.reference = reference", self.sublevel_source)
        update_body = self.sublevel_source.split("bool SubLevelManager::UpdateReferenceMetadata", 1)[1].split("SubLevelState SubLevelManager::GetState", 1)[0]
        self.assertNotIn("Reset()", update_body)
        self.assertNotIn("RemoveTrackedActors", update_body)

    def test_editor_panel_edits_cells_and_reads_real_runtime_state(self) -> None:
        self.assertIn("World Partition##WorldPartitionEditor", self.panel_header)
        self.assertIn("partition->GetStreamingSourceCell()", self.panel_header)
        self.assertIn("subLevels->GetState", self.panel_header)
        self.assertIn("UpdateSubLevelEditorMetadata", self.panel_header)
        self.assertIn("RequestLoad", self.panel_header)
        self.assertIn("RequestUnload", self.panel_header)
        self.assertIn("Retry", self.panel_header)
        self.assertIn("MarkLevelDirty", self.panel_header)
        self.assertIn("DrawEditorWorldPartitionPanel", self.prefab_panel)

    def test_runtime_grid_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Pure grid math is compiled outside the engine so cell boundaries and hysteresis remain independently testable.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase11_partition_") as temp_dir:
            executable = Path(temp_dir) / "world_partition_grid_tests"
            command = [
                compiler,
                "-std=c++20",
                str(RUNTIME_TEST),
                "-I",
                str(STREAMING_DIR),
                "-o",
                str(executable),
            ]
            compile_result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ world partition grid runtime compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )

            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ world partition grid runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("World Partition Grid runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
