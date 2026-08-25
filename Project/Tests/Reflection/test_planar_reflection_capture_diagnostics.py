from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DIAGNOSTICS = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionCaptureDiagnostics.h"
BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionSceneBridge.h"
COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "PlanarReflectionComponent.inl"


class PlanarReflectionCaptureDiagnosticsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.diagnostics = DIAGNOSTICS.read_text(encoding="utf-8")
        cls.bridge = BRIDGE.read_text(encoding="utf-8")
        cls.component = COMPONENT.read_text(encoding="utf-8")

    def test_capture_bridge_records_generic_drawable_candidates(self) -> None:
        self.assertIn("PlanarReflectionCaptureStats captureStats", self.bridge)
        self.assertIn("++captureStats.drawableCount", self.bridge)
        self.assertIn("PlanarReflectionCaptureDiagnostics::GetInstance()->Record", self.bridge)
        self.assertIn("opaqueCount", self.diagnostics)
        self.assertIn("transparentCount", self.diagnostics)

    def test_component_previews_the_actual_planar_render_target(self) -> None:
        self.assertIn("planarManager->ResolveBinding(this)", self.component)
        self.assertIn("Capture RT Preview", self.component)
        self.assertIn("ImGui::Image(static_cast<ImTextureID>(previewBinding.texture.ptr)", self.component)
        self.assertIn("Capture候補", self.component)


if __name__ == "__main__":
    unittest.main()
