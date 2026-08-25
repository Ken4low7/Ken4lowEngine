from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ACTOR_WORLD_IMGUI = PROJECT_ROOT / "Engine" / "Editor" / "Legacy" / "ActorWorld_ImGui.cpp"


class AddComponentSelectionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = ACTOR_WORLD_IMGUI.read_text(encoding="utf-8")

    def test_add_component_uses_selection_after_combo_changes(self) -> None:
        preview_pos = self.source.index("const ComponentFactory::ComponentTypeInfo& previewType")
        combo_end_pos = self.source.index("ImGui::EndCombo();", preview_pos)
        selected_pos = self.source.index("const ComponentFactory::ComponentTypeInfo& selectedType", combo_end_pos)
        add_pos = self.source.index("AddComponentToSelectedActor(classNameToAdd);", selected_pos)

        self.assertLess(preview_pos, combo_end_pos)
        self.assertLess(combo_end_pos, selected_pos)
        self.assertLess(selected_pos, add_pos)
        self.assertIn("const std::string classNameToAdd = selectedType.className;", self.source[selected_pos:add_pos])

    def test_stale_selected_type_is_not_bound_before_combo(self) -> None:
        combo_pos = self.source.index('ImGui::BeginCombo("種類"')
        selected_pos = self.source.index("const ComponentFactory::ComponentTypeInfo& selectedType")
        self.assertGreater(selected_pos, combo_pos)


if __name__ == "__main__":
    unittest.main()
