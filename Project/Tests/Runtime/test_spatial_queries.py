from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
LEVEL_DIR = PROJECT_ROOT / "Engine" / "Scene" / "Level"
GEOMETRY_DIR = PROJECT_ROOT / "Engine" / "Math" / "Geometry"
VECTOR_DIR = PROJECT_ROOT / "Engine" / "Math" / "Vector"
INDEX_HEADER = LEVEL_DIR / "StageSpatialQueryIndex.h"
STAGE_HEADER = LEVEL_DIR / "Stage.h"
STAGE_SOURCE = LEVEL_DIR / "Stage.cpp"
CHARACTER_MOVEMENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Character" / "CharacterMovementComponent.cpp"
RUNTIME_TEST = Path(__file__).with_name("StageSpatialQueryRuntimeTests.cpp")


class StageSpatialQueryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.index_header = INDEX_HEADER.read_text(encoding="utf-8")
        cls.stage_header = STAGE_HEADER.read_text(encoding="utf-8")
        cls.stage_source = STAGE_SOURCE.read_text(encoding="utf-8")
        cls.character_movement = CHARACTER_MOVEMENT.read_text(encoding="utf-8")

    def test_index_is_persistent_xz_uniform_grid(self) -> None:
        self.assertIn("class StageSpatialQueryIndex", self.index_header)
        self.assertIn("std::unordered_map<CellCoord", self.index_header)
        self.assertIn("TryGetCellRange", self.index_header)
        self.assertIn("cells_[{ x, z }]", self.index_header)
        self.assertIn("stats_.usedCellCount", self.index_header)

    def test_query_is_const_deterministic_and_has_no_shared_scratch(self) -> None:
        self.assertIn("void Query(const AABB& queryBounds, std::vector<std::size_t>& outIndices) const", self.index_header)
        self.assertIn("std::sort(outIndices.begin(), outIndices.end())", self.index_header)
        self.assertIn("std::unique(outIndices.begin(), outIndices.end())", self.index_header)
        self.assertNotIn("mutable std::vector", self.index_header)

    def test_stage_builds_and_clears_all_static_query_indices(self) -> None:
        self.assertIn("RebuildSpatialQueryIndices", self.stage_header)
        self.assertIn("worldSpatialIndex_", self.stage_header)
        self.assertIn("floorSpatialIndex_", self.stage_header)
        self.assertIn("wallObstacleSpatialIndex_", self.stage_header)
        self.assertIn("navigationObstacleSpatialIndex_", self.stage_header)
        self.assertIn("ladderSpatialIndex_", self.stage_header)
        self.assertIn("RebuildSpatialQueryIndices();", self.stage_source)
        self.assertIn("worldSpatialIndex_.Build(worldAABBs_)", self.stage_source)
        self.assertIn("ladderSpatialIndex_.Clear()", self.stage_source)

    def test_stage_exposes_candidate_queries_and_stats(self) -> None:
        self.assertIn("QueryWorldAabbCandidates", self.stage_header)
        self.assertIn("QueryFloorAabbCandidates", self.stage_header)
        self.assertIn("QueryWallObstacleAabbCandidates", self.stage_header)
        self.assertIn("QueryNavigationObstacleAabbCandidates", self.stage_header)
        self.assertIn("QueryLadderAabbCandidates", self.stage_header)
        self.assertIn("StageSpatialQueryStats GetSpatialQueryStats() const", self.stage_header)
        self.assertIn("ladderSpatialIndex_.Query(queryBounds, outIndices)", self.stage_source)

    def test_hot_queries_use_grid_candidates_before_narrow_checks(self) -> None:
        self.assertIn("QueryLadderAabbCandidates(playerAABB, candidates)", self.stage_source)
        self.assertIn("QueryWallObstacleAabbCandidates(traversalQuery, wallCandidates)", self.character_movement)
        self.assertIn("QueryFloorAabbCandidates(traversalQuery, floorCandidates)", self.character_movement)
        self.assertNotIn("for (const AABB& ladderAABB : ladderAABBs_)", self.stage_source)
        self.assertNotIn("for (size_t index = 0; index < wallObstacles.size(); ++index)", self.character_movement)

    def test_runtime_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Compile only the portable immutable grid so runtime correctness stays independent from DirectX/Win32.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase10_spatial_") as temp_dir:
            executable = Path(temp_dir) / "stage_spatial_query_tests"
            command = [
                compiler,
                "-std=c++20",
                "-pthread",
                str(RUNTIME_TEST),
                "-I",
                str(LEVEL_DIR),
                "-I",
                str(GEOMETRY_DIR),
                "-I",
                str(VECTOR_DIR),
                "-o",
                str(executable),
            ]
            compile_result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ spatial query runtime test compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )

            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ spatial query runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("Stage Spatial Query runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
