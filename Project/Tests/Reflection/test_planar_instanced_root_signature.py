from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
INSTANCED_PIPELINE = (
    PROJECT_ROOT
    / "Engine"
    / "Graphics"
    / "Renderer"
    / "Object3D"
    / "InstancedObject3DPipelineSet.cpp"
)
OBJECT_PS = PROJECT_ROOT / "Resources" / "Shaders" / "Object3D" / "Object3d.PS.hlsl"


class InstancedPlanarRootSignatureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.pipeline = INSTANCED_PIPELINE.read_text(encoding="utf-8")
        cls.shader = OBJECT_PS.read_text(encoding="utf-8")

    def test_instanced_pipeline_uses_shared_object3d_pixel_shader(self) -> None:
        self.assertIn("Object3DShaderId::Object3DPS", self.pipeline)
        self.assertIn("gPlanarReflection : register(b7)", self.shader)
        self.assertIn("gPlanarReflectionTextures[kMaxPlanarReflectionSurfaces] : register(t12)", self.shader)

    def test_instanced_root_signature_exposes_planar_cbv_b7(self) -> None:
        self.assertIn("kPlanarReflectionCBV", self.pipeline)
        self.assertIn(
            "setCBV(kPlanarReflectionCBV, 7, D3D12_SHADER_VISIBILITY_PIXEL)",
            self.pipeline,
        )

    def test_instanced_root_signature_exposes_six_planar_srvs_from_t12(self) -> None:
        self.assertIn("std::array<D3D12_DESCRIPTOR_RANGE, 13>", self.pipeline)
        self.assertIn("ranges[12].NumDescriptors = 6", self.pipeline)
        self.assertIn("ranges[12].BaseShaderRegister = 12", self.pipeline)
        self.assertIn(
            "setTable(kPlanarReflectionTexturesSRV, 12, D3D12_SHADER_VISIBILITY_PIXEL)",
            self.pipeline,
        )


if __name__ == "__main__":
    unittest.main()
