from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
RENDERER_DIR = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle" / "Renderer"
RENDERER_HEADER = RENDERER_DIR / "GpuParticleRenderer.h"
RENDERER_SOURCE = RENDERER_DIR / "GpuParticleRenderer.cpp"


class GpuParticleGpuProfilerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = RENDERER_HEADER.read_text(encoding="utf-8")
        cls.source = RENDERER_SOURCE.read_text(encoding="utf-8")

    def test_profiler_tracks_compaction_sort_graphics_and_total(self) -> None:
        for token in (
            "GpuDrivenGpuTimings",
            "GpuTimingMetric compaction",
            "GpuTimingMetric alphaSort",
            "GpuTimingMetric graphics",
            "GpuTimingMetric total",
            "GetGpuDrivenGpuTimings",
        ):
            self.assertIn(token, self.header)

        for point in ("WriteGpuTimingPoint(0)", "WriteGpuTimingPoint(1)", "WriteGpuTimingPoint(2)", "WriteGpuTimingPoint(3)"):
            self.assertIn(point, self.source)

    def test_timestamp_readback_uses_frame_resource_reuse_instead_of_waiting(self) -> None:
        # Fence generation identifies when the same frame slot is safely reusable; the profiler must never introduce a wait call itself.
        self.assertIn("GetFrameFenceValue(frameIndex)", self.source)
        self.assertIn("CollectGpuTiming(frameIndex)", self.source)
        self.assertIn("ResolveQueryData", self.source)
        self.assertIn("D3D12_HEAP_TYPE_READBACK", self.source)
        self.assertNotIn("WaitAndReset", self.source)
        self.assertNotIn("ExecuteAndWait", self.source)

    def test_profiler_is_bounded_per_frame(self) -> None:
        self.assertIn("kGpuTimingMaxSamplesPerFrame = 256", self.header)
        self.assertIn("state.sampleCount >= kGpuTimingMaxSamplesPerFrame", self.source)
        self.assertIn("kGpuTimingQueriesPerSample = 4", self.header)

    def test_active_diagnostics_pointer_is_cleared_on_destruction(self) -> None:
        self.assertIn("GetActiveRenderer", self.header)
        self.assertIn("if (activeRenderer_ == this) activeRenderer_ = nullptr", self.header)
        self.assertIn("activeRenderer_ = this", self.source)


if __name__ == "__main__":
    unittest.main()
