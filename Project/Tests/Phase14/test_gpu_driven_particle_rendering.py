from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
GPU_ROOT = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle"
SHADER_ROOT = PROJECT_ROOT / "Resources" / "Shaders" / "GpuParticle"
BUFFERS_HEADER = GPU_ROOT / "Buffers" / "GpuParticleBuffers.h"
BUFFERS_SOURCE = GPU_ROOT / "Buffers" / "GpuParticleBuffers.cpp"
RENDERER_HEADER = GPU_ROOT / "Renderer" / "GpuParticleRenderer.h"
RENDERER_SOURCE = GPU_ROOT / "Renderer" / "GpuParticleRenderer.cpp"
SPRITE_PIPELINE = GPU_ROOT / "Pipeline" / "GpuParticleSpritePipeline.cpp"
MESH_PIPELINE = GPU_ROOT / "Pipeline" / "GpuParticleMeshPipeline.cpp"
SPRITE_VS = SHADER_ROOT / "GpuParticle.VS.hlsl"
MESH_VS = SHADER_ROOT / "GpuParticleMesh.VS.hlsl"
COMPACT_CS = SHADER_ROOT / "GpuParticleCompact.CS.hlsl"


class GpuDrivenParticleRenderingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.buffers_h = BUFFERS_HEADER.read_text(encoding="utf-8")
        cls.buffers_cpp = BUFFERS_SOURCE.read_text(encoding="utf-8")
        cls.renderer_h = RENDERER_HEADER.read_text(encoding="utf-8")
        cls.renderer_cpp = RENDERER_SOURCE.read_text(encoding="utf-8")
        cls.sprite_pipeline = SPRITE_PIPELINE.read_text(encoding="utf-8")
        cls.mesh_pipeline = MESH_PIPELINE.read_text(encoding="utf-8")
        cls.sprite_vs = SPRITE_VS.read_text(encoding="utf-8")
        cls.mesh_vs = MESH_VS.read_text(encoding="utf-8")
        cls.compact_cs = COMPACT_CS.read_text(encoding="utf-8")

    def test_gpu_buffers_hold_visible_indices_and_indirect_arguments(self) -> None:
        for token in (
            "visibleParticleIndexBuffer_",
            "indirectDrawArgsBuffer_",
            "GetVisibleParticleIndexSrvIndex",
            "GetVisibleParticleIndexUavIndex",
            "GetIndirectDrawArgsUavIndex",
            "GetGpuDrivenDrawCBAddress",
        ):
            self.assertIn(token, self.buffers_h + self.buffers_cpp)
        self.assertIn("D3D12_RESOURCE_STATE_UNORDERED_ACCESS", self.buffers_cpp)

    def test_compaction_filters_dead_and_other_render_groups(self) -> None:
        # CPU reference keeps the intended stable visible-index contract readable beside the HLSL implementation.
        particles = [
            {"alive": True, "alpha": 1.0, "group": 7},
            {"alive": False, "alpha": 1.0, "group": 7},
            {"alive": True, "alpha": 0.0, "group": 7},
            {"alive": True, "alpha": 1.0, "group": 9},
            {"alive": True, "alpha": 0.5, "group": 7},
        ]
        compacted = [
            index
            for index, particle in enumerate(particles)
            if particle["alive"] and particle["alpha"] > 0.0 and particle["group"] == 7
        ]
        self.assertEqual(compacted, [0, 4])

        self.assertIn("particle.lifeTime <= 0.0f", self.compact_cs)
        self.assertIn("particle.color.a <= 0.0f", self.compact_cs)
        self.assertIn("particle.type != gDraw.renderGroup", self.compact_cs)
        self.assertIn("InterlockedAdd(gIndirectDrawArgs[1]", self.compact_cs)
        self.assertIn("gVisibleParticleIndices[compactedIndex] = particleIndex", self.compact_cs)

    def test_vertex_shaders_resolve_compacted_indices(self) -> None:
        for shader in (self.sprite_vs, self.mesh_vs):
            self.assertIn("StructuredBuffer<uint> gVisibleParticleIndices : register(t1)", shader)
            self.assertIn("gVisibleParticleIndices[instanceId]", shader)
            self.assertIn("gParticles[particleIndex]", shader)

        self.assertIn("BaseShaderRegister = 1", self.sprite_pipeline)
        self.assertIn("BaseShaderRegister = 1", self.mesh_pipeline)

    def test_renderer_builds_indirect_commands_on_gpu(self) -> None:
        for token in (
            "CreateGpuDrivenPipeline",
            "CreateIndirectCommandSignatures",
            "BuildVisibleParticleList",
            "ClearUnorderedAccessViewUint",
            "D3D12_INDIRECT_ARGUMENT_TYPE_DRAW",
            "D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED",
            "ExecuteIndirect",
        ):
            self.assertIn(token, self.renderer_cpp)

        self.assertNotIn("commandList->DrawInstanced", self.renderer_cpp)
        self.assertNotIn("commandList->DrawIndexedInstanced", self.renderer_cpp)

    def test_resource_states_bridge_compute_to_graphics_and_indirect(self) -> None:
        self.assertIn("D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE", self.renderer_cpp)
        self.assertIn("D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT", self.renderer_cpp)
        self.assertIn("D3D12_RESOURCE_STATE_UNORDERED_ACCESS", self.renderer_cpp)
        self.assertIn("gpuDrivenBuffersReadable_", self.renderer_h)


if __name__ == "__main__":
    unittest.main()
