from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MANAGER_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionManager.h"
MANAGER_INL = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionManager.inl"
COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "PlanarReflectionComponent.h"
COMPONENT_INL = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "PlanarReflectionComponent.inl"


class PlanarReflectionQualityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manager_h = MANAGER_H.read_text(encoding="utf-8")
        cls.manager = MANAGER_INL.read_text(encoding="utf-8")
        cls.component_h = COMPONENT_H.read_text(encoding="utf-8")
        cls.component = COMPONENT_INL.read_text(encoding="utf-8")

    def test_quality_levels_preserve_ultra_as_legacy_full_resolution(self) -> None:
        for quality in ("Low", "Medium", "High", "Ultra"):
            self.assertIn(f"PlanarReflectionQuality::{quality}", self.manager_h)
        self.assertIn("case PlanarReflectionQuality::Low: return 0.25f", self.manager_h)
        self.assertIn("case PlanarReflectionQuality::Medium: return 0.50f", self.manager_h)
        self.assertIn("case PlanarReflectionQuality::High: return 0.75f", self.manager_h)
        self.assertIn("return 1.00f", self.manager_h)
        self.assertIn("PlanarReflectionQuality quality = PlanarReflectionQuality::Ultra", self.manager_h)

    def test_target_recreates_when_capture_dimensions_change(self) -> None:
        self.assertIn("ResolveCaptureDimension", self.manager)
        self.assertIn("surface.target->width == requiredWidth", self.manager)
        self.assertIn("surface.target->height == requiredHeight", self.manager)
        self.assertIn("surface.target.reset()", self.manager)
        self.assertIn("CreateTarget(surface.desc.quality)", self.manager)
        self.assertIn("target->width = captureWidth", self.manager)
        self.assertIn("target->height = captureHeight", self.manager)

    def test_component_serializes_quality_and_reports_resolution(self) -> None:
        self.assertIn("PlanarReflectionQuality quality_ = PlanarReflectionQuality::Ultra", self.component_h)
        self.assertIn('outJson["Quality"]', self.component)
        self.assertIn('inJson.find("Quality")', self.component)
        self.assertIn('"Low (25%)"', self.component)
        self.assertIn('"Ultra (100%)"', self.component)
        self.assertIn("Capture Resolution: %u x %u", self.component)  # Editorから実際のCapture解像度を確認できることを固定する。
        self.assertIn("desc.quality = quality_", self.component)


if __name__ == "__main__":
    unittest.main()
