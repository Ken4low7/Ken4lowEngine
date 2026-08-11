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


if __name__ == "__main__":
    unittest.main()
