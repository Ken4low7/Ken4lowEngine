from pathlib import Path
import csv
import sys
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = PROJECT_ROOT / "Tools" / "Scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

import AnalyzeReliabilityTelemetry as analyzer  # noqa: E402


class ReliabilityGateTests(unittest.TestCase):
    def make_samples(
        self,
        *,
        frames: int = 600,
        frame_ms: float = 16.0,
        memory_start: float = 512.0,
        memory_growth: float = 4.0,
        duration_seconds: float = 60.0,
        transition_every: int = 30,
    ) -> list[dict[str, float | int]]:
        samples: list[dict[str, float | int]] = []
        for index in range(frames):
            ratio = index / max(frames - 1, 1)
            samples.append(
                {
                    "frame": index,
                    "elapsed_seconds": ratio * duration_seconds,
                    "frame_time_ms": frame_ms,
                    "working_set_mb": memory_start + ratio * memory_growth,
                    "pending_streaming": 4,
                    "queued_completions": 2,
                    "loaded_sublevels": (index // transition_every) % 3,
                }
            )
        return samples

    def test_healthy_capture_passes_performance_and_memory_budgets(self) -> None:
        summary = analyzer.summarize(self.make_samples())
        budgets = {
            "min_frames": 300,
            "max_p95_frame_ms": 33.333,
            "max_p99_frame_ms": 50.0,
            "max_memory_growth_mb": 64.0,
            "max_memory_slope_mb_per_minute": 8.0,
            "max_pending_streaming": 256,
            "max_queued_completions": 256,
        }
        self.assertEqual(analyzer.evaluate(summary, budgets), [])

    def test_frame_budget_regression_fails(self) -> None:
        summary = analyzer.summarize(self.make_samples(frame_ms=55.0))
        failures = analyzer.evaluate(summary, {"min_frames": 1, "max_p95_frame_ms": 33.333})
        self.assertTrue(any("p95 frame ms" in failure for failure in failures))

    def test_memory_leak_trend_fails_even_when_process_stays_alive(self) -> None:
        summary = analyzer.summarize(
            self.make_samples(memory_growth=120.0, duration_seconds=60.0)
        )
        failures = analyzer.evaluate(
            summary,
            {
                "min_frames": 1,
                "max_memory_growth_mb": 64.0,
                "max_memory_slope_mb_per_minute": 8.0,
            },
        )
        self.assertTrue(any("memory growth MB" in failure for failure in failures))
        self.assertTrue(any("memory slope MB/min" in failure for failure in failures))

    def test_soak_and_streaming_requirements_are_explicit_release_gates(self) -> None:
        summary = analyzer.summarize(self.make_samples(duration_seconds=20.0, transition_every=1000))
        failures = analyzer.evaluate(
            summary,
            {
                "min_frames": 1,
                "soak_min_seconds": 30.0,
                "streaming_min_loaded_transitions": 8,
            },
            require_soak=True,
            require_streaming=True,
        )
        self.assertTrue(any("soak duration" in failure for failure in failures))
        self.assertTrue(any("loaded sublevel transitions" in failure for failure in failures))

    def test_csv_loader_rejects_incomplete_telemetry(self) -> None:
        # A malformed capture must fail closed instead of silently skipping a release-gate metric.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase12_gate_") as temp_dir:
            path = Path(temp_dir) / "bad.csv"
            with path.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.writer(stream)
                writer.writerow(["frame", "elapsed_seconds"])
                writer.writerow([0, 0.0])
            with self.assertRaises(ValueError):
                analyzer.load_samples(path)


if __name__ == "__main__":
    unittest.main()
