from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
STREAMING_DIR = PROJECT_ROOT / "Engine" / "Scene" / "Streaming"
GRID_HEADER = STREAMING_DIR / "WorldPartitionGrid.h"
TELEMETRY_HEADER = PROJECT_ROOT / "Engine" / "Core" / "Diagnostics" / "ReliabilityTelemetry.h"
ALLOCATION_HEADER = PROJECT_ROOT / "Engine" / "DebugTools" / "Performance" / "FrameAllocationTracker.h"
RUNTIME_TEST = Path(__file__).with_name("StreamingStressRuntimeTests.cpp")


class StreamingStressTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.telemetry = TELEMETRY_HEADER.read_text(encoding="utf-8")
        cls.allocation = ALLOCATION_HEADER.read_text(encoding="utf-8")

    def test_runtime_telemetry_is_sampled_from_every_completed_frame(self) -> None:
        self.assertIn("RecordCurrentFrame(allocatedBytes)", self.allocation)
        self.assertIn("GetPendingRequestCount()", self.telemetry)
        self.assertIn("GetQueuedCompletionCount()", self.telemetry)
        self.assertIn("GetLoadedSubLevelCount()", self.telemetry)

    def test_environment_can_enable_streaming_stress_and_timed_soak(self) -> None:
        self.assertIn("KEN4LOW_RELIABILITY_CSV", self.telemetry)
        self.assertIn("KEN4LOW_SOAK_SECONDS", self.telemetry)
        self.assertIn("KEN4LOW_STREAMING_STRESS", self.telemetry)
        self.assertIn("worldPartition->Update(source)", self.telemetry)
        self.assertIn("PostQuitMessage(0)", self.telemetry)

    def test_world_partition_stress_runtime_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # The grid helper is pure C++, allowing a high-iteration stress test to run on Linux CI without DirectX.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase12_streaming_") as temp_dir:
            executable = Path(temp_dir) / "streaming_stress_tests"
            command = [
                compiler,
                "-std=c++20",
                str(RUNTIME_TEST),
                "-I",
                str(STREAMING_DIR),
                "-O2",
                "-o",
                str(executable),
            ]
            compile_result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ streaming stress compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )
            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ streaming stress runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("Streaming stress runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
