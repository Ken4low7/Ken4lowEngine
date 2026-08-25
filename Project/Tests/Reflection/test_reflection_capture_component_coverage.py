from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PLANAR_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionSceneBridge.h"
PROBE_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "ReflectionProbeSceneBridge.h"
INSTANCED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "InstancedModelComponent.h"
ANIMATED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponent.h"
ANIMATED_FORWARD = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponentForward.inl"
SKELETAL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponent.h"
SKELETAL_FORWARD = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponentForward.inl"


class ReflectionCaptureComponentCoverageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.planar_bridge = PLANAR_BRIDGE.read_text(encoding="utf-8")
        cls.probe_bridge = PROBE_BRIDGE.read_text(encoding="utf-8")
        cls.instanced = INSTANCED_COMPONENT.read_text(encoding="utf-8")
        cls.animated_h = ANIMATED_COMPONENT.read_text(encoding="utf-8")
        cls.animated_forward = ANIMATED_FORWARD.read_text(encoding="utf-8")
        cls.skeletal_h = SKELETAL_COMPONENT.read_text(encoding="utf-8")
        cls.skeletal_forward = SKELETAL_FORWARD.read_text(encoding="utf-8")

    def test_planar_capture_draws_primary_3d_component_types(self) -> None:
        for component_type in (
            "ModelComponent",
            "InstancedModelComponent",
            "AnimatedModelComponent",
            "SkeletalMeshComponent",
        ):
            self.assertIn(f"GetComponents<{component_type}>()", self.planar_bridge)
        self.assertIn("instanced->DrawReflectionCapture()", self.planar_bridge)
        self.assertIn("animated->DrawReflectionCapture()", self.planar_bridge)
        self.assertIn("skeletal->DrawReflectionCapture()", self.planar_bridge)

    def test_probe_capture_matches_planar_component_coverage(self) -> None:
        for component_type in (
            "ModelComponent",
            "InstancedModelComponent",
            "AnimatedModelComponent",
            "SkeletalMeshComponent",
        ):
            self.assertIn(f"GetComponents<{component_type}>()", self.probe_bridge)
        self.assertIn("instanced->DrawReflectionCapture()", self.probe_bridge)
        self.assertIn("animated->DrawReflectionCapture()", self.probe_bridge)
        self.assertIn("skeletal->DrawReflectionCapture()", self.probe_bridge)

    def test_instanced_capture_uses_current_reflection_view_and_skips_transparency(self) -> None:
        self.assertIn("void DrawReflectionCapture()", self.instanced)
        self.assertIn("renderer_->Draw()", self.instanced)
        self.assertIn("MaterialBlendMode::Transparent", self.instanced)
        self.assertIn("MaterialBlendMode::Additive", self.instanced)

    def test_animated_capture_preserves_forward_surface_mode(self) -> None:
        self.assertIn("void DrawReflectionCapture();", self.animated_h)
        self.assertIn("AnimatedModelComponent::DrawReflectionCapture()", self.animated_forward)
        self.assertIn("AnimationForwardSurface::ScopedBlendMode", self.animated_forward)
        self.assertIn("MaterialBlendMode::Transparent", self.animated_forward)
        self.assertIn("MaterialBlendMode::Additive", self.animated_forward)

    def test_skeletal_capture_preserves_forward_surface_mode(self) -> None:
        self.assertIn("void DrawReflectionCapture();", self.skeletal_h)
        self.assertIn("SkeletalMeshComponent::DrawReflectionCapture()", self.skeletal_forward)
        self.assertIn("AnimationForwardSurface::ScopedBlendMode", self.skeletal_forward)
        self.assertIn("MaterialBlendMode::Transparent", self.skeletal_forward)
        self.assertIn("MaterialBlendMode::Additive", self.skeletal_forward)


if __name__ == "__main__":
    unittest.main()
