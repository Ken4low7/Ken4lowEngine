from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MATERIAL_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.h"
MATERIAL_CPP = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.cpp"
MATERIAL_BINDING = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "MaterialBinding.cpp"
OBJECT3D_PS = PROJECT_ROOT / "Resources" / "Shaders" / "Object3D" / "Object3d.PS.hlsl"
SKINNING_PS = PROJECT_ROOT / "Resources" / "Shaders" / "Skinning" / "SkinningObject3d.PS.hlsl"


class MaterialEnvironmentOptInTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.material_header = MATERIAL_HEADER.read_text(encoding="utf-8")
        cls.material_cpp = MATERIAL_CPP.read_text(encoding="utf-8")
        cls.material_binding = MATERIAL_BINDING.read_text(encoding="utf-8")
        cls.object3d_ps = OBJECT3D_PS.read_text(encoding="utf-8")
        cls.skinning_ps = SKINNING_PS.read_text(encoding="utf-8")

    def test_legacy_material_defaults_to_no_environment_reflection(self) -> None:
        self.assertIn("float reflection = 0.0f;", self.material_header)
        self.assertIn("materialData_->reflection = 0.0f;", self.material_cpp)

    def test_legacy_shaders_require_explicit_reflection(self) -> None:
        for shader in (self.object3d_ps, self.skinning_ps):
            self.assertIn("const float reflectionRate = saturate(gMaterial.reflectionRate);", shader)
            self.assertIn("if (reflectionRate > 0.0f)", shader)
            self.assertNotIn("gMaterial.reflectionRate * 0.12f + fresnel * 0.03f", shader)
            self.assertIn("reflectionRate * (0.12f + fresnel * 0.03f)", shader)

    def test_legacy_roughness_controls_environment_mip(self) -> None:
        for shader in (self.object3d_ps, self.skinning_ps):
            self.assertIn("gEnvironmentTexture.GetDimensions", shader)
            self.assertIn("saturate(gMaterial.roughness) * maxMipLevel", shader)
            self.assertIn("SampleLevel", shader)

    def test_shininess_setter_updates_shininess_not_reflection(self) -> None:
        self.assertIn(
            "void SetShininess(float shininess) { materialData_->shininess = shininess; }",
            self.material_header,
        )
        self.assertNotIn(
            "void SetShininess(float shininess) { materialData_->reflection = shininess; }",
            self.material_header,
        )

    def test_component_override_exposes_legacy_reflection_controls(self) -> None:
        self.assertIn("Component固有Materialで上書き", self.material_binding)
        self.assertIn('ImGui::DragFloat("反射率", &desc.legacy.reflection', self.material_binding)
        self.assertIn('ImGui::DragFloat("粗さ##Legacy", &desc.legacy.roughness', self.material_binding)


if __name__ == "__main__":
    unittest.main()
