from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PLANAR_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionSceneBridge.h"
CAPTURE_DRAWABLE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "ReflectionCaptureDrawable.h"
MODEL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.h"
MODEL_COMPONENT_CPP = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.cpp"
INSTANCED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "InstancedModelComponent.h"
ANIMATED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponent.h"
ANIMATED_FORWARD = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponentForward.inl"
SKELETAL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponent.h"
SKELETAL_FORWARD = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponentForward.inl"
TEST_GROUND_PREFAB = PROJECT_ROOT / "Resources" / "ActorPrefabs" / "TestGroundActor.json"


class ReflectionCaptureComponentCoverageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.planar_bridge = PLANAR_BRIDGE.read_text(encoding="utf-8")
        cls.capture_drawable = CAPTURE_DRAWABLE.read_text(encoding="utf-8")
        cls.model = MODEL_COMPONENT.read_text(encoding="utf-8")
        cls.model_cpp = MODEL_COMPONENT_CPP.read_text(encoding="utf-8")
        cls.instanced = INSTANCED_COMPONENT.read_text(encoding="utf-8")
        cls.animated_h = ANIMATED_COMPONENT.read_text(encoding="utf-8")
        cls.animated_forward = ANIMATED_FORWARD.read_text(encoding="utf-8")
        cls.skeletal_h = SKELETAL_COMPONENT.read_text(encoding="utf-8")
        cls.skeletal_forward = SKELETAL_FORWARD.read_text(encoding="utf-8")
        cls.test_ground_prefab = TEST_GROUND_PREFAB.read_text(encoding="utf-8")

    def test_planar_capture_discovers_drawables_without_type_switches(self) -> None:
        self.assertIn("class ReflectionCaptureDrawable", self.capture_drawable)
        self.assertIn("GetReflectionCaptureBlendMode() const", self.capture_drawable)
        self.assertIn("GetReflectionCaptureSortPosition() const", self.capture_drawable)
        self.assertIn("dynamic_cast<ReflectionCaptureDrawable*>", self.planar_bridge)
        self.assertIn("item.drawable->DrawReflectionCapture()", self.planar_bridge)
        for component_type in (
            "ModelComponent",
            "InstancedModelComponent",
            "AnimatedModelComponent",
            "SkeletalMeshComponent",
        ):
            self.assertNotIn(f"GetComponents<{component_type}>()", self.planar_bridge)

    def test_primary_3d_components_implement_capture_drawable_contract(self) -> None:
        for source in (self.model, self.instanced, self.animated_h, self.skeletal_h):
            self.assertIn("public ReflectionCaptureDrawable", source)
            self.assertIn("GetReflectionCaptureBlendMode() const override", source)
            self.assertIn("GetReflectionCaptureSortPosition() const override", source)
            self.assertIn("DrawReflectionCapture() override", source)

    def test_instanced_ground_prefab_is_covered_by_generic_planar_capture(self) -> None:
        self.assertIn('"Class": "InstancedModelComponent"', self.test_ground_prefab)
        self.assertIn("public ReflectionCaptureDrawable", self.instanced)
        self.assertIn("item.drawable->DrawReflectionCapture()", self.planar_bridge)  # 床Prefabが型別分岐なしで鏡Capture対象になることを固定する。

    def test_capture_queue_uses_forward_blend_order_and_reflection_camera_depth(self) -> None:
        self.assertIn("std::stable_sort", self.planar_bridge)
        self.assertIn("case MaterialBlendMode::Opaque: return 0", self.planar_bridge)
        self.assertIn("case MaterialBlendMode::Masked: return 1", self.planar_bridge)
        self.assertIn("case MaterialBlendMode::Transparent: return 2", self.planar_bridge)
        self.assertIn("case MaterialBlendMode::Additive: return 3", self.planar_bridge)
        self.assertIn("CameraManager::GetInstance()->GetActiveCameraPosition()", self.planar_bridge)
        self.assertIn("cameraManager->GetActiveCameraForward()", self.planar_bridge)
        self.assertIn("lhs.sortDepth > rhs.sortDepth", self.planar_bridge)
        self.assertIn("lhs.sortDepth < rhs.sortDepth", self.planar_bridge)

    def test_static_and_instanced_capture_no_longer_drop_transparent_surfaces(self) -> None:
        self.assertIn("DrawWithReflectionBinding();", self.model_cpp)
        self.assertNotIn("blendMode == MaterialBlendMode::Transparent || blendMode == MaterialBlendMode::Additive", self.model_cpp)
        self.assertIn("renderer_->Draw()", self.instanced)
        self.assertNotIn("blendMode == MaterialBlendMode::Transparent || blendMode == MaterialBlendMode::Additive", self.instanced)

    def test_animated_capture_preserves_forward_surface_mode_for_transparency(self) -> None:
        self.assertIn("void DrawReflectionCapture() override;", self.animated_h)
        self.assertIn("AnimatedModelComponent::DrawReflectionCapture()", self.animated_forward)
        self.assertIn("AnimationForwardSurface::ScopedBlendMode", self.animated_forward)
        self.assertNotIn("blendMode == MaterialBlendMode::Transparent || blendMode == MaterialBlendMode::Additive", self.animated_forward)

    def test_skeletal_capture_preserves_forward_surface_mode_for_transparency(self) -> None:
        self.assertIn("void DrawReflectionCapture() override;", self.skeletal_h)
        self.assertIn("SkeletalMeshComponent::DrawReflectionCapture()", self.skeletal_forward)
        self.assertIn("AnimationForwardSurface::ScopedBlendMode", self.skeletal_forward)
        self.assertNotIn("blendMode == MaterialBlendMode::Transparent || blendMode == MaterialBlendMode::Additive", self.skeletal_forward)


if __name__ == "__main__":
    unittest.main()
