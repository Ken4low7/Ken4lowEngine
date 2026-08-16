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
SHADER_MANIFEST = PROJECT_ROOT / "Engine" / "Graphics" / "Shader" / "Manifest" / "GpuParticleShaderManifest.h"
UAV_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Descriptor" / "UAV" / "UAVManager.h"
UAV_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Descriptor" / "UAV" / "UAVManager.cpp"
RESOURCE_MANAGER_SOURCE = PROJECT_ROOT / "Engine" / "System" / "Resource" / "ResourceManager.cpp"
SPRITE_VS = SHADER_ROOT / "GpuParticle.VS.hlsl"
MESH_VS = SHADER_ROOT / "GpuParticleMesh.VS.hlsl"
COMPACT_CS = SHADER_ROOT / "GpuParticleCompact.CS.hlsl"
SORT_CS = SHADER_ROOT / "GpuParticleSort.CS.hlsl"


class GpuDrivenParticleRenderingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.buffers_h = BUFFERS_HEADER.read_text(encoding="utf-8")
        cls.buffers_cpp = BUFFERS_SOURCE.read_text(encoding="utf-8")
        cls.renderer_h = RENDERER_HEADER.read_text(encoding="utf-8")
        cls.renderer_cpp = RENDERER_SOURCE.read_text(encoding="utf-8")
        cls.sprite_pipeline = SPRITE_PIPELINE.read_text(encoding="utf-8")
        cls.mesh_pipeline = MESH_PIPELINE.read_text(encoding="utf-8")
        cls.manifest = SHADER_MANIFEST.read_text(encoding="utf-8")
        cls.uav_h = UAV_HEADER.read_text(encoding="utf-8")
        cls.uav_cpp = UAV_SOURCE.read_text(encoding="utf-8")
        cls.resource_manager_cpp = RESOURCE_MANAGER_SOURCE.read_text(encoding="utf-8")
        cls.sprite_vs = SPRITE_VS.read_text(encoding="utf-8")
        cls.mesh_vs = MESH_VS.read_text(encoding="utf-8")
        cls.compact_cs = COMPACT_CS.read_text(encoding="utf-8")
        cls.sort_cs = SORT_CS.read_text(encoding="utf-8")

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
        self.assertIn("D3D12_RESOURCE_STATE_UNORDERED_ACCESS", self.renderer_cpp)

    def test_default_heap_gpu_driven_buffers_start_in_common(self) -> None:
        # Buffer creation must match the runtime contract before first-use UAV promotion.
        gpu_driven_section = self.buffers_cpp.split("void GpuParticleBuffers::CreateGpuDrivenDrawBuffers()", 1)[1]
        self.assertNotIn("D3D12_RESOURCE_STATE_UNORDERED_ACCESS", gpu_driven_section)
        self.assertGreaterEqual(gpu_driven_section.count("D3D12_RESOURCE_STATE_COMMON"), 2)
        self.assertIn("type == D3D12_HEAP_TYPE_DEFAULT", self.resource_manager_cpp)
        self.assertIn("actualInitState = D3D12_RESOURCE_STATE_COMMON", self.resource_manager_cpp)

    def test_gpu_driven_uint_scratch_buffers_use_r32_typed_views(self) -> None:
        # Visible index / indirect args are scalar uint arrays, so ClearUAV and HLSL use one typed R32_UINT view contract.
        gpu_driven_section = self.buffers_cpp.split("void GpuParticleBuffers::CreateGpuDrivenDrawBuffers()", 1)[1]
        self.assertGreaterEqual(gpu_driven_section.count("DXGI_FORMAT_R32_UINT"), 3)
        self.assertGreaterEqual(gpu_driven_section.count("StructureByteStride = 0"), 3)
        self.assertNotIn("CreateUAVForStructuredBuffer(\n\t\tvisibleParticleIndexUavIndex_", gpu_driven_section)
        self.assertNotIn("CreateUAVForStructuredBuffer(\n\t\tindirectDrawArgsUavIndex_", gpu_driven_section)
        self.assertIn("GetClearCPUDescriptorHandle(visibleParticleIndexUavIndex_)", gpu_driven_section)
        self.assertIn("GetClearCPUDescriptorHandle(indirectDrawArgsUavIndex_)", gpu_driven_section)
        self.assertIn("Buffer<uint> gVisibleParticleIndices : register(t1)", self.sprite_vs)
        self.assertIn("Buffer<uint> gVisibleParticleIndices : register(t1)", self.mesh_vs)
        self.assertIn("RWBuffer<uint> gVisibleParticleIndices : register(u1)", self.compact_cs)
        self.assertIn("RWBuffer<uint> gIndirectDrawArgs : register(u2)", self.compact_cs)
        self.assertIn("RWBuffer<uint> gVisibleParticleIndices : register(u1)", self.sort_cs)
        self.assertIn("RWBuffer<uint> gIndirectDrawArgs : register(u2)", self.sort_cs)

    def test_compaction_filters_dead_and_other_render_groups(self) -> None:
        # CPU reference keeps the intended visible-index contract readable beside the HLSL implementation.
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
            self.assertIn("Buffer<uint> gVisibleParticleIndices : register(t1)", shader)
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

    def test_alpha_blend_uses_gpu_back_to_front_bitonic_sort(self) -> None:
        # Farther positive NDC depth must appear first for conventional alpha blending.
        depths = [(10, 0.2), (20, 0.9), (30, 0.5)]
        self.assertEqual([index for index, _ in sorted(depths, key=lambda item: (-item[1], item[0]))], [20, 30, 10])

        self.assertIn("SortVisibleParticlesByDepth", self.renderer_cpp)
        self.assertIn("BlendMode::kBlendModeNormal", self.renderer_cpp)
        self.assertIn("SetComputeRoot32BitConstants", self.renderer_cpp)
        self.assertIn("sortLevelMask", self.sort_cs)
        self.assertIn("partnerIndex = index ^ gSort.sortLevelMask", self.sort_cs)
        self.assertIn("return -(clipPosition.z / clipPosition.w)", self.sort_cs)
        self.assertIn("GPU_PARTICLE_INVALID_INDEX", self.sort_cs)
        self.assertIn("GpuParticleComputeShaderId::SortCS", self.manifest)

    def test_alpha_sort_limits_compare_network_to_visible_power_of_two_capacity(self) -> None:
        # GPU InstanceCount remains on-device and decides how much of each fixed dispatch performs useful comparisons.
        self.assertIn("ResolveSortCapacity", self.sort_cs)
        self.assertIn("gIndirectDrawArgs[1]", self.sort_cs)
        self.assertIn("visibleCount - 1u", self.sort_cs)
        self.assertIn("gSort.sortLevel > sortCapacity", self.sort_cs)
        self.assertIn("partnerIndex >= sortCapacity", self.sort_cs)
        self.assertIn("compactBarriers[1].UAV.pResource = indirectBuffer", self.renderer_cpp)

    def test_uav_clears_use_direct_cpu_only_descriptors(self) -> None:
        # D3D12 requires ClearUAV's CPU descriptor to come from a non-shader-visible heap.
        self.assertIn("clearCpuDescriptorHeap_", self.uav_h)
        self.assertIn("D3D12_DESCRIPTOR_HEAP_FLAG_NONE", self.uav_cpp)
        self.assertNotIn("MirrorUavDescriptorForClear", self.uav_h + self.uav_cpp)
        self.assertNotIn("CopyDescriptorsSimple", self.uav_cpp)
        self.assertGreaterEqual(self.uav_cpp.count("CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetClearCPUDescriptorHandle(uavIndex))"), 3)
        self.assertIn("GetClearCPUDescriptorHandle", self.renderer_cpp)
        self.assertIn("D3D12_RESOURCE_BARRIER_TYPE_UAV", self.renderer_cpp)

    def test_resource_states_bridge_compute_to_graphics_and_indirect(self) -> None:
        self.assertIn("D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE", self.renderer_cpp)
        self.assertIn("D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT", self.renderer_cpp)
        self.assertIn("D3D12_RESOURCE_STATE_UNORDERED_ACCESS", self.renderer_cpp)
        self.assertIn("gpuDrivenBuffersReadable_", self.renderer_h)


if __name__ == "__main__":
    unittest.main()
