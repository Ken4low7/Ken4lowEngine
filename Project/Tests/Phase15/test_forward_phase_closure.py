from pathlib import Path
import json
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
FORWARD_QUEUE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Forward" / "ForwardRenderQueue.h"
GPU_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle" / "Renderer" / "GpuParticleForwardRenderBridge.h"
GPU_EMITTER_DATA = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle" / "Data" / "GpuParticleEmitterData.h"
ACTOR_WORLD_DRAW = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Core" / "ActorWorld_Draw.cpp"
GAME_APPLICATION = PROJECT_ROOT / "Engine" / "Core" / "Application" / "GameApplication.cpp"
MODEL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.h"
INSTANCED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "InstancedModelComponent.h"
ANIMATED_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponent.h"
SKELETAL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponent.h"
VALIDATION_MATRIX = Path(__file__).with_name("forward_render_validation_matrix.json")
PHASE_DOC = PROJECT_ROOT / "Docs" / "Phase15RenderingCompletion.md"


class ForwardPhaseClosureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.forward_queue = FORWARD_QUEUE.read_text(encoding="utf-8")
        cls.gpu_bridge = GPU_BRIDGE.read_text(encoding="utf-8")
        cls.gpu_emitter_data = GPU_EMITTER_DATA.read_text(encoding="utf-8")
        cls.actor_world_draw = ACTOR_WORLD_DRAW.read_text(encoding="utf-8")
        cls.game_application = GAME_APPLICATION.read_text(encoding="utf-8")
        cls.components = {
            "StaticModel": MODEL_COMPONENT.read_text(encoding="utf-8"),
            "InstancedModel": INSTANCED_COMPONENT.read_text(encoding="utf-8"),
            "AnimatedModel": ANIMATED_COMPONENT.read_text(encoding="utf-8"),
            "SkeletalMesh": SKELETAL_COMPONENT.read_text(encoding="utf-8"),
        }
        cls.matrix = json.loads(VALIDATION_MATRIX.read_text(encoding="utf-8"))
        cls.phase_doc = PHASE_DOC.read_text(encoding="utf-8")

    def test_validation_matrix_declares_canonical_bucket_order(self) -> None:
        self.assertEqual(
            self.matrix["expectedBucketExecutionOrder"],
            ["Opaque", "Masked", "Transparent", "Additive"],
        )
        self.assertEqual(self.matrix["legacyActorDrawPlacement"]["after"], "Masked")
        self.assertEqual(self.matrix["legacyActorDrawPlacement"]["before"], "Transparent")

    def test_all_surface_components_support_the_four_forward_buckets(self) -> None:
        expected = ["Opaque", "Masked", "Transparent", "Additive"]
        for path_name, component in self.components.items():
            self.assertEqual(self.matrix["renderPaths"][path_name], expected)
            for bucket in expected:
                self.assertIn(f"SubmitForward{bucket}", component)

    def test_actor_world_keeps_the_phase15_execution_order(self) -> None:
        opaque = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Opaque)")
        masked = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Masked)")
        legacy = self.actor_world_draw.index("actor->Draw()")
        transparent = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Transparent)")
        additive = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Additive)")
        self.assertLess(opaque, masked)
        self.assertLess(masked, legacy)
        self.assertLess(legacy, transparent)
        self.assertLess(transparent, additive)
        self.assertIn("GpuParticleForwardRenderBridge::GetInstance()->Submit(*forwardQueue)", self.actor_world_draw)

    def test_forward_queue_keeps_post_frame_diagnostics(self) -> None:
        for token in (
            "struct ForwardRenderFrameStats",
            "submittedByBucket",
            "executedByBucket",
            "executionOrder",
            "rejectedSubmissions",
            "duplicateExecutionRequests",
            "GetLastFrameStats()",
            "CaptureLastFrameStats()",
            "IsCanonicalForwardExecutionOrder",
        ):
            self.assertIn(token, self.forward_queue)
        self.assertIn("CaptureLastFrameStats();", self.forward_queue)
        self.assertIn("lastFrameStats_.canonicalExecutionOrder", self.forward_queue)

    def test_duplicate_bucket_execution_is_diagnostic_and_non_destructive(self) -> None:
        self.assertIn("if (bucketExecuted_[bucketIndex])", self.forward_queue)
        self.assertIn("++duplicateExecutionRequestCount_", self.forward_queue)
        duplicate_guard = self.forward_queue.index("if (bucketExecuted_[bucketIndex])")
        draw_loop = self.forward_queue.index("for (const ForwardRenderItem& item : items)")
        self.assertLess(duplicate_guard, draw_loop)

    def test_gpu_particles_remain_system_packets_with_separate_diagnostics(self) -> None:
        self.assertEqual(self.matrix["renderPaths"]["GpuParticle"], ["Transparent", "Additive"])
        self.assertEqual(self.matrix["gpuParticleContract"]["queueGranularity"], "SystemPacket")
        self.assertFalse(self.matrix["gpuParticleContract"]["cpuPerParticleSubmission"])
        self.assertTrue(self.matrix["gpuParticleContract"]["indirectDrawPreserved"])
        self.assertIn("struct GpuParticleForwardPacketStats", self.gpu_bridge)
        self.assertIn("transparentPackets", self.gpu_bridge)
        self.assertIn("additivePackets", self.gpu_bridge)
        self.assertIn("GetLastPacketStats()", self.gpu_bridge)
        self.assertIn("renderPacket->manager->Draw()", self.gpu_bridge)

    def test_legacy_particle_blend_fallback_is_explicit(self) -> None:
        # Blend tagを持たない旧Emitterは従来互換でAdditiveへ落とし、新Effect Runtimeはpacked tagを使う。
        self.assertIn("if (blendTag == 0u)", self.gpu_emitter_data)
        self.assertIn("return BlendMode::kBlendModeAdd", self.gpu_emitter_data)
        self.assertIn("PackGpuParticleDrawType", self.gpu_emitter_data)

    def test_gpu_particles_are_not_redrawn_by_game_application(self) -> None:
        self.assertNotIn("GpuParticleManager::GetInstance()->Draw();", self.game_application)
        self.assertIn("GPU ParticleはActorWorld内のForward Queueへ統合済み", self.game_application)

    def test_manual_visual_recipe_covers_mixed_render_paths(self) -> None:
        recipe = self.matrix["manualVisualRecipe"]
        recipe_paths = {entry["path"] for entry in recipe}
        recipe_buckets = {entry["bucket"] for entry in recipe}
        self.assertTrue({"StaticModel", "InstancedModel", "AnimatedModel", "SkeletalMesh", "GpuParticle"}.issubset(recipe_paths))
        self.assertEqual(recipe_buckets, {"Opaque", "Masked", "Transparent", "Additive"})

    def test_phase15_document_is_closed_and_future_renderer_work_is_deferred(self) -> None:
        self.assertIn("Phase 15 status: Complete", self.phase_doc)
        for heading in (
            "15.1 — Rasterizer / Surface Visibility",
            "15.2 — Transparent Forward Migration",
            "15.3 — Additive Forward Migration",
            "15.4 — Shared Forward Contract + Instanced Opaque/Masked",
            "15.5 — Instanced Transparent/Additive",
            "15.6 — Animated / Skeletal Forward Integration",
            "15.7 — GPU Particle Forward Integration",
            "15.8 — Validation / Diagnostics / Closure",
        ):
            self.assertIn(heading, self.phase_doc)
        self.assertIn("Future Rendering Work — Not Phase 15 Blockers", self.phase_doc)
        self.assertIn("Deferred Opaque + Forward Transparent", self.phase_doc)


if __name__ == "__main__":
    unittest.main()
