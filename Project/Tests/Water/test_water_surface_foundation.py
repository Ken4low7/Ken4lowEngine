from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "WaterSurfaceComponent.h"
MODEL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.h"
FACTORY = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Serialization" / "ComponentFactory.cpp"


class WaterSurfaceFoundationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.component = COMPONENT.read_text(encoding="utf-8")
        cls.model_component = MODEL_COMPONENT.read_text(encoding="utf-8")
        cls.factory = FACTORY.read_text(encoding="utf-8")

    def test_water_surface_reuses_model_render_path(self) -> None:
        self.assertIn("class WaterSurfaceComponent final : public ModelComponent", self.component)
        self.assertIn("Object3D* GetObject3D()", self.model_component)
        self.assertIn("ModelComponent::UpdateEditor(deltaTime);", self.component)

    def test_new_water_uses_existing_plane_asset_and_transparent_forward(self) -> None:
        self.assertIn('SetModelPath("Sample/plane.gltf")', self.component)
        self.assertIn("SetAlphaBlendEnabled(true)", self.component)
        self.assertIn("SetCullMode(MaterialCullMode::None)", self.component)
        self.assertIn("SetReflectivity", self.component)
        self.assertIn("SetRoughness", self.component)

    def test_water_does_not_create_hidden_reflection_or_light_components(self) -> None:
        self.assertNotIn("AddComponent<PlanarReflectionComponent>", self.component)
        self.assertNotIn("AddComponent<LightComponent>", self.component)
        self.assertIn("PlanarReflectionComponent", self.component)

    def test_water_is_registered_in_component_factory(self) -> None:
        self.assertIn('#include "WaterSurfaceComponent.h"', self.factory)
        self.assertIn('MakeComponentTypeInfo<WaterSurfaceComponent>("WaterSurfaceComponent"', self.factory)

    def test_water_state_is_json_persistent(self) -> None:
        for key in ("WaterColor", "Opacity", "Reflectivity", "Roughness"):
            self.assertIn(f'outJson["{key}"]', self.component)
            self.assertIn(f'"{key}"', self.component)


if __name__ == "__main__":
    unittest.main()
