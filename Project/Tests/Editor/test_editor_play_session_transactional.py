from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
PLAY_SESSION = ROOT / "Engine/Editor/EditorPlaySessionManager.h"
ACTOR_WORLD = ROOT / "Engine/Scene/Actor/Core/ActorWorld.cpp"


class EditorPlaySessionTransactionalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.play_session = PLAY_SESSION.read_text(encoding="utf-8")
        cls.actor_world = ACTOR_WORLD.read_text(encoding="utf-8")

    def test_pie_stages_all_actors_before_world_commit(self) -> None:
        self.assertIn("std::vector<std::unique_ptr<Actor>> stagedActors", self.play_session)
        self.assertIn("ActorJsonSerializer::CreateActorFromJson(actorSnapshot.actorJson, spawnOptions)", self.play_session)
        self.assertIn("spawnOptions.disableAutoRegisterMainCamera = true", self.play_session)
        self.assertIn("CommitStagedActors(std::move(stagedActors), &committedActors)", self.play_session)

    def test_pie_does_not_destroy_world_before_staging(self) -> None:
        replace_start = self.play_session.index("bool ReplaceWorldFromSnapshot")
        replace_end = self.play_session.index("void SetStatus", replace_start)
        replace_body = self.play_session[replace_start:replace_end]
        self.assertNotIn("actorWorld->Finalize()", replace_body)
        self.assertNotIn("SpawnActorFromJson", replace_body)
        self.assertLess(replace_body.index("CreateActorFromJson"), replace_body.index("CommitStagedActors"))

    def test_failed_staging_explicitly_preserves_editor_world(self) -> None:
        self.assertIn("FinalizeStagedActors(stagedActors)", self.play_session)
        self.assertIn("Editor Worldは維持されます", self.play_session)
        self.assertIn("DescribeSnapshotActor", self.play_session)
        self.assertIn("lastReplaceError_", self.play_session)

    def test_editor_transient_state_is_reset_only_after_commit(self) -> None:
        replace_start = self.play_session.index("bool ReplaceWorldFromSnapshot")
        replace_end = self.play_session.index("void SetStatus", replace_start)
        replace_body = self.play_session[replace_start:replace_end]
        commit_index = replace_body.index("CommitStagedActors")
        reset_index = replace_body.index("ResetTransientState")
        state_restore_index = replace_body.index("EditorActorStateRegistry::GetInstance()->SetState")
        self.assertLess(commit_index, reset_index)
        self.assertLess(reset_index, state_restore_index)

    def test_actor_world_commit_has_no_failure_path_after_destructive_point(self) -> None:
        commit_start = self.actor_world.index("bool ActorWorld::CommitStagedActors")
        commit_end = self.actor_world.index("bool ActorWorld::AppendStagedActors", commit_start)
        commit_body = self.actor_world[commit_start:commit_end]
        self.assertIn("Finalize();", commit_body)
        self.assertIn("Initialize();", commit_body)
        self.assertIn("ここから先は検証済みActorの所有権移動だけなので失敗経路を持たせない", commit_body)


if __name__ == "__main__":
    unittest.main()
