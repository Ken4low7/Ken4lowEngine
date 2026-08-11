from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
HEADER_PATH = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraph.h"
SOURCE_PATH = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraph.cpp"


class RenderGraphCullingContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")

    def test_output_and_side_effect_roots_are_explicit(self) -> None:
        self.assertIn("MarkResourceOutput", self.header)
        self.assertIn("MarkPassSideEffect", self.header)
        self.assertIn("bool output = false", self.header)
        self.assertIn("bool sideEffect = false", self.header)

    def test_compile_stats_expose_culling_result(self) -> None:
        self.assertIn("executedPassCount", self.header)
        self.assertIn("culledPassCount", self.header)
        self.assertIn("sideEffectPassCount", self.header)
        self.assertIn("outputResourceCount", self.header)
        self.assertIn("IsPassCulled", self.header)

    def test_culling_runs_after_sort_and_before_barriers(self) -> None:
        sort_position = self.source.index("compiledOrder_.size() != passes_.size()")
        cull_position = self.source.index("ApplyPassCulling();")
        lifetime_position = self.source.index("RebuildResourceLifetimes();")
        barrier_position = self.source.index("BuildBarrierPlan(outError)")
        self.assertLess(sort_position, cull_position)
        self.assertLess(cull_position, lifetime_position)
        self.assertLess(lifetime_position, barrier_position)

    def test_only_required_dependencies_keep_predecessors_alive(self) -> None:
        # RAW carries data and Explicit carries declared side effects; WAR/WAW only order passes that already survive.
        self.assertIn("dependency.hazard == HazardType::Explicit || dependency.hazard == HazardType::ReadAfterWrite", self.source)
        self.assertIn("WAR/WAW edges order surviving passes but never keep dead work alive", self.source)

    def test_graph_without_roots_keeps_legacy_behavior(self) -> None:
        self.assertIn("if (!hasCullingRoot)", self.source)
        self.assertIn("std::fill(livePassMask_.begin(), livePassMask_.end(), uint8_t{ 1 })", self.source)
        self.assertIn("compileStats_.culledPassCount = 0", self.source)

    def test_resource_lifetimes_are_rebuilt_from_surviving_schedule(self) -> None:
        self.assertIn("void RenderGraph::RebuildResourceLifetimes()", self.source)
        self.assertIn("scheduleIndex < compiledOrder_.size()", self.source)
        self.assertIn("resource.lifetime.firstPass = (std::min)(resource.lifetime.firstPass, scheduleIndex)", self.source)
        self.assertIn("resource.lifetime.lastPass = (std::max)(resource.lifetime.lastPass, scheduleIndex)", self.source)


if __name__ == "__main__":
    unittest.main()
