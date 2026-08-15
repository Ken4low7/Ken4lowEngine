from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
FORWARD_QUEUE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Forward" / "ForwardRenderQueue.h"
OBJECT3D_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3D.h"
OBJECT3D_COMMON_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DCommon.h"
OBJECT3D_COMMON_CPP = OBJECT3D_COMMON_H.with_suffix(".cpp")
INSTANCED_PIPELINE_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "InstancedObject3DPipelineSet.h"
INSTANCED_PIPELINE_CPP = INSTANCED_PIPELINE_H.with_suffix(".cpp")
INSTANCED_RENDERER_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "InstancedObject3DRenderer.h"
INSTANCED_RENDERER_CPP = INSTANCED_RENDERER_H.with_suffix(".cpp")
MODEL_COMPONENT_CPP = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.cpp"
INSTANCED_COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "InstancedModelComponent.h"
INSTANCED_COMPONENT_CPP = INSTANCED_COMPONENT_H.with_suffix(".cpp")
ACTOR_WORLD_DRAW = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Core" / "ActorWorld_Draw.cpp"


class ForwardInstancedIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.forward_queue = FORWARD_QUEUE.read_text(encoding="utf-8")
        cls.object3d_h = OBJECT3D_H.read_text(encoding="utf-8")
        cls.object3d_common_h = OBJECT3D_COMMON_H.read_text(encoding="utf-8")
        cls.object3d_common_cpp = OBJECT3D_COMMON_CPP.read_text(encoding="utf-8")
        cls.instanced_pipeline_h = INSTANCED_PIPELINE_H.read_text(encoding="utf-8")
        cls.instanced_pipeline_cpp = INSTANCED_PIPELINE_CPP.read_text(encoding="utf-8")
        cls.instanced_renderer_h = INSTANCED_RENDERER_H.read_text(encoding="utf-8")
        cls.instanced_renderer_cpp = INSTANCED_RENDERER_CPP.read_text(encoding="utf-8")
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

    def test_instanced_renderer_exposes_forward_material_classification_and_group_depth(self) -> None:
        self.assertIn("MaterialBlendMode GetBlendMode() const", self.instanced_renderer_h)
        self.assertIn("float CalculateForwardSortDepth() const", self.instanced_renderer_h)
        self.assertIn("InstancedObject3DRenderer::CalculateForwardSortDepth() const", self.instanced_renderer_cpp)
        self.assertIn("GetActiveCameraForward()", self.instanced_renderer_cpp)

    def test_instanced_pipeline_has_alpha_and_additive_depth_read_only_variants(self) -> None:
        self.assertIn("GetAlpha", self.instanced_pipeline_h)
        self.assertIn("GetAdditive", self.instanced_pipeline_h)
        self.assertIn('L"Object3D.Instanced.Alpha.Back"', self.instanced_pipeline_cpp)
        self.assertIn('L"Object3D.Instanced.Additive.Back"', self.instanced_pipeline_cpp)
        self.assertIn("MakeBlendAlpha()", self.instanced_pipeline_cpp)
        self.assertIn("MakeBlendAdditive()", self.instanced_pipeline_cpp)
        self.assertIn("MakeDepthReadOnly()", self.instanced_pipeline_cpp)
        self.assertIn("SetInstancedAlphaRenderSetting", self.object3d_common_h)
        self.assertIn("SetInstancedAdditiveRenderSetting", self.object3d_common_h)
        self.assertIn("instancedPipelineSet_.GetAlpha(cullMode)", self.object3d_common_cpp)
        self.assertIn("instancedPipelineSet_.GetAdditive(cullMode)", self.object3d_common_cpp)

    def test_transparent_instances_sort_only_main_stream_back_to_front(self) -> None:
        self.assertIn("visibleInstanceScratch_", self.instanced_renderer_h)
        self.assertIn("const bool backToFront", self.instanced_renderer_cpp)
        self.assertIn("MaterialBlendMode::Transparent", self.instanced_renderer_cpp)
        self.assertIn("MaterialBlendMode::Additive", self.instanced_renderer_cpp)
        self.assertIn("std::stable_sort(visibleInstanceScratch_", self.instanced_renderer_cpp)
        self.assertIn("return lhsDepth > rhsDepth", self.instanced_renderer_cpp)
        self.assertIn("std::copy_n(visibleInstanceScratch_.begin(), instanceCount_, stream->mappedInstances)", self.instanced_renderer_cpp)
        self.assertNotIn("std::stable_sort(sourceInstances_", self.instanced_renderer_cpp)

    def test_instanced_renderer_selects_surface_pso_from_material_blend_mode(self) -> None:
        self.assertIn("switch (material_.GetBlendMode())", self.instanced_renderer_cpp)
        self.assertIn("SetInstancedAlphaRenderSetting(cullMode)", self.instanced_renderer_cpp)
        self.assertIn("SetInstancedAdditiveRenderSetting(cullMode)", self.instanced_renderer_cpp)
        self.assertIn("SetInstancedRenderSetting(cullMode)", self.instanced_renderer_cpp)

    def test_instanced_component_submits_all_forward_material_buckets(self) -> None:
        for method in (
            "SubmitForwardOpaque",
            "SubmitForwardMasked",
            "SubmitForwardTransparent",
            "SubmitForwardAdditive",
        ):
            self.assertIn(method, self.instanced_component_h)
        self.assertIn("SubmitForwardBucket", self.instanced_component_h)
        self.assertIn("renderer_->GetBlendMode() != expectedBlendMode", self.instanced_component_cpp)
        self.assertIn("renderer_->CalculateForwardSortDepth()", self.instanced_component_cpp)
        self.assertIn("MakeForwardRenderItem", self.instanced_component_cpp)
        self.assertNotIn("expectedBlendMode != MaterialBlendMode::Opaque", self.instanced_component_cpp)

    def test_instanced_component_avoids_double_draw_after_queue_submission(self) -> None:
        self.assertIn("lastForwardQueueSerial_", self.instanced_component_h)
        self.assertIn("queue->IsFrameActive()", self.instanced_component_cpp)
        self.assertIn("queue->GetFrameSerial() == lastForwardQueueSerial_", self.instanced_component_cpp)

    def test_actor_world_collects_static_and_instanced_all_buckets(self) -> None:
        self.assertIn('#include "InstancedModelComponent.h"', self.actor_world_draw)
        self.assertIn("GetComponents<InstancedModelComponent>()", self.actor_world_draw)
        for method in (
            "SubmitForwardOpaque",
            "SubmitForwardMasked",
            "SubmitForwardTransparent",
            "SubmitForwardAdditive",
        ):
            self.assertIn(f"instancedModelComponent->{method}(*forwardQueue)", self.actor_world_draw)

        opaque_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Opaque)")
        masked_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Masked)")
        legacy_draw = self.actor_world_draw.index("actor->Draw()")
        transparent_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Transparent)")
        additive_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Additive)")
        self.assertLess(opaque_execute, masked_execute)
        self.assertLess(masked_execute, legacy_draw)
        self.assertLess(legacy_draw, transparent_execute)
        self.assertLess(transparent_execute, additive_execute)


if __name__ == "__main__":
    unittest.main()
