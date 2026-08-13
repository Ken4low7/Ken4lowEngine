from pathlib import Path
import json
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
GPU_ROOT = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle"
SHADER_ROOT = PROJECT_ROOT / "Resources" / "Shaders" / "GpuParticle"
EFFECT_ROOT = PROJECT_ROOT / "Resources" / "Effects" / "Phase13"


class GpuParticlePerformanceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.sprite_pipeline = (GPU_ROOT / "Pipeline" / "GpuParticleSpritePipeline.cpp").read_text(encoding="utf-8")
        cls.mesh_pipeline = (GPU_ROOT / "Pipeline" / "GpuParticleMeshPipeline.cpp").read_text(encoding="utf-8")
        cls.sprite_vs = (SHADER_ROOT / "GpuParticle.VS.hlsl").read_text(encoding="utf-8")
        cls.mesh_vs = (SHADER_ROOT / "GpuParticleMesh.VS.hlsl").read_text(encoding="utf-8")
        cls.renderer = (GPU_ROOT / "Renderer" / "GpuParticleRenderer.cpp").read_text(encoding="utf-8")
        cls.charge_effect = json.loads((EFFECT_ROOT / "BossCharge.effect.json").read_text(encoding="utf-8"))

    def test_vertex_stage_rejects_dead_and_foreign_material_instances(self) -> None:
        # The current global particle buffer draw still scans all slots, so rejection must happen before rasterization.
        for shader in (self.sprite_vs, self.mesh_vs):
            self.assertIn("ConstantBuffer<Material> gMaterial : register(b1)", shader)
            self.assertIn("particle.lifeTime <= 0.0f", shader)
            self.assertIn("particle.type != gMaterial.drawType", shader)
            self.assertIn("output.position = float4(0.0f, 0.0f, -1.0f, 1.0f)", shader)

    def test_material_cb_is_visible_to_vertex_and_pixel_stages(self) -> None:
        self.assertIn("params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL", self.sprite_pipeline)
        self.assertIn("params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL", self.mesh_pipeline)

    def test_renderer_preserves_legacy_and_authored_group_paths(self) -> None:
        self.assertIn("hasAuthoredBlendTag", self.renderer)
        self.assertIn("BuildGpuParticleRenderGroup", self.renderer)
        self.assertIn(": materialDrawType", self.renderer)

    def test_boss_charge_exercises_live_parameter_driven_looping(self) -> None:
        self.assertEqual(self.charge_effect["effectName"], "BossCharge")
        self.assertEqual({p["name"] for p in self.charge_effect["userParameters"]}, {"Charge"})
        self.assertEqual(len(self.charge_effect["emitters"]), 3)
        self.assertTrue(all(emitter["loop"] for emitter in self.charge_effect["emitters"]))
        targets = {
            binding["target"]
            for emitter in self.charge_effect["emitters"]
            for binding in emitter["parameterBindings"]
        }
        self.assertTrue({"SpawnRate", "Size", "Alpha", "Force"}.issubset(targets))
        self.assertTrue(any(emitter["attractorStrength"] > 0.0 for emitter in self.charge_effect["emitters"]))
        self.assertTrue(any(emitter["vortexStrength"] > 0.0 for emitter in self.charge_effect["emitters"]))


if __name__ == "__main__":
    unittest.main()
