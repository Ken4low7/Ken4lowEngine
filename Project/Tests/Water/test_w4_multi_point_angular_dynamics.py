from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
RIGIDBODY_H = ROOT / "Engine/Physics/Core/Rigidbody.h"
RIGIDBODY_CPP = ROOT / "Engine/Physics/Core/Rigidbody.cpp"
RIGIDBODY_COMPONENT = ROOT / "Engine/Scene/Actor/Components/RigidbodyComponent.cpp"
ACTOR_WORLD_PHYSICS = ROOT / "Engine/Scene/Actor/Core/ActorWorld_Pysics.cpp"
WATER_INTERACTION = ROOT / "Engine/Scene/Actor/Components/WaterInteractionComponent.h"


class W4MultiPointAngularDynamicsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.rigidbody_h = RIGIDBODY_H.read_text(encoding="utf-8")
        cls.rigidbody_cpp = RIGIDBODY_CPP.read_text(encoding="utf-8")
        cls.rigidbody_component = RIGIDBODY_COMPONENT.read_text(encoding="utf-8")
        cls.actor_world_physics = ACTOR_WORLD_PHYSICS.read_text(encoding="utf-8")
        cls.water = WATER_INTERACTION.read_text(encoding="utf-8")

    def test_force_at_position_generates_torque(self) -> None:
        self.assertIn("AddForceAtPosition", self.rigidbody_h)
        self.assertIn("Vector3::Cross(worldPosition - centerOfMass, force)", self.rigidbody_cpp)
        self.assertIn("angularVelocity_ += angularAcceleration * deltaTime", self.rigidbody_cpp)

    def test_mass_and_shape_define_angular_inertia(self) -> None:
        self.assertIn("SetInertiaScale", self.rigidbody_h)
        self.assertIn("CalculateInertiaScale", self.actor_world_physics)
        self.assertIn("(half.y * half.y + half.z * half.z) / 3.0f", self.actor_world_physics)
        self.assertIn("0.4f * radius * radius", self.actor_world_physics)

    def test_multi_point_buoyancy_is_distributed_to_probes(self) -> None:
        self.assertIn("probeWeights[index] / probeWeightSum", self.water)
        self.assertIn("AddForceAtPosition(probeForce, probes.points[index], centerOfMass)", self.water)
        self.assertIn("Vector3::Cross(probes.points[index] - centerOfMass, probeForce)", self.water)

    def test_water_drag_damps_linear_and_angular_velocity(self) -> None:
        self.assertIn("velocityBeforeDrag * damping", self.water)
        self.assertIn("angularVelocity * angularDamping", self.water)
        self.assertIn("SetAngularVelocity", self.water)

    def test_surface_alignment_uses_torque_not_direct_rotation(self) -> None:
        self.assertIn("ApplySurfaceAlignmentTorque", self.water)
        self.assertIn("rigidbody->AddTorque(alignmentTorque)", self.water)
        self.assertNotIn("rotation.x = std::lerp", self.water)

    def test_angular_velocity_reaches_actor_transform(self) -> None:
        self.assertIn("rotation += angularVelocity_ * deltaTime", self.rigidbody_component)
        self.assertIn("root->RefreshWorldTransform()", self.rigidbody_component)

    def test_diagnostics_expose_probe_and_torque_state(self) -> None:
        self.assertIn("Buoyancy Torque", self.water)
        self.assertIn("Angular Speed", self.water)
        self.assertIn("Submerged Probes", self.water)


if __name__ == "__main__":
    unittest.main()
