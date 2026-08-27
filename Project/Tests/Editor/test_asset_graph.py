from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
EDITOR_DIR = PROJECT_ROOT / "Engine" / "Editor"
NLOHMANN_DIR = PROJECT_ROOT / "Externals" / "nlohmann"
GRAPH_HEADER = EDITOR_DIR / "EditorAssetGraph.h"
CONTENT_BROWSER_HEADER = EDITOR_DIR / "EditorContentBrowserPanel.h"
RUNTIME_TEST = Path(__file__).with_name("EditorAssetGraphRuntimeTests.cpp")


class EditorAssetGraphContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.graph_header = GRAPH_HEADER.read_text(encoding="utf-8")
        cls.content_browser = CONTENT_BROWSER_HEADER.read_text(encoding="utf-8")

    def test_graph_consumes_phase8_manifest_and_optional_package_manifest(self) -> None:
        self.assertIn('"AssetManifest.json"', self.graph_header)
        self.assertIn('"PackageManifest.json"', self.graph_header)
        self.assertIn('"AssetToChunk"', self.graph_header)
        self.assertIn('"Dependencies"', self.graph_header)
        self.assertIn('"OutputPaths"', self.graph_header)

    def test_graph_builds_forward_reverse_and_transitive_impact(self) -> None:
        self.assertIn("consumersByDependencyPath_", self.graph_header)
        self.assertIn("directDependentAssetIds", self.graph_header)
        self.assertIn("affectedAssetIds", self.graph_header)
        self.assertIn("std::queue<std::string> pending", self.graph_header)
        self.assertIn("affectedChunkIds", self.graph_header)

    def test_content_browser_exposes_graph_for_selected_asset(self) -> None:
        self.assertIn('#include "EditorAssetGraph.h"', self.content_browser)
        self.assertIn("DrawAssetGraphWindow();", self.content_browser)
        self.assertIn('ImGui::Begin("Asset Graph###AssetGraph")', self.content_browser)
        self.assertIn('"Direct Dependents"', self.content_browser)
        self.assertIn('"Rebuild Impact"', self.content_browser)
        self.assertIn("Reload Manifest", self.content_browser)

    def test_runtime_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Header-only graph is exercised directly so reverse/transitive impact is tested independently from ImGui.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase11_asset_graph_") as temp_dir:
            executable = Path(temp_dir) / "editor_asset_graph_tests"
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
                msg=f"C++ asset graph runtime compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )

            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ asset graph runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("Editor Asset Graph runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
