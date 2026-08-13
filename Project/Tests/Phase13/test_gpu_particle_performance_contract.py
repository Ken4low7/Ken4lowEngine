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
        cls.compact_cs = (SHADER_ROOT / "GpuParticleCompact.CS.hlsl").read_text(encoding="utf-8")
        cls.renderer = (GPU_ROOT / "Renderer" / "GpuParticleRenderer.cpp").read_text(encoding="utf-8")
        cls.charge_effect = json.loads((EFFECT_ROOT / "BossCharge.effect.json").read_text(encoding="utf-8"))

    def test_vertex_stage_consumes_only_compacted_particle_instances(self) -> None:
        # Phase14 supersedes the Phase13 VS rejection path by removing dead/foreign particles before graphics dispatch.
        self.assertIn("particle.lifeTime <= 0.0f", self.compact_cs)
        self.assertIn("particle.type != gDraw.renderGroup", self.compact_cs)
        for shader in (self.sprite_vs, self.mesh_vs):
            self.assertIn("gVisibleParticleIndices[instanceId]", shader)
            self.assertIn("gParticles[particleIndex]", shader)
        self.assertIn("ExecuteIndirect", self.renderer)

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
