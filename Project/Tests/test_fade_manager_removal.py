from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
GAME_APPLICATION = PROJECT_ROOT / "Engine" / "Core" / "Application" / "GameApplication.cpp"
SCENE_MANAGER = PROJECT_ROOT / "Engine" / "Scene" / "Management" / "SceneManager.cpp"
BUILD_TARGETS = PROJECT_ROOT / "Directory.Build.targets"
FADE_DIR = PROJECT_ROOT / "ApplicationLayer" / "SceneManagement" / "FadeManager"
CRACK_SOURCE = PROJECT_ROOT / "Resources" / "Textures" / "Sources" / "Effects" / "CrackAtlas.png"
CRACK_OUTPUT = PROJECT_ROOT / "Resources" / "Textures" / "Compiled" / "Effects" / "CrackAtlas.dds"
BLACK_SOURCE = PROJECT_ROOT / "Resources" / "Textures" / "Sources" / "Effects" / "black.png"
BLACK_OUTPUT = PROJECT_ROOT / "Resources" / "Textures" / "Compiled" / "Effects" / "black.dds"


class FadeManagerRemovalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.application = GAME_APPLICATION.read_text(encoding="utf-8")
        cls.scene_manager = SCENE_MANAGER.read_text(encoding="utf-8")
        cls.build_targets = BUILD_TARGETS.read_text(encoding="utf-8")

    def test_application_no_longer_creates_fade_manager(self) -> None:
        # FadeManagerをApplication起動経路へ戻す退行を防ぐ。
        self.assertNotIn("#include <FadeManager.h>", self.application)
        self.assertNotIn("make_unique<FadeManager>", self.application)
        self.assertNotIn("SetSceneTransition(std::make_unique<FadeManager>", self.application)

    def test_fade_manager_sources_are_physically_removed(self) -> None:
        self.assertFalse((FADE_DIR / "FadeManager.h").exists())
        self.assertFalse((FADE_DIR / "FadeManager.cpp").exists())
        self.assertIn('ClCompile Remove="ApplicationLayer\\SceneManagement\\FadeManager\\FadeManager.cpp"', self.build_targets)
        self.assertIn('ClInclude Remove="ApplicationLayer\\SceneManagement\\FadeManager\\FadeManager.h"', self.build_targets)

    def test_fade_only_texture_assets_are_removed(self) -> None:
        self.assertFalse(CRACK_SOURCE.exists())
        self.assertFalse(CRACK_OUTPUT.exists())
        self.assertFalse(BLACK_SOURCE.exists())
        self.assertFalse(BLACK_OUTPUT.exists())

    def test_scene_manager_keeps_no_transition_fast_path(self) -> None:
        self.assertIn("if (!sceneTransition_)", self.scene_manager)
        self.assertIn("ApplyNextScene();", self.scene_manager)


if __name__ == "__main__":
    unittest.main()
