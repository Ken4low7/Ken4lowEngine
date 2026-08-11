from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
HEADER_PATH = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraph.h"
SOURCE_PATH = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraph.cpp"
D3D12_ADAPTER_PATH = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraphD3D12Barrier.h"


class RenderGraphBarrierContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")
        cls.adapter = D3D12_ADAPTER_PATH.read_text(encoding="utf-8")

    def test_barrier_plan_types_are_exposed(self) -> None:
        self.assertIn("enum class BarrierType", self.header)
        self.assertIn("enum class BarrierPlacement", self.header)
        self.assertIn("struct BarrierRecord", self.header)
        self.assertIn("GetBarrierPlan()", self.header)
        self.assertIn("BarrierType::Transition", self.source)
        self.assertIn("BarrierType::UnorderedAccess", self.source)

    def test_resource_initial_and_final_states_are_part_of_contract(self) -> None:
        self.assertIn("ResourceState initialState", self.header)
        self.assertIn("ResourceState finalState", self.header)
        self.assertIn("node.initialState = initialState", self.source)
        self.assertIn("node.finalState = finalState", self.source)
        self.assertIn("BarrierPlacement::AfterGraph", self.source)

    def test_barriers_are_planned_after_topological_sort(self) -> None:
        sort_position = self.source.index("compiledOrder_.size() != passes_.size()")
        plan_position = self.source.index("BuildBarrierPlan(outError)")
        self.assertLess(sort_position, plan_position)
        self.assertIn("for (uint32_t passIndex : compiledOrder_)", self.source)

    def test_unknown_state_does_not_guess_a_transition(self) -> None:
        self.assertIn("unknownStateAccessCount", self.header)
        self.assertIn("currentStates[resourceIndex] = ResourceState::Unknown", self.source)
        self.assertIn("currentState != ResourceState::Unknown && currentState != requestedState", self.source)

    def test_uav_write_ordering_is_explicit(self) -> None:
        self.assertIn("uavBarrierCount", self.header)
        self.assertIn("previousWrites || currentWrites", self.source)
        self.assertIn("previousAccessStates[resourceIndex] == ResourceState::UnorderedAccess", self.source)

    def test_execution_hook_places_barriers_around_passes(self) -> None:
        self.assertIn("using BarrierCallback", self.header)
        self.assertIn("BarrierPlacement::BeforePass", self.source)
        self.assertIn("BarrierPlacement::AfterGraph", self.source)
        self.assertIn("barrierCallback(barrier)", self.source)

    def test_d3d12_adapter_maps_and_emits_native_barriers(self) -> None:
        self.assertIn("class RenderGraphD3D12BarrierEmitter", self.adapter)
        self.assertIn("D3D12_RESOURCE_BARRIER_TYPE_TRANSITION", self.adapter)
        self.assertIn("D3D12_RESOURCE_BARRIER_TYPE_UAV", self.adapter)
        self.assertIn("D3D12_RESOURCE_STATE_RENDER_TARGET", self.adapter)
        self.assertIn("D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE", self.adapter)
        self.assertIn("D3D12_RESOURCE_STATE_PRESENT", self.adapter)
        self.assertIn("commandList->ResourceBarrier(1, &barrier)", self.adapter)

    def test_d3d12_adapter_requires_explicit_resource_binding(self) -> None:
        self.assertIn("BindResource", self.adapter)
        self.assertIn("ResolveResource", self.adapter)
        self.assertIn("ResourceにD3D12 ResourceがBindされていません", self.adapter)
        self.assertIn("legacy owner-managed barriers are never duplicated accidentally", self.adapter)

    def test_debug_fill_uses_exact_uint8_type(self) -> None:
        self.assertNotIn("std::fill(accessMasks.begin(), accessMasks.end(), 0u)", self.source)
        self.assertIn("std::fill(accessMasks.begin(), accessMasks.end(), uint8_t{ 0 })", self.source)


if __name__ == "__main__":
    unittest.main()
