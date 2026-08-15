from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MATERIAL_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.h"
MATERIAL_CPP = MATERIAL_H.with_suffix(".cpp")
MATERIAL_SOURCE_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "MaterialDescLoader.h"
MATERIAL_SOURCE_CPP = MATERIAL_SOURCE_H.with_suffix(".cpp")
MATERIAL_JSON_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "MaterialDescJsonConverter.h"
MATERIAL_JSON_CPP = MATERIAL_JSON_H.with_suffix(".cpp")
PIPELINE_PRESETS_H = PROJECT_ROOT / "Engine" / "Graphics" / "Pipeline" / "PipelineStatePresets.h"
PIPELINE_PRESETS_CPP = PIPELINE_PRESETS_H.with_suffix(".cpp")
FORWARD_QUEUE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Forward" / "ForwardRenderQueue.h"
OBJECT_PIPELINE_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DPipelineSet.h"
OBJECT_PIPELINE_CPP = OBJECT_PIPELINE_H.with_suffix(".cpp")
OBJECT_COMMON_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DCommon.h"
OBJECT_COMMON_CPP = OBJECT_COMMON_H.with_suffix(".cpp")
OBJECT3D_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3D.h"
OBJECT3D_CPP = OBJECT3D_H.with_suffix(".cpp")
MODEL_COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.h"
MODEL_COMPONENT_CPP = MODEL_COMPONENT_H.with_suffix(".cpp")
ACTOR_WORLD_DRAW = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Core" / "ActorWorld_Draw.cpp"


class ForwardRenderQueueFoundationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.material_h = MATERIAL_H.read_text(encoding="utf-8")
        cls.material_cpp = MATERIAL_CPP.read_text(encoding="utf-8")
        cls.material_source_h = MATERIAL_SOURCE_H.read_text(encoding="utf-8")
        cls.material_source_cpp = MATERIAL_SOURCE_CPP.read_text(encoding="utf-8")
        cls.material_json_h = MATERIAL_JSON_H.read_text(encoding="utf-8")
        cls.material_json_cpp = MATERIAL_JSON_CPP.read_text(encoding="utf-8")
        cls.pipeline_presets_h = PIPELINE_PRESETS_H.read_text(encoding="utf-8")
        cls.pipeline_presets_cpp = PIPELINE_PRESETS_CPP.read_text(encoding="utf-8")
        cls.forward_queue = FORWARD_QUEUE.read_text(encoding="utf-8")
        cls.object_pipeline_h = OBJECT_PIPELINE_H.read_text(encoding="utf-8")
        cls.object_pipeline_cpp = OBJECT_PIPELINE_CPP.read_text(encoding="utf-8")
        cls.object_common_h = OBJECT_COMMON_H.read_text(encoding="utf-8")
        cls.object_common_cpp = OBJECT_COMMON_CPP.read_text(encoding="utf-8")
        cls.object3d_h = OBJECT3D_H.read_text(encoding="utf-8")
        cls.object3d_cpp = OBJECT3D_CPP.read_text(encoding="utf-8")
        cls.model_component_h = MODEL_COMPONENT_H.read_text(encoding="utf-8")
        cls.model_component_cpp = MODEL_COMPONENT_CPP.read_text(encoding="utf-8")
        cls.actor_world_draw = ACTOR_WORLD_DRAW.read_text(encoding="utf-8")

    def test_material_owns_high_level_forward_blend_classification(self) -> None:
        self.assertIn("enum class MaterialBlendMode", self.material_h)
        for mode in ("Opaque", "Masked", "Transparent", "Additive"):
            self.assertIn(mode, self.material_h)
        self.assertIn("MaterialBlendMode blendMode = MaterialBlendMode::Opaque", self.material_h)
        self.assertIn("GetBlendMode()", self.material_h)
        self.assertIn("blendMode_ = desc.blendMode", self.material_cpp)
        self.assertIn("Blend Mode##Material", self.material_cpp)

    def test_material_source_and_json_preserve_blend_mode(self) -> None:
        self.assertIn("MaterialBlendMode blendMode = MaterialBlendMode::Opaque", self.material_source_h)
        self.assertIn("desc.blendMode = normalizedSource.blendMode", self.material_source_cpp)
        self.assertIn('static constexpr const char* BlendMode = "blendMode"', self.material_json_h)
        self.assertIn("BlendModeFromString", self.material_json_h)
        self.assertIn('Keys::BlendMode, "opaque"', self.material_json_cpp)
        self.assertIn("json[Keys::BlendMode] = ToString(normalized.blendMode)", self.material_json_cpp)
        self.assertIn("return MaterialBlendMode::Opaque", self.material_json_cpp)

    def test_forward_policy_maps_material_modes_to_stable_queue_contracts(self) -> None:
        self.assertIn("enum class ForwardRenderBucket", self.forward_queue)
        self.assertIn("enum class ForwardSortMode", self.forward_queue)
        self.assertIn("ResolveForwardRenderPolicy", self.forward_queue)
        self.assertIn("ForwardRenderBucket::Opaque, BlendMode::kBlendModeNone, ForwardSortMode::FrontToBack, true, false", self.forward_queue)
        self.assertIn("ForwardRenderBucket::Masked, BlendMode::kBlendModeNone, ForwardSortMode::FrontToBack, true, true", self.forward_queue)
        self.assertIn("ForwardRenderBucket::Transparent, BlendMode::kBlendModeNormal, ForwardSortMode::BackToFront, false, false", self.forward_queue)
        self.assertIn("ForwardRenderBucket::Additive, BlendMode::kBlendModeAdd, ForwardSortMode::BackToFront, false, false", self.forward_queue)

    def test_forward_queue_owns_stable_submit_sort_execute_lifecycle(self) -> None:
        self.assertIn("struct ForwardRenderItem", self.forward_queue)
        self.assertIn("class ForwardRenderQueue", self.forward_queue)
        self.assertIn("void BeginFrame()", self.forward_queue)
        self.assertIn("bool Submit(ForwardRenderItem item)", self.forward_queue)
        self.assertIn("std::stable_sort", self.forward_queue)
        self.assertIn("submissionOrder", self.forward_queue)
        self.assertIn("void ExecuteBucket(ForwardRenderBucket bucket)", self.forward_queue)
        self.assertIn("void EndFrame()", self.forward_queue)

    def test_object_renderer_has_distinct_alpha_and_additive_depth_read_only_pipelines(self) -> None:
        self.assertIn("MakeBlendAdditive", self.pipeline_presets_h)
        self.assertIn("D3D12_BLEND_DESC MakeBlendAdditive()", self.pipeline_presets_cpp)
        self.assertIn("DestBlend = D3D12_BLEND_ONE", self.pipeline_presets_cpp)
        self.assertIn("GetAdditive", self.object_pipeline_h)
        self.assertIn('L"Object3D.Additive.Back"', self.object_pipeline_cpp)
        self.assertIn("MakeBlendAdditive()", self.object_pipeline_cpp)
        self.assertIn("MakeDepthReadOnly()", self.object_pipeline_cpp)
        self.assertIn("SetAdditiveRenderSetting", self.object_common_h)
        self.assertIn("pipelineSet_.GetAdditive(cullMode)", self.object_common_cpp)

    def test_object3d_selects_surface_pso_from_material_blend_mode(self) -> None:
        self.assertIn("const MaterialBlendMode blendMode = material_.GetBlendMode()", self.object3d_cpp)
        self.assertIn("blendMode == MaterialBlendMode::Transparent", self.object3d_cpp)
        self.assertIn("blendMode == MaterialBlendMode::Additive", self.object3d_cpp)
        self.assertIn("SetAlphaRenderSetting(cullMode)", self.object3d_cpp)
        self.assertIn("SetAdditiveRenderSetting(cullMode)", self.object3d_cpp)

    def test_static_model_component_submits_all_forward_material_buckets(self) -> None:
        self.assertIn("GetBlendMode() const", self.object3d_h)
        self.assertIn("SubmitForwardOpaque", self.model_component_h)
        self.assertIn("SubmitForwardMasked", self.model_component_h)
        self.assertIn("SubmitForwardTransparent", self.model_component_h)
        self.assertIn("SubmitForwardAdditive", self.model_component_h)
        self.assertIn("SubmitForwardBucket", self.model_component_h)
        self.assertIn("object3D_->IsAlphaBlendEnabled()", self.model_component_cpp)
        self.assertIn("object3D_->GetBlendMode() != expectedBlendMode", self.model_component_cpp)
        self.assertIn("MaterialBlendMode::Opaque", self.model_component_cpp)
        self.assertIn("MaterialBlendMode::Masked", self.model_component_cpp)
        self.assertIn("MaterialBlendMode::Transparent", self.model_component_cpp)
        self.assertIn("MaterialBlendMode::Additive", self.model_component_cpp)
        self.assertIn("item.policy = ResolveForwardRenderPolicy(expectedBlendMode)", self.model_component_cpp)
        self.assertIn("item.sortDepth = CalculateForwardSortDepth", self.model_component_cpp)
        self.assertIn("lastForwardQueueSerial_ = queue.GetFrameSerial()", self.model_component_cpp)

    def test_queued_model_is_owned_by_forward_queue_for_the_entire_frame(self) -> None:
        self.assertIn("alreadySubmittedToForwardQueue", self.model_component_cpp)
        self.assertIn("queue->IsFrameActive()", self.model_component_cpp)
        self.assertIn("queue->GetFrameSerial() == lastForwardQueueSerial_", self.model_component_cpp)

    def test_actor_world_executes_transparent_then_additive_after_legacy_world_draws(self) -> None:
        begin_index = self.actor_world_draw.index("forwardQueue->BeginFrame()")
        opaque_submit_index = self.actor_world_draw.index("SubmitForwardOpaque")
        masked_submit_index = self.actor_world_draw.index("SubmitForwardMasked")
        transparent_submit_index = self.actor_world_draw.index("SubmitForwardTransparent")
        additive_submit_index = self.actor_world_draw.index("SubmitForwardAdditive")
        opaque_execute_index = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Opaque)")
        masked_execute_index = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Masked)")
        legacy_draw_index = self.actor_world_draw.index("actor->Draw()")
        transparent_execute_index = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Transparent)")
        additive_execute_index = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Additive)")
        end_index = self.actor_world_draw.index("forwardQueue->EndFrame()")
        self.assertLess(begin_index, opaque_submit_index)
        self.assertLess(opaque_submit_index, masked_submit_index)
        self.assertLess(masked_submit_index, transparent_submit_index)
        self.assertLess(transparent_submit_index, additive_submit_index)
        self.assertLess(additive_submit_index, opaque_execute_index)
        self.assertLess(opaque_execute_index, masked_execute_index)
        self.assertLess(masked_execute_index, legacy_draw_index)
        self.assertLess(legacy_draw_index, transparent_execute_index)
        self.assertLess(transparent_execute_index, additive_execute_index)
        self.assertLess(additive_execute_index, end_index)

    def test_forward_policy_is_compiled_by_object_renderer(self) -> None:
        self.assertIn("Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h", self.object_common_h)


if __name__ == "__main__":
    unittest.main()
