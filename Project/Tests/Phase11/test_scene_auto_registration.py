from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
FACTORY_HEADER = PROJECT_ROOT / "ApplicationLayer" / "SceneManagement" / "SceneFactory" / "SceneFactory.h"
FACTORY_SOURCE = FACTORY_HEADER.with_suffix(".cpp")
SCENE_MANAGER_HEADER = PROJECT_ROOT / "Engine" / "Scene" / "Management" / "SceneManager.h"
EDITOR_WINDOW_SOURCE = PROJECT_ROOT / "Engine" / "Editor" / "EditorWindowManager.cpp"
GAME_APPLICATION_SOURCE = PROJECT_ROOT / "Engine" / "Core" / "Application" / "GameApplication.cpp"
SAMPLE_SCENE_HEADER = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "SampleScene" / "SampleScene.h"


class SceneAutoRegistrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.factory_h = FACTORY_HEADER.read_text(encoding="utf-8")
        cls.factory_cpp = FACTORY_SOURCE.read_text(encoding="utf-8")
        cls.scene_manager_h = SCENE_MANAGER_HEADER.read_text(encoding="utf-8")
        cls.editor_cpp = EDITOR_WINDOW_SOURCE.read_text(encoding="utf-8")
        cls.game_application_cpp = GAME_APPLICATION_SOURCE.read_text(encoding="utf-8")
        cls.sample_scene_h = SAMPLE_SCENE_HEADER.read_text(encoding="utf-8")

    def test_factory_supports_scene_self_registration(self) -> None:
        # Scene追加時にEditor側のボタン一覧を手書きしない登録契約を固定する。
        self.assertIn("RegisterSceneClass", self.factory_h)
        self.assertIn("K4E_REGISTER_SCENE", self.factory_h)
        self.assertIn("GetRegisteredSceneNames", self.factory_h)
        self.assertIn("GetSceneCreators().contains", self.factory_cpp)

    def test_scene_manager_only_exposes_creatable_scenes(self) -> None:
        self.assertIn("GetAvailableSceneIds", self.scene_manager_h)
        self.assertIn("CanCreateScene", self.scene_manager_h)
        self.assertIn("representedClasses", self.scene_manager_h)

    def test_scene_window_is_data_driven(self) -> None:
        self.assertIn("GetAvailableSceneIds", self.editor_cpp)
        self.assertNotIn('ImGui::Button("TitleScene")', self.editor_cpp)
        self.assertNotIn('ImGui::Button("StageSelectScene")', self.editor_cpp)
        self.assertNotIn('ImGui::Button("GamePlayScene")', self.editor_cpp)
        self.assertNotIn('ImGui::Button("DebugScene")', self.editor_cpp)

    def test_sample_scene_is_editable_actor_world_scene(self) -> None:
        self.assertIn("class SampleScene final : public BaseScene", self.sample_scene_h)
        self.assertIn("GetEditorActorWorld", self.sample_scene_h)
        self.assertIn("CollectActorWorldEditorObjects", self.sample_scene_h)
        self.assertIn('K4E_REGISTER_SCENE_NAMED("SampleScene"', self.factory_cpp)

    def test_scene_switch_uses_gpu_safe_transition(self) -> None:
        # Draw中に要求されたScene切替が次Updateまで旧Sceneを保持する契約を固定する。
        self.assertIn("GpuSafeSceneTransition", self.game_application_cpp)
        self.assertIn("SignalAndGetValue", self.game_application_cpp)
        self.assertIn("WaitForValue", self.game_application_cpp)
        self.assertIn("SetSceneTransition(std::move(safeSceneTransition))", self.game_application_cpp)
        startup = self.game_application_cpp.index("sceneManager_->ChangeScene(startSceneName);")
        install = self.game_application_cpp.index("sceneManager_->SetSceneTransition(std::move(safeSceneTransition))")
        self.assertLess(startup, install)


if __name__ == "__main__":
    unittest.main()
