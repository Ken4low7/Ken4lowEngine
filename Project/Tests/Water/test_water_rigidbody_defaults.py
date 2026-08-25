from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
RIGIDBODY_COMPONENT = ROOT / "Engine/Scene/Actor/Components/RigidbodyComponent.h"
COLLIDER_COMPONENT = ROOT / "Engine/Scene/Actor/Components/ColliderComponent.cpp"


class WaterRigidbodyDefaultsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.rigidbody = RIGIDBODY_COMPONENT.read_text(encoding="utf-8")
        cls.collider = COLLIDER_COMPONENT.read_text(encoding="utf-8")

    def test_new_dynamic_rigidbody_uses_gravity_by_default(self) -> None:
        self.assertIn("BodyType bodyType_ = BodyType::Dynamic", self.rigidbody)
        self.assertIn("bool useGravity_ = true", self.rigidbody)

    def test_physics_collider_position_is_applied_back_to_actor_root(self) -> None:
        self.assertIn("const Vector3 correctedWorldPosition = collider_->GetCenterPosition()", self.collider)
        self.assertIn("root->LocalPosition() += correctionDelta", self.collider)
        self.assertIn("root->RefreshWorldTransform()", self.collider)


if __name__ == "__main__":
    unittest.main()
