from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCENE_COMPONENT_HEADER = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SceneComponent.h"
SCENE_COMPONENT_SOURCE = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SceneComponent.cpp"
DEBUG_SCENE_HEADER = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "DebugScene" / "DebugScene.h"
DEBUG_SCENE_SOURCE = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "DebugScene" / "DebugScene.cpp"


class TransformDirtyTrackingContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = SCENE_COMPONENT_HEADER.read_text(encoding="utf-8")
        cls.source = SCENE_COMPONENT_SOURCE.read_text(encoding="utf-8")
        cls.debug_header = DEBUG_SCENE_HEADER.read_text(encoding="utf-8")
        cls.debug_source = DEBUG_SCENE_SOURCE.read_text(encoding="utf-8")

    def test_scene_component_tracks_self_and_subtree_dirty_state(self) -> None:
        self.assertIn("worldTransformDirty_", self.header)
        self.assertIn("subtreeTransformDirty_", self.header)
        self.assertIn("worldTransformRevision_", self.header)
        self.assertIn("lastParentWorldTransformRevision_", self.header)
        self.assertIn("IsWorldTransformDirty()", self.header)
        self.assertIn("IsTransformHierarchyDirty()", self.header)

    def test_all_local_transform_mutation_paths_mark_dirty(self) -> None:
        self.assertIn("SetLocalPosition", self.header)
        self.assertIn("SetLocalRotation", self.header)
        self.assertIn("SetLocalScale", self.header)
        self.assertGreaterEqual(self.header.count("MarkTransformDirty();"), 6)
        self.assertIn("if (transformChanged) MarkTransformDirty();", self.source)

        # Legacy mutable-reference access remains compatible, but taking that path dirties before mutation can happen.
        self.assertIn("Vector3& LocalPosition()", self.header)
        self.assertIn("Vector3& LocalRotation()", self.header)
        self.assertIn("Vector3& LocalScale()", self.header)

    def test_dirty_propagation_covers_descendants_and_ancestors(self) -> None:
        self.assertIn("MarkWorldTransformDirtyRecursive", self.source)
        self.assertIn("child->MarkWorldTransformDirtyRecursive()", self.source)
        self.assertIn("MarkSubtreeDirtyUpward", self.source)
        self.assertIn("component = component->parent_", self.source)
        self.assertIn("ancestor == this", self.source)

    def test_refresh_starts_from_hierarchy_root_before_recalculating_children(self) -> None:
        self.assertIn("SceneComponent* hierarchyRoot = this", self.source)
        self.assertIn("while (hierarchyRoot->parent_)", self.source)
        self.assertIn("return hierarchyRoot->UpdateWorldTransform()", self.source)
        self.assertIn("lastParentWorldTransformRevision_ != parentRevision", self.source)

    def test_clean_transform_hierarchy_skips_recalculation(self) -> None:
        self.assertIn("if (!subtreeTransformDirty_ && !worldTransformDirty_ && !parentChanged)", self.source)
        self.assertIn("return 0;", self.source)
        self.assertIn("++worldTransformRevision_", self.source)
        self.assertIn("recomputedCount += child->UpdateWorldTransform()", self.source)
        self.assertIn("subtreeTransformDirty_ = false", self.source)

    def test_scheduler_finalizes_dirty_transforms_before_physics_and_after_post_physics(self) -> None:
        self.assertIn("TransformFinalizeStats", self.debug_header)
        self.assertIn("FinalizeDirtyWorldTransforms", self.debug_header)
        self.assertIn('"ActorWorld.FinalizePrePhysicsTransforms"', self.debug_source)
        self.assertIn('"PhysicsWorld.Update"', self.debug_source)
        self.assertIn('"ActorWorld.FinalizePostPhysicsTransforms"', self.debug_source)
        self.assertLess(
            self.debug_source.index('"ActorWorld.FinalizePrePhysicsTransforms"'),
            self.debug_source.index('"PhysicsWorld.Update"'),
        )
        self.assertLess(
            self.debug_source.index('"ActorWorld.PostPhysicsUpdate"'),
            self.debug_source.index('"ActorWorld.FinalizePostPhysicsTransforms"'),
        )
        self.assertIn("kLocalTransformState", self.debug_source)
        self.assertIn("kWorldTransformState", self.debug_source)

    def test_debug_diagnostics_snapshot_dirty_count_before_flush(self) -> None:
        self.assertIn("dirtyComponentCount", self.debug_header)
        self.assertIn("recomputedComponentCount", self.debug_header)
        self.assertIn("std::vector<K4E::SceneComponent*> sceneComponents", self.debug_source)
        self.assertLess(
            self.debug_source.index("if (sceneComponent->IsWorldTransformDirty()) ++stats.dirtyComponentCount"),
            self.debug_source.index("for (K4E::SceneComponent* sceneComponent : sceneComponents)"),
        )
        self.assertIn("RefreshWorldTransformHierarchy()", self.debug_source)
        self.assertIn("Dirty Transform PrePhysics", self.debug_source)
        self.assertIn("Dirty Transform PostPhysics", self.debug_source)


if __name__ == "__main__":
    unittest.main()
