from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PLANAR_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "PlanarReflectionComponent.inl"


class PlanarReflectionCubeAutoNormalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.component = PLANAR_COMPONENT.read_text(encoding="utf-8")

    def test_cube_falls_back_to_receiver_local_z_axis(self) -> None:
        self.assertNotIn("GetActiveCameraForward()", self.component)
        self.assertIn("fallbackAxis = axes[2]", self.component)
        self.assertIn("fallbackCenterProjection = centerProjection[2]", self.component)
        self.assertIn("if (!bestReceiver && fallbackReceiver)", self.component)
        self.assertIn("bestAxis = fallbackAxis", self.component)

    def test_flat_receiver_still_prefers_thinnest_axis(self) -> None:
        self.assertIn("kAutoNormalFlatnessThreshold", self.component)
        self.assertIn("bestAxis = axes[thinnestIndex]", self.component)
        self.assertIn("autoNormalCacheValid_ = true", self.component)


if __name__ == "__main__":
    unittest.main()
