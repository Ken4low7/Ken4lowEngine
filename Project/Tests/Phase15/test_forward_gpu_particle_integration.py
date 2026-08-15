from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
EMITTER_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle" / "Emitter" / "GpuParticleEmitter.h"
BRIDGE_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle" / "Renderer" / "GpuParticleForwardRenderBridge.h"
RENDERER_CPP = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle" / "Renderer" / "GpuParticleRenderer.cpp"
SPRITE_PIPELINE_CPP = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle" / "Pipeline" / "GpuParticleSpritePipeline.cpp"
ACTOR_WORLD_DRAW = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Core" / "ActorWorld_Draw.cpp"
GAME_APPLICATION = PROJECT_ROOT / "Engine" / "Core" / "Application" / "GameApplication.cpp"


class ForwardGpuParticleIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.emitter_h = EMITTER_H.read_text(encoding="utf-8")
        cls.bridge_h = BRIDGE_H.read_text(encoding="utf-8")
        cls.renderer_cpp = RENDERER_CPP.read_text(encoding="utf-8")
        cls.sprite_pipeline_cpp = SPRITE_PIPELINE_CPP.read_text(encoding="utf-8")
        cls.actor_world_draw = ACTOR_WORLD_DRAW.read_text(encoding="utf-8")
        cls.game_application = GAME_APPLICATION.read_text(encoding="utf-8")

    def test_emitter_exposes_forward_blend_filter(self) -> None:
        self.assertIn("enum class GpuParticleForwardDrawPass", self.emitter_h)
        self.assertIn("Transparent", self.emitter_h)
        self.assertIn("Additive", self.emitter_h)
        self.assertIn("UnpackGpuParticleBlendMode(GetDrawType())", self.emitter_h)
        self.assertIn("blendMode != BlendMode::kBlendModeAdd", self.emitter_h)
        self.assertIn("blendMode == BlendMode::kBlendModeAdd", self.emitter_h)
        self.assertIn("thread_local GpuParticleForwardDrawPass", self.emitter_h)

    def test_bridge_submits_system_packets_not_individual_particles(self) -> None:
        self.assertIn("class GpuParticleForwardRenderBridge", self.bridge_h)
        self.assertIn("RenderPacket transparentPacket_", self.bridge_h)
        self.assertIn("RenderPacket additivePacket_", self.bridge_h)
        self.assertIn("MakeForwardRenderItem", self.bridge_h)
        self.assertIn("MaterialBlendMode::Transparent", self.bridge_h)
        self.assertIn("MaterialBlendMode::Additive", self.bridge_h)
        self.assertNotIn("GpuParticleBuffers::GetMaxParticles", self.bridge_h)
        self.assertNotIn("for (const Particle", self.bridge_h)

    def test_bridge_reuses_existing_gpu_driven_draw_path(self) -> None:
        self.assertIn("renderPacket->manager->Draw()", self.bridge_h)
        self.assertIn("ScopedDrawPass", self.bridge_h)
        self.assertIn("SetForwardDrawPass(pass)", self.bridge_h)
        self.assertIn("SetForwardDrawPass(previous_)", self.bridge_h)
        self.assertIn("ExecuteIndirect", self.renderer_cpp)
        self.assertIn("BuildVisibleParticleList", self.renderer_cpp)

    def test_alpha_particle_depth_sort_remains_on_gpu(self) -> None:
        self.assertIn("blendMode_ == BlendMode::kBlendModeNormal", self.renderer_cpp)
        self.assertIn("SortVisibleParticlesByDepth()", self.renderer_cpp)
        self.assertIn("depthSortPipelineState_", self.renderer_cpp)
        self.assertIn("sortDepth", self.bridge_h)
        self.assertIn("0.0f", self.bridge_h)

    def test_particle_graphics_pipeline_keeps_depth_read_only(self) -> None:
        self.assertIn("D3D12_DEPTH_WRITE_MASK_ZERO", self.sprite_pipeline_cpp)
        self.assertIn("D3D12_COMPARISON_FUNC_LESS_EQUAL", self.sprite_pipeline_cpp)
        self.assertIn("GetGfxPSO(blendMode_)", self.renderer_cpp)

    def test_actor_world_collects_gpu_particle_packets_before_execution(self) -> None:
        self.assertIn("GpuParticleForwardRenderBridge.h", self.actor_world_draw)
        submit = self.actor_world_draw.index("GpuParticleForwardRenderBridge::GetInstance()->Submit(*forwardQueue)")
        opaque_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Opaque)")
        transparent_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Transparent)")
        additive_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Additive)")
        self.assertLess(submit, opaque_execute)
        self.assertLess(transparent_execute, additive_execute)

    def test_game_application_no_longer_draws_gpu_particles_outside_forward_queue(self) -> None:
        self.assertNotIn("GpuParticleManager::GetInstance()->Draw();", self.game_application)
        self.assertNotIn("#include <GpuParticleManager.h>", self.game_application)
        self.assertIn("GPU ParticleはActorWorld内のForward Queueへ統合済み", self.game_application)


if __name__ == "__main__":
    unittest.main()
