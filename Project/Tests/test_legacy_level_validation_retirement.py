from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
LEGACY_LEVEL_VALIDATION = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "DebugScene" / "Validation" / "LevelDataValidation.h"
LEGACY_IMPORT_VALIDATION = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "DebugScene" / "Validation" / "LevelImportValidation.h"
DEBUG_SCENE = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "DebugScene" / "DebugScene.cpp"
TRANSACTIONAL_LOADER = PROJECT_ROOT / "Engine" / "Scene" / "Level" / "TransactionalLevelLoader.cpp"


class LegacyLevelValidationRetirementTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.level_validation = LEGACY_LEVEL_VALIDATION.read_text(encoding="utf-8")
        cls.import_validation = LEGACY_IMPORT_VALIDATION.read_text(encoding="utf-8")
        cls.debug_scene = DEBUG_SCENE.read_text(encoding="utf-8")

    def test_removed_fps_level_is_not_loaded_at_debug_scene_startup(self) -> None:
        # 削除済みFPSリソースをDebugScene生成時に再び自動読込しないことを固定する。
        combined = self.level_validation + self.import_validation
        self.assertNotIn("fps_stage00.json", combined)
        self.assertNotIn("hajimarinoheigen.json", combined)
        self.assertNotIn("hajimarinoheigen.gltf", combined)
        self.assertNotIn("LevelLoader", self.level_validation)
        self.assertNotIn("BlenderSceneLoader", combined)
        self.assertNotIn("BlenderLevelImporter", combined)

    def test_debug_scene_compatibility_hooks_are_io_free(self) -> None:
        self.assertIn("levelDataValidation_.DrawImGui();", self.debug_scene)
        self.assertIn("levelImportValidation_.DrawImGui();", self.debug_scene)
        self.assertIn("void DrawImGui() const noexcept", self.level_validation)
        self.assertIn("void DrawImGui() const noexcept", self.import_validation)
        self.assertNotIn("Load(", self.level_validation)
        self.assertNotIn("Reload(", self.import_validation)

    def test_current_transactional_level_pipeline_is_kept(self) -> None:
        self.assertTrue(TRANSACTIONAL_LOADER.exists())


if __name__ == "__main__":
    unittest.main()
