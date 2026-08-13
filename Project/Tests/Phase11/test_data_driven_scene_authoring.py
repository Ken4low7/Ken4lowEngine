from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DATA_SCENE = PROJECT_ROOT / "Engine" / "Scene" / "Management" / "DataDrivenScene.h"
DEFINITION = PROJECT_ROOT / "Engine" / "Scene" / "Management" / "SceneDefinition.h"
REGISTRY = PROJECT_ROOT / "Engine" / "Scene" / "Management" / "SceneDefinitionRegistry.h"
SERIALIZER = PROJECT_ROOT / "Engine" / "Scene" / "Management" / "SceneDefinitionSerializer.cpp"
FACTORY = PROJECT_ROOT / "ApplicationLayer" / "SceneManagement" / "SceneFactory" / "SceneFactory.cpp"


class DataDrivenSceneAuthoringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.data_scene = DATA_SCENE.read_text(encoding="utf-8")
        cls.definition = DEFINITION.read_text(encoding="utf-8")
        cls.registry = REGISTRY.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER.read_text(encoding="utf-8")
        cls.factory = FACTORY.read_text(encoding="utf-8")

    def test_generic_scene_owns_editable_actor_world(self) -> None:
        self.assertIn("class DataDrivenScene final : public BaseScene", self.data_scene)
        self.assertIn("ActorWorld actorWorld_", self.data_scene)
        self.assertIn("GetEditorActorWorld", self.data_scene)
        self.assertIn("sceneDefinition_.id", self.data_scene)

    def test_scene_class_is_optional_for_normal_scene_json(self) -> None:
        self.assertIn('std::string className = "DataDrivenScene"', self.definition)
        self.assertIn('json.value("Class", std::string("DataDrivenScene"))', self.serializer)

    def test_level_json_is_auto_discovered_as_scene(self) -> None:
        # Save Level Asだけで新しい通常Sceneを作れる契約を固定する。
        self.assertIn('sceneDirectory.parent_path() / "Levels"', self.registry)
        self.assertIn('definition.className = "DataDrivenScene"', self.registry)
        self.assertIn("definition.levelPath = path.generic_string()", self.registry)
        self.assertIn("RefreshDiscoveredLevelScenes(); // Save Level As直後", self.registry)

    def test_factory_has_single_generic_runtime_class(self) -> None:
        self.assertIn('K4E_REGISTER_SCENE_NAMED("DataDrivenScene"', self.factory)
        self.assertIn("DataDrivenScene.h", self.factory)


if __name__ == "__main__":
    unittest.main()
