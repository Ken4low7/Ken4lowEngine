from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
WATER_SURFACE = ROOT / "Engine/Scene/Actor/Components/WaterSurfaceComponent.h"
WATER_INTERACTION = ROOT / "Engine/Scene/Actor/Components/WaterInteractionComponent.h"
COMPONENT_FACTORY = ROOT / "Engine/Scene/Actor/Serialization/ComponentFactory.cpp"


class WaterInteractionRegressionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.surface = WATER_SURFACE.read_text(encoding="utf-8")
        cls.interaction = WATER_INTERACTION.read_text(encoding="utf-8")
        cls.factory = COMPONENT_FACTORY.read_text(encoding="utf-8")

    def test_cpu_surface_query_uses_same_two_gerstner_waves(self) -> None:
        self.assertIn("SampleSurfaceAtWorldPosition", self.surface)
        self.assertIn("amplitude * 0.45f", self.surface)
        self.assertIn("wavelength * 0.58f", self.surface)
        self.assertIn("speed * 1.35f", self.surface)
        self.assertIn("steepness * 0.7f", self.surface)
        self.assertIn("1.7f", self.surface)

    def test_interaction_uses_trigger_only_as_candidate_volume(self) -> None:
        self.assertIn("class WaterInteractionComponent final : public ColliderComponent", self.interaction)
        self.assertIn("SetShapeType(ECollisionShapeType::OBB)", self.interaction)
        self.assertIn("SetIsTrigger(true)", self.interaction)
        self.assertIn("SetOnOverlapBeginCallback", self.interaction)
        self.assertIn("SetOnOverlapStayCallback", self.interaction)
        self.assertIn("SetOnOverlapEndCallback", self.interaction)
        self.assertIn("SampleSurfaceAtWorldPosition", self.interaction)

    def test_contact_states_and_transitions_are_exposed(self) -> None:
        self.assertIn("EWaterContactState", self.interaction)
        self.assertIn("AboveSurface", self.interaction)
        self.assertIn("TouchingSurface", self.interaction)
        self.assertIn("Submerged", self.interaction)
        self.assertIn("SetOnWaterEnter", self.interaction)
        self.assertIn("SetOnWaterStay", self.interaction)
        self.assertIn("SetOnWaterExit", self.interaction)
        self.assertIn("GetActorSubmersionDepth", self.interaction)

    def test_factory_registers_single_water_interaction_component(self) -> None:
        self.assertIn('#include "WaterInteractionComponent.h"', self.factory)
        self.assertIn(
            'MakeComponentTypeInfo<WaterInteractionComponent>("WaterInteractionComponent", false',
            self.factory,
        )


if __name__ == "__main__":
    unittest.main()
