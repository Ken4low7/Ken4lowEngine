from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ANIMATED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponent.h"
SKELETAL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponent.h"
ACTOR_WORLD = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Core" / "ActorWorld.cpp"


class AnimationComponentEditorTickTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.animated = ANIMATED_COMPONENT.read_text(encoding="utf-8")
        cls.skeletal = SKELETAL_COMPONENT.read_text(encoding="utf-8")
        cls.actor_world = ACTOR_WORLD.read_text(encoding="utf-8")

    def test_actor_world_uses_editor_update_path(self) -> None:
        self.assertIn("actor->UpdateEditor(deltaTime)", self.actor_world)

    def test_animated_model_processes_reload_and_animation_in_editor(self) -> None:
        self.assertIn("void UpdateEditor(float deltaTime) override", self.animated)
        self.assertIn("ProcessReloadRequest();", self.animated)
        self.assertIn("animatedModel_->Update();", self.animated)

    def test_skeletal_mesh_processes_reload_and_skinning_in_editor(self) -> None:
        self.assertIn("void UpdateEditor(float deltaTime) override", self.skeletal)
        self.assertIn("ProcessReloadRequest();", self.skeletal)
        self.assertIn("animationModel_->Update();", self.skeletal)  # Editor追加後もReload Pendingで止めずSkinning更新まで進める。


if __name__ == "__main__":
    unittest.main()
