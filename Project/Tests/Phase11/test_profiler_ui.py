from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
EDITOR_DIR = PROJECT_ROOT / "Engine" / "Editor"
SCENE_BASE = PROJECT_ROOT / "Engine" / "Scene" / "Management" / "BaseScene.h"
DEBUG_SCENE = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "DebugScene" / "DebugScene.h"
PROFILER_HEADER = EDITOR_DIR / "EditorProfilerPanel.h"
WINDOW_MANAGER = EDITOR_DIR / "EditorWindowManager.h"
LEVEL_OVERLAY = EDITOR_DIR / "EditorLevelOverlay.h"


class EditorProfilerContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.profiler = PROFILER_HEADER.read_text(encoding="utf-8")
        cls.window_manager = WINDOW_MANAGER.read_text(encoding="utf-8")
        cls.level_overlay = LEVEL_OVERLAY.read_text(encoding="utf-8")
        cls.scene_base = SCENE_BASE.read_text(encoding="utf-8")
        cls.debug_scene = DEBUG_SCENE.read_text(encoding="utf-8")

    def test_profiler_reads_existing_subsystem_diagnostics(self) -> None:
        # Phase 11.5 must aggregate subsystem-owned counters instead of inventing another schedule or lifetime model.
        required_reads = [
            "GetCompletedFrameTiming()",
            "GetCompileStats()",
            "GetRenderGraphTransientPool().GetStats()",
            "GetWorkerCount()",
            "GetPendingJobCount()",
            "GetStats()",
            "GetDescriptorStats()",
            "GetShaderCacheStats()",
            "GetCacheStats()",
            "GetLoadedSubLevelCount()",
        ]
        for token in required_reads:
            self.assertIn(token, self.profiler)

    def test_profiler_stays_read_only_for_runtime_truth(self) -> None:
        forbidden_mutations = [
            ".Compile(",
            ".Reset(",
            ".AddPass(",
            ".AddSystem(",
            ".AllocateTransient(",
            ".ApplyEditorSettings(",
            ".UpdateSubLevelEditorMetadata(",
        ]
        for token in forbidden_mutations:
            self.assertNotIn(token, self.profiler)

    def test_frame_monitor_is_shared_from_editor_window_manager(self) -> None:
        self.assertIn("const PerformanceMonitor& GetPerformanceMonitor() const", self.window_manager)
        self.assertIn("&windowManager->GetPerformanceMonitor()", self.level_overlay)
        self.assertNotIn("PerformanceMonitor performanceMonitor_", self.profiler)
        self.assertNotIn("PerformanceMonitor profiler_", self.profiler)

    def test_scene_scheduler_is_exposed_as_optional_read_only_diagnostic(self) -> None:
        self.assertIn("virtual const SystemScheduler* GetEditorSystemScheduler() const", self.scene_base)
        self.assertIn("const K4E::SystemScheduler* GetEditorSystemScheduler() const override", self.debug_scene)
        self.assertIn("scene->GetEditorSystemScheduler()", self.profiler)
        self.assertIn("GetCompiledOrder()", self.profiler)
        self.assertIn("GetExecutionPolicy(handle)", self.profiler)

    def test_profiler_is_integrated_with_editor_and_rendergraph_detail_view(self) -> None:
        self.assertIn('#include "EditorProfilerPanel.h"', self.level_overlay)
        self.assertIn("EditorProfilerPanel::GetInstance()->Draw", self.level_overlay)
        self.assertIn("ImGuiKey_F11", self.profiler)
        self.assertIn("RenderGraphVisualizer::GetInstance()->SetVisible(true)", self.profiler)


if __name__ == "__main__":
    unittest.main()
