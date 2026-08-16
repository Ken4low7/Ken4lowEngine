from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ENGINE_ROOT = PROJECT_ROOT / "Engine"


class EditorPlaceActorsTests(unittest.TestCase):
    def test_place_actor_service_supports_all_palette_entries(self) -> None:
        service = ENGINE_ROOT / "Editor" / "EditorAssetPlacementService.h"
        text = service.read_text(encoding="utf-8")

        required_tokens = [
            "EditorPlaceableType::EmptyActor",
            "EditorPlaceableType::Cube",
            "EditorPlaceableType::Sphere",
            "EditorPlaceableType::Plane",
            "EditorPlaceableType::DirectionalLight",
            "EditorPlaceableType::PointLight",
            "EditorPlaceableType::SpotLight",
            "EditorPlaceableType::TriggerBox",
            "EditorPlaceableType::TriggerSphere",
            "EditorPlaceableType::ActorPrefab",
        ]
        for token in required_tokens:
            self.assertIn(token, text)

    def test_viewport_click_routes_pending_placement_before_gpu_picking(self) -> None:
        panel = ENGINE_ROOT / "Editor" / "EditorContentBrowserPanel.h"
        text = panel.read_text(encoding="utf-8")

        place_index = text.index("EditorAssetPlacementService::PlaceActor")
        pick_index = text.index("EditorGpuPickingManager::GetInstance()->RequestPick")
        self.assertLess(place_index, pick_index)
        self.assertIn("editorContext->ClearPlacementRequest()", text)

    def test_placement_supports_cancel_and_undo_snapshot(self) -> None:
        panel = ENGINE_ROOT / "Editor" / "EditorContentBrowserPanel.h"
        service = ENGINE_ROOT / "Editor" / "EditorAssetPlacementService.h"
        panel_text = panel.read_text(encoding="utf-8")
        service_text = service.read_text(encoding="utf-8")

        self.assertIn("ImGuiKey_Escape", panel_text)
        self.assertIn("ImGuiMouseButton_Right", panel_text)
        self.assertIn("RecordPlacementCommand", service_text)
        self.assertIn("MarkLevelDirty", service_text)

    def test_place_actors_lists_actor_prefabs_and_preserves_viewport_display_switch(self) -> None:
        shell = ENGINE_ROOT / "Editor" / "EditorShell.h"
        context = ENGINE_ROOT / "Editor" / "EditorContext.h"
        service = ENGINE_ROOT / "Editor" / "EditorAssetPlacementService.h"
        shell_text = shell.read_text(encoding="utf-8")
        context_text = context.read_text(encoding="utf-8")
        service_text = service.read_text(encoding="utf-8")

        self.assertIn('std::filesystem::path prefabDirectory{ "Resources/ActorPrefabs" }', shell_text)
        self.assertIn('ImGui::CollapsingHeader("プリファブ"', shell_text)
        self.assertIn("QueuePrefabPlacement", shell_text)
        self.assertIn("QueuePrefabPlacement", context_text)
        self.assertIn("SpawnActorFromJson(request.assetPath", service_text)

        combo_index = shell_text.index('ImGui::BeginCombo("##ビューポート表示"')
        tool_gate_index = shell_text.index("if (viewportWidth >= 560.0f)")
        self.assertLess(combo_index, tool_gate_index)
        self.assertNotIn("if (viewportWidth >= 850.0f)", shell_text)


if __name__ == "__main__":
    unittest.main()
