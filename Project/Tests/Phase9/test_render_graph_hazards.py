from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
HEADER_PATH = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraph.h"
SOURCE_PATH = PROJECT_ROOT / "Engine" / "Graphics" / "RenderGraph" / "RenderGraph.cpp"


class RenderGraphHazardContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")

    def test_resource_access_model_is_declared(self) -> None:
        self.assertIn("enum class AccessType", self.header)
        self.assertIn("enum class ResourceState", self.header)
        self.assertIn("struct ResourceAccess", self.header)
        self.assertIn("ReadWrite", self.header)
        self.assertIn("ShaderResource", self.header)
        self.assertIn("UnorderedAccess", self.header)
        self.assertIn("Present", self.header)

    def test_raw_war_waw_hazards_are_independently_tracked(self) -> None:
        self.assertIn("ReadAfterWrite", self.header)
        self.assertIn("WriteAfterRead", self.header)
        self.assertIn("WriteAfterWrite", self.header)
        self.assertIn("rawHazardCount", self.header)
        self.assertIn("warHazardCount", self.header)
        self.assertIn("wawHazardCount", self.header)

        # Phase 9 must keep the previous writer and all readers since that write to distinguish hazards.
        self.assertIn("lastWriter", self.source)
        self.assertIn("activeReaders", self.source)
        self.assertIn("HazardType::ReadAfterWrite", self.source)
        self.assertIn("HazardType::WriteAfterRead", self.source)
        self.assertIn("HazardType::WriteAfterWrite", self.source)

    def test_read_read_access_does_not_force_a_dependency(self) -> None:
        self.assertIn("only RAW, WAR and WAW hazards generate ordering edges", self.source)
        self.assertNotIn("lastAccess.assign", self.source)

    def test_dependency_records_are_exposed_for_visualization(self) -> None:
        self.assertIn("struct DependencyRecord", self.header)
        self.assertIn("GetDependencies()", self.header)
        self.assertIn("GetPassAccesses", self.header)
        self.assertIn("GetResourceName", self.header)

    def test_legacy_read_write_add_pass_api_remains_available(self) -> None:
        self.assertIn("std::vector<ResourceHandle> reads", self.header)
        self.assertIn("std::vector<ResourceHandle> writes", self.header)
        self.assertIn("AccessType::Read, ResourceState::Unknown", self.source)
        self.assertIn("AccessType::Write, ResourceState::Unknown", self.source)


if __name__ == "__main__":
    unittest.main()
