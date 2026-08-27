from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
GRAPH_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraph.h"
POOL_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraphTransientPool.h"
D3D12_POOL_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraphD3D12TransientPool.h"
BARRIER_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraphD3D12Barrier.h"
PIPELINE_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Pipeline" / "RenderPipelineController.h"


class TransientResourcePoolContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.graph = GRAPH_HEADER.read_text(encoding="utf-8")
        cls.pool = POOL_HEADER.read_text(encoding="utf-8")
        cls.d3d12_pool = D3D12_POOL_HEADER.read_text(encoding="utf-8")
        cls.barrier = BARRIER_HEADER.read_text(encoding="utf-8")
        cls.pipeline = PIPELINE_HEADER.read_text(encoding="utf-8")

    def test_pool_uses_compiled_resource_lifetimes(self) -> None:
        self.assertIn("graph.GetResourceLifetime", self.pool)
        self.assertIn("graph.GetCompiledPassHandle", self.pool)
        self.assertIn("GetCompiledPassHandle", self.graph)
        self.assertIn("lifetime->firstPass", self.pool)
        self.assertIn("request.lifetime.lastPass", self.pool)

    def test_imported_resources_are_not_transient(self) -> None:
        self.assertIn("if (lifetime->imported)", self.pool)
        self.assertIn("Imported ResourceはTransient Poolへ登録できません", self.pool)

    def test_aliasing_requires_non_overlapping_compatible_lifetimes(self) -> None:
        # Reuse is legal only when the previous lifetime ended before the new lifetime begins and compatibility matches.
        self.assertIn("slot.record.compatibilityKey != request.desc.compatibilityKey", self.pool)
        self.assertIn("slot.lastPass >= request.lifetime.firstPass", self.pool)
        self.assertIn("request.desc.allowAliasing", self.pool)
        self.assertIn("aliasingPlan_.push_back", self.pool)

    def test_pool_reports_memory_efficiency(self) -> None:
        self.assertIn("logicalBytes", self.pool)
        self.assertIn("physicalBytes", self.pool)
        self.assertIn("peakLiveBytes", self.pool)
        self.assertIn("savedBytes", self.pool)
        self.assertIn("fragmentationBytes", self.pool)
        self.assertIn("aliasingReuseCount", self.pool)

    def test_d3d12_backend_uses_allocation_info_and_placed_resources(self) -> None:
        self.assertIn("GetResourceAllocationInfo", self.d3d12_pool)
        self.assertIn("CreateHeap", self.d3d12_pool)
        self.assertIn("CreatePlacedResource", self.d3d12_pool)
        self.assertIn("heapSlot->heap.Get(),\n\t\t\t\t0,", self.d3d12_pool)
        self.assertIn("D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT", self.d3d12_pool)
        self.assertIn("D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT", self.d3d12_pool)

    def test_aliasing_barrier_has_physical_d3d12_emitter(self) -> None:
        self.assertIn("EmitAliasing", self.barrier)
        self.assertIn("D3D12_RESOURCE_BARRIER_TYPE_ALIASING", self.barrier)
        self.assertIn("barrier.Aliasing.pResourceBefore", self.barrier)
        self.assertIn("barrier.Aliasing.pResourceAfter", self.barrier)

    def test_header_only_backend_is_compiled_by_normal_pipeline_build(self) -> None:
        self.assertIn("RenderGraphTransientPool.h", self.pipeline)
        self.assertIn("RenderGraphD3D12TransientPool.h", self.pipeline)


if __name__ == "__main__":
    unittest.main()
