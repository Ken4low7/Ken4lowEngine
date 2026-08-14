from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PIPELINE_PRESETS_H = PROJECT_ROOT / "Engine" / "Graphics" / "Pipeline" / "PipelineStatePresets.h"
PIPELINE_PRESETS_CPP = PIPELINE_PRESETS_H.with_suffix(".cpp")
OBJECT_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DPipelineSet.cpp"
INSTANCED_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "InstancedObject3DPipelineSet.cpp"
ANIMATION_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Pipeline" / "AnimationPipelineBuilder.cpp"
OBJECT_ID_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "ObjectIdPipeline.h"
PHASE_DOC = PROJECT_ROOT / "Docs" / "Phase15RenderingCompletion.md"


class RenderingCompletionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.presets_h = PIPELINE_PRESETS_H.read_text(encoding="utf-8")
        cls.presets_cpp = PIPELINE_PRESETS_CPP.read_text(encoding="utf-8")
        cls.object_pipeline = OBJECT_PIPELINE.read_text(encoding="utf-8")
        cls.instanced_pipeline = INSTANCED_PIPELINE.read_text(encoding="utf-8")
        cls.animation_pipeline = ANIMATION_PIPELINE.read_text(encoding="utf-8")
        cls.object_id_pipeline = OBJECT_ID_PIPELINE.read_text(encoding="utf-8")
        cls.phase_doc = PHASE_DOC.read_text(encoding="utf-8")

    def test_rasterizer_presets_define_all_cull_modes(self) -> None:
        self.assertIn("MakeRasterizerCullBack", self.presets_h)
        self.assertIn("MakeRasterizerCullFront", self.presets_h)
        self.assertIn("MakeRasterizerCullNone", self.presets_h)
        self.assertIn("D3D12_CULL_MODE_BACK", self.presets_cpp)
        self.assertIn("D3D12_CULL_MODE_FRONT", self.presets_cpp)
        self.assertIn("D3D12_CULL_MODE_NONE", self.presets_cpp)

    def test_winding_contract_is_explicit(self) -> None:
        # Rasterizerごとに暗黙の既定値へ依存せず、CWをfront faceとして固定する。
        self.assertGreaterEqual(self.presets_cpp.count("FrontCounterClockwise = FALSE"), 3)
        self.assertIn("時計回り", self.presets_cpp)

    def test_static_object_pipeline_culls_back_faces(self) -> None:
        base_start = self.object_pipeline.index("GraphicsPipelineDesc MakeBaseObject3DDesc")
        base_end = self.object_pipeline.index("GraphicsPipelineDesc MakeBaseShadowDesc")
        base_block = self.object_pipeline[base_start:base_end]
        self.assertIn("MakeRasterizerCullBack", base_block)
        self.assertNotIn("MakeRasterizerCullNone", base_block)

    def test_instanced_object_pipeline_culls_back_faces(self) -> None:
        pipeline_start = self.instanced_pipeline.index('desc.debugName = L"Object3D.Instanced"')
        setup_start = self.instanced_pipeline.rfind("GraphicsPipelineDesc desc{};", 0, pipeline_start)
        setup_block = self.instanced_pipeline[setup_start:pipeline_start]
        self.assertIn("MakeRasterizerCullBack", setup_block)
        self.assertNotIn("MakeRasterizerCullNone", setup_block)

    def test_skinned_pipeline_culls_back_faces(self) -> None:
        create_pso = self.animation_pipeline[self.animation_pipeline.index("void AnimationPipelineBuilder::CreatePSO()") :]
        self.assertIn("rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK", create_pso)
        self.assertIn("rasterizerDesc.DepthClipEnable = TRUE", create_pso)

    def test_editor_picking_matches_visible_back_face_rule(self) -> None:
        self.assertIn("MakeRasterizerCullBack", self.object_id_pipeline)

    def test_phase15_documents_forward_deferred_and_hzb_targets(self) -> None:
        self.assertIn("15.2 — Forward Renderer Completion", self.phase_doc)
        self.assertIn("15.3 — Deferred Renderer", self.phase_doc)
        self.assertIn("15.4 — GPU Visibility / HZB", self.phase_doc)
        self.assertIn("Deferred Opaque + Forward Transparent", self.phase_doc)


if __name__ == "__main__":
    unittest.main()
