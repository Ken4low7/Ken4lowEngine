from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
RENDERER_DIR = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "GpuParticle" / "Renderer"
ESTIMATOR_HEADER = RENDERER_DIR / "GpuParticleWorkloadEstimator.h"
RUNTIME_TEST = Path(__file__).with_name("GpuParticleWorkloadEstimatorRuntimeTests.cpp")
RENDERER_HEADER = RENDERER_DIR / "GpuParticleRenderer.h"
RENDERER_SOURCE = RENDERER_DIR / "GpuParticleRenderer.cpp"


class GpuParticleWorkloadStressTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.estimator = ESTIMATOR_HEADER.read_text(encoding="utf-8")
        cls.renderer_h = RENDERER_HEADER.read_text(encoding="utf-8")
        cls.renderer_cpp = RENDERER_SOURCE.read_text(encoding="utf-8")

    def test_worst_case_command_count_remains_bounded_by_render_groups(self) -> None:
        # 10000 random workloads verify the same monotonic bounds used by the runtime diagnostics.
        rng = random.Random(0x4B344C57)
        max_particles = 131072
        bitonic_passes = 153
        for _ in range(10000):
            render_groups = rng.randint(0, 128)
            alpha_groups = rng.randint(0, 160)
            clamped_alpha = min(render_groups, alpha_groups)

            particle_scans = max_particles * render_groups
            compaction_thread_groups = 512 * render_groups
            alpha_sort_dispatches = bitonic_passes * clamped_alpha

            self.assertLessEqual(particle_scans, max_particles * 128)
            self.assertLessEqual(compaction_thread_groups, 512 * 128)
            self.assertLessEqual(alpha_sort_dispatches, bitonic_passes * render_groups)

    def test_renderer_exposes_no_readback_workload_statistics(self) -> None:
        for field in (
            "drawRequests",
            "compactionDispatches",
            "compactionParticleScans",
            "alphaSortGroups",
            "alphaSortDispatches",
            "indirectDraws",
        ):
            self.assertIn(field, self.renderer_h)
            self.assertIn(f"gpuDrivenStatistics_.{field}", self.renderer_cpp)
        self.assertIn("ResetGpuDrivenStatistics", self.renderer_h)
        self.assertIn("GetGpuDrivenStatistics", self.renderer_h)

    def test_estimator_runtime_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Header-only estimator is exercised outside D3D12 so Linux validation can still catch workload math regressions.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase14_workload_") as temp_dir:
            executable = Path(temp_dir) / "gpu_particle_workload_tests"
            compile_result = subprocess.run(
                [
                    compiler,
                    "-std=c++20",
                    str(RUNTIME_TEST),
                    "-I",
                    str(RENDERER_DIR),
                    "-o",
                    str(executable),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ GPU particle workload compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )
            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ GPU particle workload test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("GPU particle workload estimator runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
