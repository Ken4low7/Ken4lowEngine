from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRV_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Descriptor" / "SRV" / "SRVManager.h"
SRV_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Descriptor" / "SRV" / "SRVManager.cpp"


class DescriptorManagementContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = SRV_HEADER.read_text(encoding="utf-8")
        cls.source = SRV_SOURCE.read_text(encoding="utf-8")

    def test_persistent_and_transient_ranges_are_disjoint(self) -> None:
        self.assertIn("kTransientBeginIndex = kMaxSRVCount - kTransientSRVCount", self.header)
        self.assertIn("if (useIndex >= kTransientBeginIndex)", self.source)
        self.assertIn("freeTransientRanges_.push_back({ kTransientBeginIndex, kTransientSRVCount })", self.source)

    def test_transient_ranges_retire_against_gpu_fence(self) -> None:
        # Fence+1 protects descriptors referenced by the command list currently being recorded.
        self.assertIn("GetCurrentValue() + 1", self.source)
        self.assertIn("GetCompletedValue()", self.source)
        self.assertIn("completedFenceValue < it->retireFenceValue", self.source)
        self.assertIn("InsertFreeTransientRangeLocked(it->range)", self.source)

    def test_transient_allocator_supports_contiguous_descriptor_tables(self) -> None:
        self.assertIn("AllocateTransient(uint32_t count = 1)", self.header)
        self.assertIn("if (freeRange.count < count)", self.source)
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
            "transientInFlight",
            "transientHighWater",
            "pendingTransientRangeCount",
            "transientAllocationCount",
            "transientReclaimedCount",
            "exhaustionCount",
        ):
            self.assertIn(field, self.header)
        self.assertIn("GetDescriptorStats", self.header)
        self.assertIn("GetDescriptorStats() const", self.source)

    def test_transient_reclamation_merges_free_ranges(self) -> None:
        self.assertIn("std::sort(", self.source)
        self.assertIn("freeTransientRanges_.begin(), freeTransientRanges_.end()", self.source)
        self.assertIn("freeTransientRanges_ = std::move(merged)", self.source)
        self.assertIn("Fence完了後だけRangeを戻し", self.source)


if __name__ == "__main__":
    unittest.main()
