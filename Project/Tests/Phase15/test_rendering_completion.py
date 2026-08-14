from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PIPELINE_PRESETS_H = PROJECT_ROOT / "Engine" / "Graphics" / "Pipeline" / "PipelineStatePresets.h"
PIPELINE_PRESETS_CPP = PIPELINE_PRESETS_H.with_suffix(".cpp")
MATERIAL_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.h"
MATERIAL_CPP = MATERIAL_H.with_suffix(".cpp")
MATERIAL_SOURCE_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "MaterialDescLoader.h"
MATERIAL_SOURCE_CPP = MATERIAL_SOURCE_H.with_suffix(".cpp")
MATERIAL_JSON_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "MaterialDescJsonConverter.h"
MATERIAL_JSON_CPP = MATERIAL_JSON_H.with_suffix(".cpp")
OBJECT_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DPipelineSet.cpp"
INSTANCED_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "InstancedObject3DPipelineSet.cpp"
SHADOW_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "ShadowCasterPipelineSet.h"
OBJECT_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3D.cpp"
OBJECT_HEADER = OBJECT_SOURCE.with_suffix(".h")
INSTANCED_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "InstancedObject3DRenderer.cpp"
INSTANCED_SHADOW = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "InstancedObject3DRendererShadow.inl"
ANIMATION_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Pipeline" / "AnimationPipelineBuilder.cpp"
ANIMATION_PIPELINE_H = ANIMATION_PIPELINE.with_suffix(".h")
ANIMATION_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Core" / "AnimationModel.cpp"
ANIMATION_SHADOW = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Core" / "AnimationModelShadow.inl"
OBJECT_ID_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "ObjectIdPipeline.h"
ASSIMP_LOADER = PROJECT_ROOT / "Engine" / "Graphics" / "Resource" / "Model" / "AssimpLoader.cpp"
MODEL_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Resource" / "Model" / "Model.h"
MODEL_SOURCE = MODEL_HEADER.with_suffix(".cpp")
PHASE_DOC = PROJECT_ROOT / "Docs" / "Phase15RenderingCompletion.md"


class RenderingCompletionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        paths = {
            "presets_h": PIPELINE_PRESETS_H,
            "presets_cpp": PIPELINE_PRESETS_CPP,
            "material_h": MATERIAL_H,
            "material_cpp": MATERIAL_CPP,
            "material_source_h": MATERIAL_SOURCE_H,
            "material_source_cpp": MATERIAL_SOURCE_CPP,
            "material_json_h": MATERIAL_JSON_H,
            "material_json_cpp": MATERIAL_JSON_CPP,
            "object_pipeline": OBJECT_PIPELINE,
            "instanced_pipeline": INSTANCED_PIPELINE,
            "shadow_pipeline": SHADOW_PIPELINE,
            "object_source": OBJECT_SOURCE,
            "object_header": OBJECT_HEADER,
            "instanced_source": INSTANCED_SOURCE,
            "instanced_shadow": INSTANCED_SHADOW,
            "animation_pipeline": ANIMATION_PIPELINE,
            "animation_pipeline_h": ANIMATION_PIPELINE_H,
            "animation_source": ANIMATION_SOURCE,
            "animation_shadow": ANIMATION_SHADOW,
            "object_id_pipeline": OBJECT_ID_PIPELINE,
            "assimp_loader": ASSIMP_LOADER,
            "model_header": MODEL_HEADER,
            "model_source": MODEL_SOURCE,
            "phase_doc": PHASE_DOC,
        }
        for name, path in paths.items():
            setattr(cls, name, path.read_text(encoding="utf-8"))

    def test_rasterizer_presets_define_all_cull_modes(self) -> None:
        self.assertIn("MakeRasterizerCullBack", self.presets_h)
        self.assertIn("MakeRasterizerCullFront", self.presets_h)
        self.assertIn("MakeRasterizerCullNone", self.presets_h)
        self.assertIn("D3D12_CULL_MODE_BACK", self.presets_cpp)
        self.assertIn("D3D12_CULL_MODE_FRONT", self.presets_cpp)
        self.assertIn("D3D12_CULL_MODE_NONE", self.presets_cpp)

    def test_winding_contract_is_explicit(self) -> None:
        self.assertGreaterEqual(self.presets_cpp.count("FrontCounterClockwise = FALSE"), 3)
        self.assertIn("時計回り", self.presets_cpp)

    def test_material_surface_contract_defaults_to_back(self) -> None:
        self.assertIn("enum class MaterialCullMode", self.material_h)
        self.assertIn("Back = 0", self.material_h)
        self.assertIn("Front", self.material_h)
        self.assertIn("None", self.material_h)
        self.assertIn("MaterialCullMode cullMode = MaterialCullMode::Back", self.material_h)
        self.assertIn("cullMode_ = MaterialCullMode::Back", self.material_cpp)
        self.assertIn('"None (Two Sided)"', self.material_cpp)

    def test_mirrored_world_winding_is_resolved_once(self) -> None:
        self.assertIn("CalculateWorldHandednessDeterminant", self.material_h)
        self.assertIn("ResolveMaterialCullModeForWorld", self.material_h)
        self.assertIn("MaterialCullMode::Front", self.material_h)
        self.assertIn("ResolveMaterialCullModeForWorld", self.object_source)

    def test_static_object_builds_back_front_and_two_sided_pipelines(self) -> None:
        self.assertIn("MakeMaterialRasterizer", self.object_pipeline)
        self.assertIn("MakeRasterizerCullBack", self.object_pipeline)
        self.assertIn("MakeRasterizerCullFront", self.object_pipeline)
        self.assertIn("MakeRasterizerCullNone", self.object_pipeline)
        self.assertIn("Object3D.Default.TwoSided", self.object_pipeline)
        self.assertIn("Object3D.Alpha.TwoSided", self.object_pipeline)
        self.assertIn("GetDefault(MaterialCullMode", self.object_pipeline)
        self.assertIn("GetAlpha(MaterialCullMode", self.object_pipeline)

    def test_instanced_pipeline_builds_material_cull_variants(self) -> None:
        self.assertIn("MakeMaterialRasterizer", self.instanced_pipeline)
        self.assertIn("Object3D.Instanced.Back", self.instanced_pipeline)
        self.assertIn("Object3D.Instanced.Front", self.instanced_pipeline)
        self.assertIn("Object3D.Instanced.TwoSided", self.instanced_pipeline)
        self.assertIn("ResolveEffectiveCullMode", self.instanced_source)
        self.assertIn("hasNormalHandedness && hasMirroredHandedness", self.instanced_source)
        self.assertIn("return MaterialCullMode::None", self.instanced_source)

    def test_shadow_caster_uses_same_surface_contract(self) -> None:
        self.assertIn("MakeShadowRasterizer(MaterialCullMode", self.shadow_pipeline)
        self.assertIn("MakeRasterizerCullFront", self.shadow_pipeline)
        self.assertIn("MakeRasterizerCullNone", self.shadow_pipeline)
        self.assertIn("SetShadowMapRenderSetting(effectiveCullMode)", self.object_source)
        self.assertIn("SetInstancedShadowMapRenderSetting(ResolveEffectiveCullMode())", self.instanced_shadow)

    def test_editor_picking_uses_material_cull_mode(self) -> None:
        self.assertIn("staticPipelines_", self.object_id_pipeline)
        self.assertIn("instancedPipelines_", self.object_id_pipeline)
        self.assertIn("MakeRasterizer(cullMode)", self.object_id_pipeline)
        self.assertIn("BindStatic(commandList, objectId, cullMode)", self.object_header)
        self.assertIn("BindInstanced(commandList, baseObjectId, true, ResolveEffectiveCullMode())", self.instanced_shadow)

    def test_material_json_round_trips_cull_mode_and_keeps_legacy_default(self) -> None:
        self.assertIn("MaterialCullMode cullMode = MaterialCullMode::Back", self.material_source_h)
        self.assertIn('static constexpr const char* CullMode = "cullMode"', self.material_json_h)
        self.assertIn('ReadStringOr(root, Keys::CullMode, "back")', self.material_json_cpp)
        self.assertIn("json[Keys::CullMode] = ToString(normalized.cullMode)", self.material_json_cpp)
        self.assertIn('text == "none"', self.material_json_cpp)
        self.assertIn("desc.cullMode = normalizedSource.cullMode", self.material_source_cpp)

    def test_skinned_pipeline_has_back_front_and_two_sided_pso_variants(self) -> None:
        self.assertIn("SetRenderSetting(MaterialCullMode", self.animation_pipeline_h)
        self.assertIn("graphicsPipelineStateFront_", self.animation_pipeline_h)
        self.assertIn("graphicsPipelineStateTwoSided_", self.animation_pipeline_h)
        self.assertIn("MakeAnimationRasterizer", self.animation_pipeline)
        self.assertIn("MakeRasterizerCullFront", self.animation_pipeline)
        self.assertIn("MakeRasterizerCullNone", self.animation_pipeline)
        self.assertIn("createPipeline(MaterialCullMode::Back", self.animation_pipeline)
        self.assertIn("createPipeline(MaterialCullMode::Front", self.animation_pipeline)
        self.assertIn("createPipeline(MaterialCullMode::None", self.animation_pipeline)

    def test_skinned_batches_group_models_by_effective_cull_mode(self) -> None:
        self.assertIn("const MaterialCullMode cullModes[]", self.animation_source)
        self.assertIn("ResolveMaterialCullModeForWorld(m->material_.GetCullMode(), cullWorld)", self.animation_source)
        self.assertIn("if (effectiveCullMode != cullMode) continue", self.animation_source)
        self.assertIn("SetRenderSetting(cullMode)", self.animation_source)
        self.assertGreaterEqual(self.animation_source.count("bool pipelineBound = false"), 2)

    def test_skinned_single_and_shadow_draws_resolve_mirrored_winding(self) -> None:
        self.assertIn("SetRenderSetting(effectiveCullMode)", self.animation_source)
        self.assertIn("ResolveMaterialCullModeForWorld(material_.GetCullMode(), shadowWorld)", self.animation_shadow)
        self.assertIn("SetShadowMapRenderSetting(effectiveCullMode)", self.animation_shadow)

    def test_assimp_preserves_two_sided_surface_metadata(self) -> None:
        self.assertIn("AI_MATKEY_TWOSIDED", self.assimp_loader)
        self.assertIn("SetCullMode(MaterialCullMode::None)", self.assimp_loader)
        self.assertIn("SetCullMode(MaterialCullMode::Back)", self.assimp_loader)
        self.assertIn("GetMaterialCullModes", self.model_header)
        self.assertIn("materialCullModes_.push_back(sub.material.GetCullMode())", self.model_source)

    def test_phase15_documents_forward_deferred_and_hzb_targets(self) -> None:
        self.assertIn("15.2 — Forward Renderer Completion", self.phase_doc)
        self.assertIn("15.3 — Deferred Renderer", self.phase_doc)
        self.assertIn("15.4 — GPU Visibility / HZB", self.phase_doc)
        self.assertIn("Deferred Opaque + Forward Transparent", self.phase_doc)


if __name__ == "__main__":
    unittest.main()
