from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
EDITOR_DIR = PROJECT_ROOT / "Engine" / "Editor"
NLOHMANN_DIR = PROJECT_ROOT / "Externals" / "nlohmann"
PREFAB_DIFF_HEADER = EDITOR_DIR / "EditorPrefabDiff.h"
PREFAB_PANEL_HEADER = EDITOR_DIR / "EditorPrefabDiffPanel.h"
ACTOR_WORLD_IMGUI = EDITOR_DIR / "Legacy" / "ActorWorld_ImGui.cpp"
RUNTIME_TEST = Path(__file__).with_name("EditorPrefabDiffRuntimeTests.cpp")


class EditorPrefabDiffContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = PREFAB_DIFF_HEADER.read_text(encoding="utf-8")
        cls.panel = PREFAB_PANEL_HEADER.read_text(encoding="utf-8")
        cls.actor_world_imgui = ACTOR_WORLD_IMGUI.read_text(encoding="utf-8")

    def test_diff_model_has_semantic_actor_and_component_kinds(self) -> None:
        self.assertIn("enum class EditorPrefabDiffKind", self.header)
        self.assertIn("ActorPropertyChanged", self.header)
        self.assertIn("ComponentAdded", self.header)
        self.assertIn("ComponentRemoved", self.header)
        self.assertIn("ComponentPropertyChanged", self.header)
        self.assertIn("EditorPrefabDiffSummary", self.header)

    def test_components_are_matched_by_stable_identity_not_array_position(self) -> None:
        self.assertIn('"N:" + name', self.header)
        self.assertIn('"C:" + className', self.header)
        self.assertIn("ComponentMap baseComponents", self.header)
        self.assertIn("instanceComponents.find(key)", self.header)
        self.assertIn("std::sort(result.entries.begin()", self.header)

    def test_nested_property_diff_tracks_missing_values(self) -> None:
        self.assertIn("baseExists", self.header)
        self.assertIn("instanceExists", self.header)
        self.assertIn("JoinPath(path, key)", self.header)
        self.assertIn("Settings", RUNTIME_TEST.read_text(encoding="utf-8"))

    def test_prefab_editor_panel_uses_tracked_source_and_semantic_diff(self) -> None:
        self.assertIn("PrefabInstanceRegistry::GetInstance()->Find", self.panel)
        self.assertIn("PrefabReferenceResolver::LoadBaseActor", self.panel)
        self.assertIn("EditorPrefabDiff::Build", self.panel)
        self.assertIn('SeparatorText("Prefab Diff")', self.panel)
        self.assertIn("DrawEditorPrefabDiffPanel(saveTargetActor)", self.actor_world_imgui)

    def test_runtime_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Header-only semantic diff is compiled directly to verify deterministic matching and change classification.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase11_prefab_diff_") as temp_dir:
            executable = Path(temp_dir) / "editor_prefab_diff_tests"
            command = [
                compiler,
                "-std=c++20",
                str(RUNTIME_TEST),
                "-I",
                str(EDITOR_DIR),
                "-I",
                str(NLOHMANN_DIR),
                "-o",
                str(executable),
            ]
            compile_result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ prefab diff runtime compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )

            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ prefab diff runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("Editor Prefab Diff runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
