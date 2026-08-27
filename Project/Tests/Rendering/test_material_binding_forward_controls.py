from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MATERIAL_BINDING = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "MaterialBinding.cpp"
MODEL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.cpp"
INSTANCED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "InstancedModelComponent.cpp"
ANIMATED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponent.cpp"
SKELETAL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponent.cpp"


class MaterialBindingForwardControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.binding = MATERIAL_BINDING.read_text(encoding="utf-8")
        cls.components = [
            MODEL_COMPONENT.read_text(encoding="utf-8"),
            INSTANCED_COMPONENT.read_text(encoding="utf-8"),
            ANIMATED_COMPONENT.read_text(encoding="utf-8"),
            SKELETAL_COMPONENT.read_text(encoding="utf-8"),
        ]

    def test_component_override_exposes_all_forward_blend_modes(self) -> None:
        self.assertIn('const char* blendModeNames[] = { "Opaque", "Masked", "Transparent", "Additive" }', self.binding)
        self.assertIn('ImGui::Combo("Blend Mode"', self.binding)
        self.assertIn("desc.blendMode = static_cast<MaterialBlendMode>(blendModeIndex)", self.binding)

    def test_component_override_exposes_surface_cull_modes(self) -> None:
        self.assertIn('const char* cullModeNames[] = { "Back", "Front", "None (Two Sided)" }', self.binding)
        self.assertIn('ImGui::Combo("Cull Mode"', self.binding)
        self.assertIn("desc.cullMode = static_cast<MaterialCullMode>(cullModeIndex)", self.binding)

    def test_override_json_persists_forward_surface_contract(self) -> None:
        self.assertIn('constexpr const char* kCullModeKey = "cullMode"', self.binding)
        self.assertIn('constexpr const char* kBlendModeKey = "blendMode"', self.binding)
        self.assertIn("MaterialDescJsonConverter::ToString(desc.cullMode)", self.binding)
        self.assertIn("MaterialDescJsonConverter::ToString(desc.blendMode)", self.binding)
        self.assertIn("MaterialDescJsonConverter::CullModeFromString", self.binding)
        self.assertIn("MaterialDescJsonConverter::BlendModeFromString", self.binding)

    def test_old_actor_json_keeps_safe_defaults(self) -> None:
        self.assertIn('JsonReadUtil::ReadStringOr(json, kCullModeKey, "back")', self.binding)
        self.assertIn('JsonReadUtil::ReadStringOr(json, kBlendModeKey, "opaque")', self.binding)

    def test_all_forward_surface_components_use_shared_material_binding_ui(self) -> None:
        for component in self.components:
            self.assertIn("DrawMaterialBindingImGui(materialBinding_", component)


if __name__ == "__main__":
    unittest.main()
