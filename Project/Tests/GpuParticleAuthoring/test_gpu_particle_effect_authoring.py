from pathlib import Path
import json
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
GPU_ROOT = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle"
SHADER_ROOT = PROJECT_ROOT / "Resources" / "Shaders" / "GpuParticle"


class GpuParticleEffectAuthoringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.modules = (GPU_ROOT / "Runtime" / "GpuParticleEffectModules.h").read_text(encoding="utf-8")
        cls.runtime = (GPU_ROOT / "Runtime" / "GpuParticleEffectRuntime.h").read_text(encoding="utf-8")
        cls.effect_desc = (GPU_ROOT / "Data" / "GpuParticleEffectDesc.h").read_text(encoding="utf-8")
        cls.emitter_data = (GPU_ROOT / "Data" / "GpuParticleEmitterData.h").read_text(encoding="utf-8")
        cls.buffers = (GPU_ROOT / "Buffers" / "GpuParticleBuffers.h").read_text(encoding="utf-8")
        cls.serializer = (GPU_ROOT / "Preset" / "GpuParticleEffectSerializer.cpp").read_text(encoding="utf-8")
        cls.editor = (GPU_ROOT / "Preset" / "GpuParticleEffectEditor.cpp").read_text(encoding="utf-8")
        cls.renderer = (GPU_ROOT / "Renderer" / "GpuParticleRenderer.cpp").read_text(encoding="utf-8")
        cls.update_shader = (SHADER_ROOT / "GpuParticleUpdate.CS.hlsl").read_text(encoding="utf-8")
        cls.particle_data_shader = (SHADER_ROOT / "GpuParticleData.hlsli").read_text(encoding="utf-8")
        cls.explosion = json.loads((PROJECT_ROOT / "Resources" / "Effects" / "Explosion.effect.json").read_text(encoding="utf-8"))
        cls.mesh_debris = json.loads((PROJECT_ROOT / "Resources" / "Effects" / "MeshDebris.effect.json").read_text(encoding="utf-8"))
        cls.boss_charge = json.loads((PROJECT_ROOT / "Resources" / "Effects" / "BossCharge.effect.json").read_text(encoding="utf-8"))

    def test_effect_assets_compile_into_explicit_modules(self) -> None:
        for module_name in (
            "GpuParticleEmissionModule",
            "GpuParticleSpawnModule",
            "GpuParticleUpdateModule",
            "GpuParticleRenderModule",
        ):
            self.assertIn(f"struct {module_name}", self.modules)
        self.assertIn("class GpuParticleEffectCompiler", self.modules)
        self.assertIn("out.emission.spawnRate = desc.spawnRate", self.modules)
        self.assertIn("out.spawn.velocity = desc.velocity", self.modules)
        self.assertIn("out.update.noiseStrength = desc.noiseStrength", self.modules)
        self.assertIn("out.render.meshSubMeshIndex = desc.meshSubMeshIndex", self.modules)

    def test_runtime_uses_serializer_compiler_and_handle_scoped_playback(self) -> None:
        self.assertIn("GpuParticleEffectCompiler::Compile(effect)", self.runtime)
        self.assertIn("GpuParticleEffectSerializer::Load(effect, filePath)", self.runtime)
        self.assertIn("bool Play(const std::string& effectName", self.runtime)
        self.assertIn("PlayHandle PlayLoop", self.runtime)
        self.assertIn("bool StopLoop(PlayHandle handle)", self.runtime)
        self.assertIn("parameterOverrides", self.runtime)

    def test_authoring_supports_shapes_blends_curves_gradients_and_forces(self) -> None:
        for name in ("Point", "Sphere", "Box", "Cone", "Circle", "Ring", "Hemisphere"):
            self.assertIn(name, self.effect_desc)
        for name in ("Alpha", "Additive", "Multiply"):
            self.assertIn(name, self.effect_desc)
        for token in (
            "useSizeCurve",
            "sizeCurveLut",
            "useColorGradient",
            "colorGradientLut",
            "noiseStrength",
            "vortexStrength",
            "attractorStrength",
        ):
            self.assertIn(token, self.effect_desc)
            self.assertIn(token, self.serializer)
        self.assertIn("SampleScalarLut", self.update_shader)
        self.assertIn("SampleColorGradient", self.update_shader)
        self.assertIn("EvaluateNoiseAcceleration", self.update_shader)
        self.assertIn("EvaluateVortexAcceleration", self.update_shader)
        self.assertIn("EvaluateAttractorAcceleration", self.update_shader)

    def test_cpu_gpu_layout_contracts_remain_explicit(self) -> None:
        # CPU/GPU間の構造体サイズはShader契約を壊さないため明示的に固定する。
        self.assertIn("static_assert(sizeof(GpuEmitterCBData) == 624)", self.emitter_data)
        self.assertIn("static_assert(sizeof(ParticleCS) == 544)", self.buffers)
        self.assertIn("float4 sizeCurveLut", self.particle_data_shader)
        self.assertIn("float3 angularVelocity3D", self.particle_data_shader)
        self.assertIn("float3 previousTranslate", self.particle_data_shader)

    def test_current_sample_assets_have_functional_names_and_expected_content(self) -> None:
        self.assertEqual(self.explosion["effectName"], "Explosion")
        self.assertEqual({emitter["name"] for emitter in self.explosion["emitters"]}, {"Flash", "Smoke", "Sparks"})
        self.assertEqual(self.mesh_debris["effectName"], "MeshDebris")
        self.assertEqual(self.mesh_debris["emitters"][0]["renderType"], "Mesh")
        self.assertEqual(self.mesh_debris["emitters"][0]["meshPath"], "Sample/cube.gltf")
        self.assertEqual(self.boss_charge["effectName"], "BossCharge")
        self.assertTrue(all(emitter["loop"] for emitter in self.boss_charge["emitters"]))

    def test_effect_editor_uses_runtime_preview_instead_of_a_second_backend(self) -> None:
        self.assertIn("RegisterPreviewEffect", self.editor)
        self.assertIn("runtime->RegisterEffect(effect)", self.editor)
        self.assertIn("runtime->Play(effect.effectName, previewPosition)", self.editor)
        self.assertIn("runtime->PlayLoop(effect.effectName, previewPosition)", self.editor)
        self.assertNotIn("CreateCommittedResource", self.editor)

    def test_mesh_and_material_identity_reach_renderer(self) -> None:
        self.assertIn("LoadMeshAssetsFromAssimp", self.runtime)
        self.assertIn("meshSubMeshIndex", self.effect_desc)
        self.assertIn("BuildGpuParticleRenderGroup", self.emitter_data)
        self.assertIn("UnpackGpuParticleBlendMode", self.renderer)


if __name__ == "__main__":
    unittest.main()
