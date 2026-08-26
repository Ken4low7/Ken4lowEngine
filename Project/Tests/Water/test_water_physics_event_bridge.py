from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
DISPATCHER = ROOT / "Engine/Physics/Event/PhysicsEventDispatcher.cpp"
WATER_INTERACTION = ROOT / "Engine/Scene/Actor/Components/WaterInteractionComponent.h"


class WaterPhysicsEventBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dispatcher = DISPATCHER.read_text(encoding="utf-8")
        cls.water_interaction = WATER_INTERACTION.read_text(encoding="utf-8")

    def test_trigger_events_are_forwarded_to_colliders(self) -> None:
        self.assertIn('collider->OnOverlapBegin(hit);', self.dispatcher)
        self.assertIn('collider->OnOverlapStay(hit);', self.dispatcher)
        self.assertIn('collider->OnOverlapEnd(hit);', self.dispatcher)

    def test_both_contact_sides_receive_oriented_hits(self) -> None:
        self.assertIn('BuildColliderHit(event.colliderA, event.colliderB, event.contact, false)', self.dispatcher)
        self.assertIn('BuildColliderHit(event.colliderB, event.colliderA, event.contact, true)', self.dispatcher)
        self.assertIn('contact.normal * -1.0f', self.dispatcher)

    def test_water_tracks_trigger_overlap_callbacks(self) -> None:
        self.assertIn('SetOnOverlapBeginCallback', self.water_interaction)
        self.assertIn('SetOnOverlapStayCallback', self.water_interaction)
        self.assertIn('SetOnOverlapEndCallback', self.water_interaction)
        self.assertIn('ApplyWaterDynamics(contact, deltaTime)', self.water_interaction)


if __name__ == "__main__":
    unittest.main()
