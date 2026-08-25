from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ANIMATION_MODEL = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Core" / "AnimationModel.cpp"


class PlanarReflectionAnimationMultiViewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = ANIMATION_MODEL.read_text(encoding="utf-8")

    def test_animation_model_snapshots_per_draw_constants(self) -> None:
        self.assertIn("FrameUploadArena& frameUploadArena", self.source)
        self.assertIn("AllocateConstant(*wvpData_)", self.source)
        self.assertIn("AllocateConstant(*cameraData)", self.source)
        self.assertIn("AllocateConstant(*shadowParameterData_)", self.source)
        self.assertIn("SetGraphicsRootConstantBufferView(1, transformAllocation.gpuAddress)", self.source)
        self.assertIn("SetGraphicsRootConstantBufferView(3, cameraAllocation.gpuAddress)", self.source)
        self.assertIn("SetGraphicsRootConstantBufferView(7, shadowParameterAllocation.gpuAddress)", self.source)
        self.assertNotIn("SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress())", self.source)
        self.assertNotIn("SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress())", self.source)

    def test_animation_model_refreshes_current_render_view(self) -> None:
        self.assertIn("const float distSq = CalcDistanceSqToCamera();", self.source)
        self.assertIn("lodController_.UpdateByDistanceSq(distSq", self.source)
        self.assertGreaterEqual(
            self.source.count("cameraData->worldPosition = CameraManager::GetInstance()->GetActiveCameraPosition()"),
            2,
        )  # Reflection/Mainの両DrawでActive Cameraへ同期する契約を固定する。


if __name__ == "__main__":
    unittest.main()
