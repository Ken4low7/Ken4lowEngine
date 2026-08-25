from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "WaterSurfaceComponent.h"
MODEL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.h"
FACTORY = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Serialization" / "ComponentFactory.cpp"
MATERIAL_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.h"
MATERIAL_CPP = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.cpp"
OBJECT3D = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3D.h"
OBJECT3D_PS = PROJECT_ROOT / "Resources" / "Shaders" / "Object3D" / "Object3d.PS.hlsl"


class WaterSurfaceFoundationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.component = COMPONENT.read_text(encoding="utf-8")
        cls.model_component = MODEL_COMPONENT.read_text(encoding="utf-8")
        cls.factory = FACTORY.read_text(encoding="utf-8")
        cls.material_h = MATERIAL_H.read_text(encoding="utf-8")
        cls.material_cpp = MATERIAL_CPP.read_text(encoding="utf-8")
        cls.object3d = OBJECT3D.read_text(encoding="utf-8")
        cls.object3d_ps = OBJECT3D_PS.read_text(encoding="utf-8")

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
        for key in (
            "WaterColor",
            "Opacity",
            "Reflectivity",
            "Roughness",
            "WaveScale",
            "WaveSpeed",
            "NormalStrength",
            "FresnelF0",
            "ReflectionDistortion",
            "SecondaryWaveScale",
        ):
            self.assertIn(f'outJson["{key}"]', self.component)
            self.assertIn(f'"{key}"', self.component)

    def test_material_constant_layout_contains_water_state_on_cpu_and_hlsl(self) -> None:
        for field in (
            "waterSurfaceEnabled",
            "waterTime",
            "waterWaveScale",
            "waterWaveSpeed",
            "waterNormalStrength",
            "waterFresnelF0",
            "waterReflectionDistortion",
            "waterSecondaryWaveScale",
        ):
            self.assertIn(field, self.material_h)
            self.assertIn(field, self.object3d_ps)
        self.assertIn("sizeof(Material::MaterialCBData) == 272", self.material_cpp)

    def test_water_shader_perturbs_normal_without_moving_geometry(self) -> None:
        self.assertIn("float3 geometricNormal = normalize(input.normal);", self.object3d_ps)
        self.assertIn("waterReflectionOffset", self.object3d_ps)
        self.assertIn("slopeTangent", self.object3d_ps)
        self.assertIn("slopeBitangent", self.object3d_ps)
        self.assertIn("abs(dot(geometricNormal, planarNormal))", self.object3d_ps)
        self.assertIn("worldPosition + waterReflectionOffset", self.object3d_ps)

    def test_water_uses_fresnel_for_planar_reflection(self) -> None:
        self.assertIn("waterFresnelF0", self.object3d_ps)
        self.assertIn("ComputeFresnelSchlick", self.object3d_ps)
        self.assertIn("strength *= lerp(0.20f, 1.0f, waterFresnel);", self.object3d_ps)

    def test_water_component_drives_object3d_water_state(self) -> None:
        self.assertIn("SetWaterSurfaceState", self.component)
        self.assertIn("SetWaterSurfaceState", self.object3d)
        self.assertIn("waterTime_ +=", self.component)


if __name__ == "__main__":
    unittest.main()
