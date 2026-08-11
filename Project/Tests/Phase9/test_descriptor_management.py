from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRV_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Descriptor" / "SRV" / "SRVManager.h"
SRV_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Descriptor" / "SRV" / "SRVManager.cpp"
COMMAND_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Device" / "Command" / "DX12CommandManager.h"


class DescriptorManagementContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = SRV_HEADER.read_text(encoding="utf-8")
        cls.source = SRV_SOURCE.read_text(encoding="utf-8")
        cls.command_header = COMMAND_HEADER.read_text(encoding="utf-8")

    def test_persistent_and_transient_ranges_are_disjoint(self) -> None:
        self.assertIn("kTransientBeginIndex = kMaxSRVCount - kTransientSRVCount", self.header)
        self.assertIn("if (useIndex >= kTransientBeginIndex)", self.source)
        self.assertIn("state.firstIndex = nextTransientIndex", self.source)
        self.assertIn("nextTransientIndex += state.capacity", self.source)

    def test_transient_reuse_tracks_frame_fence_generation(self) -> None:
        # FrameResource fence values act as generation IDs and change only after that frame has been submitted.
        self.assertIn("GetFrameFenceValue", self.command_header)
        self.assertIn("observedFrameFenceValue", self.header)
        self.assertIn("GetFrameFenceValue(frameIndex)", self.source)
        self.assertIn("state.observedFrameFenceValue == frameFenceValue", self.source)
        self.assertIn("state.cursor = 0", self.source)
        self.assertIn("Frame fence世代が変わった時だけArenaを戻し", self.source)

    def test_allocator_rejects_submitted_command_list_window(self) -> None:
        self.assertIn("IsCommandListSubmitted", self.command_header)
        self.assertIn("IsCommandListSubmitted()", self.source)
        self.assertIn("Transient descriptors cannot be allocated while the command list is submitted", self.source)

    def test_transient_allocator_supports_contiguous_descriptor_tables(self) -> None:
        self.assertIn("AllocateTransient(uint32_t count = 1)", self.header)
        self.assertIn("if (count > state.capacity - state.cursor)", self.source)
        self.assertIn("const uint32_t firstIndex = state.firstIndex + state.cursor", self.source)
        self.assertIn("allocation.firstIndex = firstIndex", self.source)
        self.assertIn("allocation.count = count", self.source)
        self.assertIn("allocation.cpuHandle", self.source)
        self.assertIn("allocation.gpuHandle", self.source)

    def test_persistent_double_free_is_rejected(self) -> None:
        self.assertIn("persistentAllocated_", self.header)
        self.assertIn("Persistent SRV descriptor was freed twice", self.source)
        self.assertIn("srvIndex >= kTransientBeginIndex", self.source)

    def test_descriptor_diagnostics_expose_pressure(self) -> None:
        for field in (
            "persistentInUse",
            "persistentHighWater",
            "transientCapacityPerFrame",
            "transientInUse",
            "transientHighWater",
            "transientAllocationCount",
            "transientReclaimedCount",
            "transientFrameRecycleCount",
            "exhaustionCount",
        ):
            self.assertIn(field, self.header)
        self.assertIn("GetDescriptorStats", self.header)
        self.assertIn("GetDescriptorStats() const", self.source)

    def test_transient_capacity_is_partitioned_by_frame_resource_count(self) -> None:
        self.assertIn("GetFrameResourceCount()", self.source)
        self.assertIn("transientFrameStates_.resize(frameCount)", self.source)
        self.assertIn("kTransientSRVCount / frameCount", self.source)
        self.assertIn("kTransientSRVCount % frameCount", self.source)


if __name__ == "__main__":
    unittest.main()
