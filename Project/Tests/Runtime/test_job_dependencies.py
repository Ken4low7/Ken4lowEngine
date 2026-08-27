from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONCURRENCY_DIR = PROJECT_ROOT / "Engine" / "Core" / "Concurrency"
HEADER_PATH = CONCURRENCY_DIR / "JobSystem.h"
SOURCE_PATH = CONCURRENCY_DIR / "JobSystem.cpp"
RUNTIME_TEST_PATH = Path(__file__).with_name("JobDependencyRuntimeTests.cpp")


class JobDependencyContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")

    def test_dependency_aware_api_is_exposed(self) -> None:
        self.assertIn("DispatchAfter", self.header)
        self.assertIn("ParallelForAfter", self.header)
        self.assertIn("const std::vector<JobHandle>& dependencies", self.header)

    def test_dependency_batch_waits_for_registration_and_all_prerequisites(self) -> None:
        self.assertIn("struct DependencyBatch", self.header)
        self.assertIn("remainingDependencies", self.header)
        self.assertIn("registrationComplete", self.header)
        self.assertIn("enqueued", self.header)
        self.assertIn("RegisterCompletionCallback", self.source)
        self.assertIn("TryEnqueueDependencyBatch", self.source)
        self.assertIn("compare_exchange_strong", self.source)

    def test_completion_releases_registered_dependents(self) -> None:
        self.assertIn("completionCallbacks", self.header)
        self.assertIn("completionCallbacks = std::move(state->completionCallbacks)", self.source)
        self.assertIn("if (callback) callback();", self.source)

    def test_parallel_for_chunks_share_one_completion_state(self) -> None:
        self.assertIn("state->remaining.store(chunkCount", self.source)
        self.assertIn("tasks.reserve(chunkCount)", self.source)
        self.assertIn("EnqueueTasksAfterDependencies(std::move(tasks), dependencies, priority)", self.source)

    def test_runtime_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Compile only the portable JobSystem translation unit so runtime dependency ordering is exercised in CI.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase10_") as temp_dir:
            executable = Path(temp_dir) / "job_dependency_tests"
            command = [
                compiler,
                "-std=c++20",
                "-pthread",
                str(RUNTIME_TEST_PATH),
                str(SOURCE_PATH),
                "-I",
                str(CONCURRENCY_DIR),
                "-o",
                str(executable),
            ]
            compile_result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ runtime test compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )

            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("Job Dependency runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
