from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
FORWARD_QUEUE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Forward" / "ForwardRenderQueue.h"
OBJECT3D_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3D.h"
INSTANCED_RENDERER_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "InstancedObject3DRenderer.h"
MODEL_COMPONENT_CPP = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.cpp"
INSTANCED_COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "InstancedModelComponent.h"
INSTANCED_COMPONENT_CPP = INSTANCED_COMPONENT_H.with_suffix(".cpp")
ACTOR_WORLD_DRAW = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Core" / "ActorWorld_Draw.cpp"


class ForwardInstancedIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.forward_queue = FORWARD_QUEUE.read_text(encoding="utf-8")
        cls.object3d_h = OBJECT3D_H.read_text(encoding="utf-8")
        cls.instanced_renderer_h = INSTANCED_RENDERER_H.read_text(encoding="utf-8")
        cls.model_component_cpp = MODEL_COMPONENT_CPP.read_text(encoding="utf-8")
        cls.instanced_component_h = INSTANCED_COMPONENT_H.read_text(encoding="utf-8")
        cls.instanced_component_cpp = INSTANCED_COMPONENT_CPP.read_text(encoding="utf-8")
        cls.actor_world_draw = ACTOR_WORLD_DRAW.read_text(encoding="utf-8")

    def test_forward_item_builder_is_renderer_agnostic(self) -> None:
        self.assertIn("MakeForwardRenderItem", self.forward_queue)
        self.assertIn("ResolveForwardRenderPolicy(blendMode)", self.forward_queue)
        self.assertIn("ForwardRenderDrawCallback draw", self.forward_queue)

    def test_legacy_alpha_api_updates_material_classification(self) -> None:
        self.assertIn("void SetAlphaBlendEnabled(bool enabled)", self.object3d_h)
        self.assertIn("material_.SetBlendMode(MaterialBlendMode::Transparent)", self.object3d_h)
        self.assertIn("material_.SetBlendMode(MaterialBlendMode::Opaque)", self.object3d_h)
        self.assertNotIn("object3D_->IsAlphaBlendEnabled()", self.model_component_cpp)
        self.assertIn("MakeForwardRenderItem", self.model_component_cpp)

    def test_instanced_renderer_exposes_forward_material_classification(self) -> None:
        self.assertIn("MaterialBlendMode GetBlendMode() const", self.instanced_renderer_h)
        self.assertIn("material_.GetBlendMode()", self.instanced_renderer_h)

    def test_instanced_component_submits_only_depth_writing_buckets(self) -> None:
        self.assertIn("SubmitForwardOpaque", self.instanced_component_h)
        self.assertIn("SubmitForwardMasked", self.instanced_component_h)
        self.assertIn("SubmitForwardBucket", self.instanced_component_h)
        self.assertIn("expectedBlendMode != MaterialBlendMode::Opaque", self.instanced_component_cpp)
        self.assertIn("expectedBlendMode != MaterialBlendMode::Masked", self.instanced_component_cpp)
        self.assertIn("renderer_->GetBlendMode() != expectedBlendMode", self.instanced_component_cpp)
        self.assertIn("MakeForwardRenderItem", self.instanced_component_cpp)
        self.assertNotIn("SubmitForwardTransparent", self.instanced_component_h)
        self.assertNotIn("SubmitForwardAdditive", self.instanced_component_h)

    def test_instanced_component_avoids_double_draw_after_queue_submission(self) -> None:
        self.assertIn("lastForwardQueueSerial_", self.instanced_component_h)
        self.assertIn("queue->IsFrameActive()", self.instanced_component_cpp)
        self.assertIn("queue->GetFrameSerial() == lastForwardQueueSerial_", self.instanced_component_cpp)

    def test_actor_world_collects_static_and_instanced_depth_buckets(self) -> None:
        self.assertIn('#include "InstancedModelComponent.h"', self.actor_world_draw)
        self.assertIn("GetComponents<InstancedModelComponent>()", self.actor_world_draw)
        self.assertIn("instancedModelComponent->SubmitForwardOpaque(*forwardQueue)", self.actor_world_draw)
        self.assertIn("instancedModelComponent->SubmitForwardMasked(*forwardQueue)", self.actor_world_draw)

        opaque_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Opaque)")
        masked_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Masked)")
        legacy_draw = self.actor_world_draw.index("actor->Draw()")
        transparent_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Transparent)")
        self.assertLess(opaque_execute, masked_execute)
        self.assertLess(masked_execute, legacy_draw)
        self.assertLess(legacy_draw, transparent_execute)


if __name__ == "__main__":
    unittest.main()
