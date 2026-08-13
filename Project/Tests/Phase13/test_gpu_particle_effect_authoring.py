from pathlib import Path
import json
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MODULE_HEADER = (
    PROJECT_ROOT
    / "Engine"
    / "Graphics"
    / "Renderer"
    / "GpuParticle"
    / "Runtime"
    / "GpuParticleEffectModules.h"
)
RUNTIME_HEADER = MODULE_HEADER.with_name("GpuParticleEffectRuntime.h")
EFFECT_DESC_HEADER = (
    PROJECT_ROOT
    / "Engine"
    / "Graphics"
    / "Renderer"
    / "GpuParticle"
    / "Data"
    / "GpuParticleEffectDesc.h"
)
SERIALIZER_SOURCE = (
    PROJECT_ROOT
    / "Engine"
    / "Graphics"
    / "Renderer"
    / "GpuParticle"
    / "Preset"
    / "GpuParticleEffectSerializer.cpp"
)
EDITOR_SOURCE = SERIALIZER_SOURCE.with_name("GpuParticleEffectEditor.cpp")
SAMPLE_EFFECT = PROJECT_ROOT / "Resources" / "Effects" / "Phase13" / "Explosion.effect.json"
EMIT_SHADER = PROJECT_ROOT / "Resources" / "Shaders" / "GpuParticle" / "GpuParticleEmit.CS.hlsl"


class GpuParticleEffectAuthoringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.modules = MODULE_HEADER.read_text(encoding="utf-8")
        cls.runtime = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.effect_desc = EFFECT_DESC_HEADER.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER_SOURCE.read_text(encoding="utf-8")
        cls.editor = EDITOR_SOURCE.read_text(encoding="utf-8")
        cls.emit_shader = EMIT_SHADER.read_text(encoding="utf-8")
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
        self.assertIn("CompileEmitter(const GpuParticleEmitterDesc& desc)", self.modules)
        self.assertIn("out.emission.spawnRate = desc.spawnRate", self.modules)
        self.assertIn("out.spawn.velocity = desc.velocity", self.modules)
        self.assertIn("out.update.gravity = desc.gravity", self.modules)
        self.assertIn("out.render.texturePath = desc.texturePath", self.modules)

    def test_runtime_uses_compiler_and_existing_effect_serializer(self) -> None:
        # Authoring JSON remains backward compatible while runtime depends on the compiled module representation.
        self.assertIn("GpuParticleEffectCompiler::Compile(effect)", self.runtime)
        self.assertIn("GpuParticleEffectSerializer::Load(effect, filePath)", self.runtime)
        self.assertIn("std::unordered_map<std::string, GpuParticleCompiledEffect>", self.runtime)
        self.assertIn('root["effectName"]', self.serializer)
        self.assertIn('root["emitters"]', self.serializer)

    def test_gameplay_runtime_exposes_effect_level_playback_api(self) -> None:
        for api in (
            "bool LoadEffect(const std::string& filePath)",
            "bool ReloadEffect(const std::string& effectName)",
            "bool Play(const std::string& effectName, const Vector3& worldPosition)",
            "PlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition)",
            "bool StopLoop(PlayHandle handle)",
            "bool SetLoopPosition(PlayHandle handle, const Vector3& worldPosition)",
        ):
            self.assertIn(api, self.runtime)

    def test_burst_playback_is_bounded_and_loop_instances_are_handle_scoped(self) -> None:
        self.assertIn("kBurstEmitterPoolSize = 8", self.runtime)
        self.assertIn("nextSlot++ % kBurstEmitterPoolSize", self.runtime)
        self.assertIn('"RuntimeAssetBurst_"', self.runtime)
        self.assertIn('"RuntimeAssetLoop_"', self.runtime)
        self.assertIn("activeLoops_[instance.handle.id] = instance", self.runtime)

    def test_effect_editor_previews_unsaved_module_configuration_through_runtime(self) -> None:
        self.assertIn('#include "GpuParticleEffectRuntime.h"', self.editor)
        for section in ("Emission Module", "Spawn Module", "Update Module", "Render Module"):
            self.assertIn(section, self.editor)
        self.assertIn('ImGui::Button("Preview Burst")', self.editor)
        self.assertIn('ImGui::Button("Start Loop Preview")', self.editor)
        self.assertIn("runtime->RegisterEffect(effect)", self.editor)
        self.assertIn("runtime->Play(effect.effectName, previewPosition)", self.editor)
        self.assertIn("runtime->PlayLoop(effect.effectName, previewPosition)", self.editor)

    def test_runtime_fails_closed_for_backend_features_not_implemented_yet(self) -> None:
        self.assertIn("GpuParticleRenderType::Sprite", self.runtime)
        self.assertIn("GpuParticleBlendMode::Alpha", self.runtime)
        for shape in ("Point", "Sphere", "Box"):
            self.assertIn(f"GpuParticleSpawnShape::{shape}", self.runtime)

        # The current custom spawn shader only has dedicated Sphere and Box branches after the Point/default path.
        self.assertIn("spawnShape == 1u", self.emit_shader)
        self.assertIn("spawnShape == 2u", self.emit_shader)
        self.assertNotIn("spawnShape == 3u", self.emit_shader)

    def test_sample_effect_exercises_multi_emitter_authoring(self) -> None:
        self.assertEqual(self.sample["effectName"], "Phase13Explosion")
        self.assertEqual(len(self.sample["emitters"]), 3)
        self.assertEqual(
            {emitter["name"] for emitter in self.sample["emitters"]},
            {"Flash", "Smoke", "Sparks"},
        )
        for emitter in self.sample["emitters"]:
            self.assertEqual(emitter["renderType"], "Sprite")
            self.assertEqual(emitter["blendMode"], "Alpha")
            self.assertIn(emitter["spawnShape"], {"Point", "Sphere", "Box"})
            self.assertGreater(emitter["maxParticles"], 0)
            self.assertGreater(emitter["burstCount"], 0)

    def test_authoring_desc_retains_future_shapes_and_blend_modes(self) -> None:
        # Unsupported values stay in the asset schema so later GPU backend work does not require a save-format break.
        for name in ("Cone", "Circle", "Ring", "Hemisphere"):
            self.assertIn(name, self.effect_desc)
        for name in ("Alpha", "Additive", "Multiply"):
            self.assertIn(name, self.effect_desc)


if __name__ == "__main__":
    unittest.main()
