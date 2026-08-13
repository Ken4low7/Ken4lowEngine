from pathlib import Path
import json
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
GPU_ROOT = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle"
MODULE_HEADER = GPU_ROOT / "Runtime" / "GpuParticleEffectModules.h"
RUNTIME_HEADER = GPU_ROOT / "Runtime" / "GpuParticleEffectRuntime.h"
EFFECT_DESC_HEADER = GPU_ROOT / "Data" / "GpuParticleEffectDesc.h"
EMITTER_DATA_HEADER = GPU_ROOT / "Data" / "GpuParticleEmitterData.h"
BUFFERS_HEADER = GPU_ROOT / "Buffers" / "GpuParticleBuffers.h"
SERIALIZER_SOURCE = GPU_ROOT / "Preset" / "GpuParticleEffectSerializer.cpp"
EDITOR_SOURCE = GPU_ROOT / "Preset" / "GpuParticleEffectEditor.cpp"
SPRITE_PIPELINE_HEADER = GPU_ROOT / "Pipeline" / "GpuParticleSpritePipeline.h"
MESH_PIPELINE_HEADER = GPU_ROOT / "Pipeline" / "GpuParticleMeshPipeline.h"
RENDERER_SOURCE = GPU_ROOT / "Renderer" / "GpuParticleRenderer.cpp"
SAMPLE_EFFECT = PROJECT_ROOT / "Resources" / "Effects" / "Phase13" / "Explosion.effect.json"
EMIT_SHADER = PROJECT_ROOT / "Resources" / "Shaders" / "GpuParticle" / "GpuParticleEmit.CS.hlsl"
UPDATE_SHADER = PROJECT_ROOT / "Resources" / "Shaders" / "GpuParticle" / "GpuParticleUpdate.CS.hlsl"
MESH_VERTEX_SHADER = PROJECT_ROOT / "Resources" / "Shaders" / "GpuParticle" / "GpuParticleMesh.VS.hlsl"
PARTICLE_DATA_SHADER = PROJECT_ROOT / "Resources" / "Shaders" / "GpuParticle" / "GpuParticleData.hlsli"


class GpuParticleEffectAuthoringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.modules = MODULE_HEADER.read_text(encoding="utf-8")
        cls.runtime = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.effect_desc = EFFECT_DESC_HEADER.read_text(encoding="utf-8")
        cls.emitter_data = EMITTER_DATA_HEADER.read_text(encoding="utf-8")
        cls.buffers = BUFFERS_HEADER.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER_SOURCE.read_text(encoding="utf-8")
        cls.editor = EDITOR_SOURCE.read_text(encoding="utf-8")
        cls.sprite_pipeline = SPRITE_PIPELINE_HEADER.read_text(encoding="utf-8")
        cls.mesh_pipeline = MESH_PIPELINE_HEADER.read_text(encoding="utf-8")
        cls.renderer = RENDERER_SOURCE.read_text(encoding="utf-8")
        cls.emit_shader = EMIT_SHADER.read_text(encoding="utf-8")
        cls.update_shader = UPDATE_SHADER.read_text(encoding="utf-8")
        cls.mesh_vertex_shader = MESH_VERTEX_SHADER.read_text(encoding="utf-8")
        cls.particle_data_shader = PARTICLE_DATA_SHADER.read_text(encoding="utf-8")
        cls.sample = json.loads(SAMPLE_EFFECT.read_text(encoding="utf-8"))

    def test_flat_asset_is_compiled_into_explicit_module_boundaries(self) -> None:
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
        self.assertIn("out.update.sizeCurveLut = desc.sizeCurveLut", self.modules)
        self.assertIn("out.render.meshSubMeshIndex = desc.meshSubMeshIndex", self.modules)
        self.assertIn("out.userParameters = effect.userParameters", self.modules)

    def test_runtime_uses_compiler_serializer_and_effect_level_playback(self) -> None:
        self.assertIn("GpuParticleEffectCompiler::Compile(effect)", self.runtime)
        self.assertIn("GpuParticleEffectSerializer::Load(effect, filePath)", self.runtime)
        self.assertIn("std::unordered_map<std::string, GpuParticleCompiledEffect>", self.runtime)
        for api in (
            "bool LoadEffect(const std::string& filePath)",
            "bool ReloadEffect(const std::string& effectName)",
            "bool Play(const std::string& effectName, const Vector3& worldPosition)",
            "PlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition)",
            "bool StopLoop(PlayHandle handle)",
            "bool SetLoopPosition(PlayHandle handle, const Vector3& worldPosition)",
        ):
            self.assertIn(api, self.runtime)
        self.assertIn('root["effectName"]', self.serializer)
        self.assertIn('root["emitters"]', self.serializer)

    def test_burst_pool_and_loop_instances_remain_bounded_and_handle_scoped(self) -> None:
        self.assertIn("kBurstEmitterPoolSize = 8", self.runtime)
        self.assertIn("nextSlot++ % kBurstEmitterPoolSize", self.runtime)
        self.assertIn('"RuntimeAssetBurst_"', self.runtime)
        self.assertIn('"RuntimeAssetLoop_"', self.runtime)
        self.assertIn("std::unordered_map<uint32_t, LoopInstance> activeLoops_", self.runtime)
        self.assertIn("parameterOverrides", self.runtime)

    def test_all_authored_spawn_shapes_reach_custom_emit_shader(self) -> None:
        for name in ("Point", "Sphere", "Box", "Cone", "Circle", "Ring", "Hemisphere"):
            self.assertIn(name, self.effect_desc)
        for branch in (
            "spawnShape == 1u",
            "spawnShape == 2u",
            "spawnShape == 3u",
            "spawnShape == 4u",
            "spawnShape == 5u",
            "spawnShape == 6u",
        ):
            self.assertIn(branch, self.emit_shader)
        self.assertIn("SampleAuthoredShape", self.emit_shader)

    def test_blend_modes_are_real_pipeline_state_choices(self) -> None:
        for name in ("Alpha", "Additive", "Multiply"):
            self.assertIn(name, self.effect_desc)
        self.assertIn("GetGfxPSO(BlendMode blendMode) const", self.sprite_pipeline)
        self.assertIn("GetGfxPSO(BlendMode blendMode) const", self.mesh_pipeline)
        self.assertIn("PackGpuParticleDrawType", self.emitter_data)
        self.assertIn("UnpackGpuParticleBlendMode", self.renderer)
        self.assertIn("GetGfxPSO(blendMode_)", self.renderer)
        self.assertIn("BlendMode::kBlendModeNormal", self.runtime)
        self.assertIn("BlendMode::kBlendModeMultiply", self.runtime)

    def test_mesh_authoring_loads_real_mesh_assets_and_rotates_instances(self) -> None:
        self.assertIn("GpuParticleKind::Mesh", self.runtime)
        self.assertIn("LoadMeshAssetsFromAssimp", self.runtime)
        self.assertIn('"Mesh:" + std::to_string(meshId)', self.runtime)
        self.assertIn("meshSubMeshIndex", self.effect_desc)
        self.assertIn("startRotation3D", self.effect_desc)
        self.assertIn("angularVelocity", self.effect_desc)
        self.assertIn("RotateEulerXYZ", self.mesh_vertex_shader)
        self.assertIn("particle.rotation3D", self.mesh_vertex_shader)

    def test_curves_gradients_and_force_modules_execute_on_gpu(self) -> None:
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

        self.assertIn("GPU_PARTICLE_CUSTOM_SIZE_CURVE", self.particle_data_shader)
        self.assertIn("GPU_PARTICLE_CUSTOM_COLOR_GRADIENT", self.particle_data_shader)
        self.assertIn("SampleScalarLut", self.update_shader)
        self.assertIn("SampleColorGradient", self.update_shader)
        self.assertIn("EvaluateNoiseAcceleration", self.update_shader)
        self.assertIn("EvaluateVortexAcceleration", self.update_shader)
        self.assertIn("EvaluateAttractorAcceleration", self.update_shader)

    def test_user_parameters_are_serialized_bound_and_runtime_mutable(self) -> None:
        self.assertIn("GpuParticleUserParameterDesc", self.effect_desc)
        self.assertIn("GpuParticleParameterBindingDesc", self.effect_desc)
        self.assertIn('root["userParameters"]', self.serializer)
        self.assertIn('emitter["parameterBindings"]', self.serializer)
        self.assertIn(
            "bool SetFloatParameter(const std::string& effectName, const std::string& parameterName, float value)",
            self.runtime,
        )
        self.assertIn(
            "bool SetFloatParameter(PlayHandle handle, const std::string& parameterName, float value)",
            self.runtime,
        )
        self.assertIn("EvaluateTargetFactor", self.runtime)

    def test_cpu_gpu_struct_strides_are_explicit(self) -> None:
        # StructuredBuffer/CB stride changes must be intentional because a silent mismatch corrupts every particle.
        self.assertIn("static_assert(sizeof(GpuEmitterCBData) == 480)", self.emitter_data)
        self.assertIn("static_assert(sizeof(ParticleCS) == 384)", self.buffers)
        self.assertIn("float4 sizeCurveLut", self.particle_data_shader)
        self.assertIn("float3 angularVelocity3D", self.particle_data_shader)

    def test_sample_effect_still_exercises_multi_emitter_authoring(self) -> None:
        self.assertEqual(self.sample["effectName"], "Phase13Explosion")
        self.assertEqual(len(self.sample["emitters"]), 3)
        self.assertEqual(
            {emitter["name"] for emitter in self.sample["emitters"]},
            {"Flash", "Smoke", "Sparks"},
        )
        for emitter in self.sample["emitters"]:
            self.assertEqual(emitter["renderType"], "Sprite")
            self.assertIn(emitter["blendMode"], {"Alpha", "Additive", "Multiply"})
            self.assertIn(emitter["spawnShape"], {"Point", "Sphere", "Box", "Cone", "Circle", "Ring", "Hemisphere"})
            self.assertGreater(emitter["maxParticles"], 0)
            self.assertGreater(emitter["burstCount"], 0)

    def test_effect_editor_previews_unsaved_module_configuration_through_runtime(self) -> None:
        self.assertIn("RegisterPreviewEffect", self.editor)
        self.assertIn("runtime->RegisterEffect(effect)", self.editor)
        self.assertIn("runtime->Play(effect.effectName, previewPosition)", self.editor)
        self.assertIn("runtime->PlayLoop(effect.effectName, previewPosition)", self.editor)
        for section in ("Emission Module", "Spawn Module", "Update Module", "Render Module"):
            self.assertIn(section, self.editor)


if __name__ == "__main__":
    unittest.main()
