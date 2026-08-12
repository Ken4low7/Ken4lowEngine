from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
GAME_APPLICATION = PROJECT_ROOT / "Engine" / "Core" / "Application" / "GameApplication.cpp"
SCENE_MANAGER = PROJECT_ROOT / "Engine" / "Scene" / "Management" / "SceneManager.cpp"
FADE_HEADER = PROJECT_ROOT / "ApplicationLayer" / "SceneManagement" / "FadeManager" / "FadeManager.h"
FADE_SOURCE = PROJECT_ROOT / "ApplicationLayer" / "SceneManagement" / "FadeManager" / "FadeManager.cpp"


class FadeManagerRemovalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.application = GAME_APPLICATION.read_text(encoding="utf-8")
        cls.scene_manager = SCENE_MANAGER.read_text(encoding="utf-8")
        cls.fade_header = FADE_HEADER.read_text(encoding="utf-8")
        cls.fade_source = FADE_SOURCE.read_text(encoding="utf-8")

    def test_application_no_longer_creates_fade_manager(self) -> None:
        # FadeManagerをApplication起動経路へ戻す退行を防ぐ。
        self.assertNotIn("#include <FadeManager.h>", self.application)
        self.assertNotIn("make_unique<FadeManager>", self.application)
        self.assertNotIn("SetSceneTransition(std::make_unique<FadeManager>", self.application)

    def test_retired_stub_contains_no_fade_runtime_or_texture_paths(self) -> None:
        combined = self.fade_header + self.fade_source
        self.assertNotIn("class FadeManager", combined)
        self.assertNotIn("Stage/rock.dds", combined)
        self.assertNotIn("Effects/CrackAtlas.dds", combined)
        self.assertNotIn("Effects/black.dds", combined)
        self.assertNotIn("StartCover()", combined)

    def test_scene_manager_keeps_no_transition_fast_path(self) -> None:
        self.assertIn("if (!sceneTransition_)", self.scene_manager)
        self.assertIn("ApplyNextScene();", self.scene_manager)


if __name__ == "__main__":
    unittest.main()
