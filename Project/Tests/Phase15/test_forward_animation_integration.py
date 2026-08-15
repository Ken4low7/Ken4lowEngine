from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
FORWARD_QUEUE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Forward" / "ForwardRenderQueue.h"
ANIMATION_PIPELINE_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Pipeline" / "AnimationPipelineBuilder.h"
ANIMATION_PIPELINE_CPP = ANIMATION_PIPELINE_H.with_suffix(".cpp")
ANIMATION_FORWARD_SCOPE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Pipeline" / "AnimationForwardSurfaceScope.h"
ANIMATED_COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponent.h"
ANIMATED_COMPONENT_FORWARD = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "AnimatedModelComponentForward.inl"
SKELETAL_COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponent.h"
SKELETAL_COMPONENT_FORWARD = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "SkeletalMeshComponentForward.inl"
ACTOR_CPP = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Core" / "Actor.cpp"
ACTOR_WORLD_DRAW = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Core" / "ActorWorld_Draw.cpp"


class ForwardAnimationIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.forward_queue = FORWARD_QUEUE.read_text(encoding="utf-8")
        cls.animation_pipeline_h = ANIMATION_PIPELINE_H.read_text(encoding="utf-8")
        cls.animation_pipeline_cpp = ANIMATION_PIPELINE_CPP.read_text(encoding="utf-8")
        cls.animation_forward_scope = ANIMATION_FORWARD_SCOPE.read_text(encoding="utf-8")
        cls.animated_component_h = ANIMATED_COMPONENT_H.read_text(encoding="utf-8")
        cls.animated_component_forward = ANIMATED_COMPONENT_FORWARD.read_text(encoding="utf-8")
        cls.skeletal_component_h = SKELETAL_COMPONENT_H.read_text(encoding="utf-8")
        cls.skeletal_component_forward = SKELETAL_COMPONENT_FORWARD.read_text(encoding="utf-8")
        cls.actor_cpp = ACTOR_CPP.read_text(encoding="utf-8")
        cls.actor_world_draw = ACTOR_WORLD_DRAW.read_text(encoding="utf-8")

    def test_forward_queue_exposes_payload_ownership(self) -> None:
        self.assertIn("bool OwnsPayload(const void* payload) const", self.forward_queue)
        self.assertIn("item.payload == payload", self.forward_queue)
        self.assertIn("!forwardQueue->OwnsPayload(component)", self.actor_cpp)

    def test_animation_pipeline_has_forward_blend_variants(self) -> None:
        self.assertIn("SetSurfaceBlendMode", self.animation_pipeline_h)
        self.assertIn("MaterialBlendMode surfaceBlendMode_", self.animation_pipeline_h)
        self.assertIn("alphaPipelineState_", self.animation_pipeline_h)
        self.assertIn("additivePipelineState_", self.animation_pipeline_h)
        self.assertIn("switch (surfaceBlendMode_)", self.animation_pipeline_cpp)
        self.assertIn("PipelineStatePresets::MakeBlendAlpha()", self.animation_pipeline_cpp)
        self.assertIn("PipelineStatePresets::MakeBlendAdditive()", self.animation_pipeline_cpp)
        self.assertGreaterEqual(self.animation_pipeline_cpp.count("PipelineStatePresets::MakeDepthReadOnly()"), 2)

    def test_animation_forward_scope_restores_pipeline_classification(self) -> None:
        self.assertIn("binding.Resolve(desc) ? desc.blendMode", self.animation_forward_scope)
        self.assertIn("class ScopedBlendMode", self.animation_forward_scope)
        self.assertIn("previousBlendMode_(builder_->GetSurfaceBlendMode())", self.animation_forward_scope)
        self.assertIn("builder_->SetSurfaceBlendMode(previousBlendMode_)", self.animation_forward_scope)
        self.assertIn("GetActiveCameraForward", self.animation_forward_scope)

    def assert_component_supports_all_buckets(self, header: str, adapter: str, component_name: str) -> None:
        for bucket_name in ("Opaque", "Masked", "Transparent", "Additive"):
            self.assertIn(f"SubmitForward{bucket_name}", header)
            self.assertIn(f"SubmitForward{bucket_name}", adapter)
        self.assertIn("MakeForwardRenderItem", adapter)
        self.assertIn("AnimationForwardSurface::ScopedBlendMode", adapter)
        self.assertIn(f"static_cast<{component_name}*>(payload)", adapter)
        self.assertIn("component->Draw()", adapter)
        self.assertIn("CalculateSortDepth(GetWorldPosition())", adapter)

    def test_animated_component_supports_all_forward_buckets(self) -> None:
        self.assert_component_supports_all_buckets(
            self.animated_component_h,
            self.animated_component_forward,
            "AnimatedModelComponent",
        )

    def test_skeletal_component_supports_all_forward_buckets(self) -> None:
        self.assert_component_supports_all_buckets(
            self.skeletal_component_h,
            self.skeletal_component_forward,
            "SkeletalMeshComponent",
        )

    def test_actor_world_collects_animated_and_skeletal_buckets(self) -> None:
        self.assertIn('#include "AnimatedModelComponent.h"', self.actor_world_draw)
        self.assertIn('#include "SkeletalMeshComponent.h"', self.actor_world_draw)
        for component_name in ("animatedModelComponent", "skeletalMeshComponent"):
            for bucket_name in ("Opaque", "Masked", "Transparent", "Additive"):
                self.assertIn(
                    f"{component_name}->SubmitForward{bucket_name}(*forwardQueue)",
                    self.actor_world_draw,
                )

    def test_forward_execution_order_is_preserved(self) -> None:
        opaque_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Opaque)")
        masked_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Masked)")
        legacy_draw = self.actor_world_draw.index("actor->Draw()")
        transparent_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Transparent)")
        additive_execute = self.actor_world_draw.index("ExecuteBucket(ForwardRenderBucket::Additive)")
        self.assertLess(opaque_execute, masked_execute)
        self.assertLess(masked_execute, legacy_draw)
        self.assertLess(legacy_draw, transparent_execute)
        self.assertLess(transparent_execute, additive_execute)


if __name__ == "__main__":
    unittest.main()
