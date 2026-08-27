from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONCURRENCY_DIR = PROJECT_ROOT / "Engine" / "Core" / "Concurrency"
HEADER_PATH = CONCURRENCY_DIR / "JobSystem.h"
SOURCE_PATH = CONCURRENCY_DIR / "JobSystem.cpp"
DEBUG_SCENE_HEADER = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "DebugScene" / "DebugScene.h"
DEBUG_SCENE_SOURCE = PROJECT_ROOT / "ApplicationLayer" / "Scene" / "DebugScene" / "DebugScene.cpp"
RUNTIME_TEST_PATH = Path(__file__).with_name("SystemSchedulerRuntimeTests.cpp")


class SystemSchedulerContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")
        cls.debug_header = DEBUG_SCENE_HEADER.read_text(encoding="utf-8")
        cls.debug_source = DEBUG_SCENE_SOURCE.read_text(encoding="utf-8")

    def test_scheduler_exposes_resource_ownership_and_thread_policy(self) -> None:
        self.assertIn("class SystemScheduler", self.header)
        self.assertIn("enum class SystemAccessType", self.header)
        self.assertIn("ReadWrite", self.header)
        self.assertIn("enum class SystemExecutionPolicy", self.header)
        self.assertIn("MainThread", self.header)
        self.assertIn("Worker", self.header)
        self.assertIn("SystemResourceAccess", self.header)

    def test_scheduler_compiles_raw_war_waw_dependencies(self) -> None:
        self.assertIn("SystemDependencyType::ReadAfterWrite", self.source)
        self.assertIn("SystemDependencyType::WriteAfterRead", self.source)
        self.assertIn("SystemDependencyType::WriteAfterWrite", self.source)
        self.assertIn("compiledPrerequisites", self.source)
        self.assertIn("std::priority_queue", self.source)
        self.assertIn("dependency graph contains a cycle", self.source)

    def test_main_thread_systems_keep_affinity_and_worker_systems_use_job_dependencies(self) -> None:
        self.assertIn("CreateCompletedHandle", self.header)
        self.assertIn("SystemExecutionPolicy::Worker", self.source)
        self.assertIn("DispatchAfter(", self.source)
        self.assertIn("jobSystem->Wait(dependencyJob)", self.source)
        self.assertIn("MainThread system", self.source)

    def test_debug_world_migrates_actor_physics_post_physics_to_scheduler(self) -> None:
        self.assertIn("SystemScheduler worldSystemScheduler_", self.debug_header)
        self.assertIn("SetupWorldSystemSchedule", self.debug_header)
        self.assertIn('"ActorWorld.Update"', self.debug_source)
        self.assertIn('"PhysicsWorld.Update"', self.debug_source)
        self.assertIn('"ActorWorld.PostPhysicsUpdate"', self.debug_source)
        self.assertIn("worldSystemScheduler_.ExecuteAndWait(deltaTime)", self.debug_source)
        self.assertIn("kPhysicsRegistry", self.debug_source)
        self.assertIn("kPhysicsState", self.debug_source)

    def test_runtime_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Build the portable scheduler with the real JobSystem implementation to exercise DAG execution in CI.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase10_scheduler_") as temp_dir:
            executable = Path(temp_dir) / "system_scheduler_tests"
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
                msg=f"C++ scheduler runtime test compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )

            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ scheduler runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("System Scheduler runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
