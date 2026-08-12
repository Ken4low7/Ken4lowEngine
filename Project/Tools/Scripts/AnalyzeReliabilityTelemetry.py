from __future__ import annotations

import argparse
import csv
from dataclasses import asdict, dataclass
import json
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class ReliabilitySummary:
    frames: int
    duration_seconds: float
    p95_frame_ms: float
    p99_frame_ms: float
    memory_growth_mb: float
    memory_slope_mb_per_minute: float
    max_pending_streaming: int
    max_queued_completions: int
    loaded_sublevel_transitions: int


def percentile(values: list[float], percentile_value: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    position = (len(ordered) - 1) * percentile_value
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def linear_slope_per_minute(samples: list[tuple[float, float]]) -> float:
    if len(samples) < 2:
        return 0.0
    mean_x = sum(sample[0] for sample in samples) / len(samples)
    mean_y = sum(sample[1] for sample in samples) / len(samples)
    numerator = sum((x - mean_x) * (y - mean_y) for x, y in samples)
    denominator = sum((x - mean_x) ** 2 for x, _ in samples)
    if denominator <= 0.0:
        return 0.0
    return (numerator / denominator) * 60.0


def load_samples(path: Path) -> list[dict[str, float | int]]:
    required = {
        "frame",
        "elapsed_seconds",
        "frame_time_ms",
        "working_set_mb",
        "pending_streaming",
        "queued_completions",
        "loaded_sublevels",
    }
    samples: list[dict[str, float | int]] = []
    with path.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        missing = required.difference(reader.fieldnames or [])
        if missing:
            raise ValueError(f"Telemetry CSV is missing fields: {', '.join(sorted(missing))}")
        for row in reader:
            samples.append(
                {
                    "frame": int(row["frame"]),
                    "elapsed_seconds": float(row["elapsed_seconds"]),
                    "frame_time_ms": float(row["frame_time_ms"]),
                    "working_set_mb": float(row["working_set_mb"]),
                    "pending_streaming": int(row["pending_streaming"]),
                    "queued_completions": int(row["queued_completions"]),
                    "loaded_sublevels": int(row["loaded_sublevels"]),
                }
            )
    return samples


def summarize(samples: list[dict[str, float | int]]) -> ReliabilitySummary:
    if not samples:
        return ReliabilitySummary(0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0)

    frame_times = [float(sample["frame_time_ms"]) for sample in samples]
    memory_samples = [
        (float(sample["elapsed_seconds"]), float(sample["working_set_mb"])) for sample in samples
    ]
    loaded_values = [int(sample["loaded_sublevels"]) for sample in samples]
    transitions = sum(1 for previous, current in zip(loaded_values, loaded_values[1:]) if previous != current)

    # Memory growth and trend are kept separate so one intentional cache warm-up does not look like a continuing leak.
    return ReliabilitySummary(
        frames=len(samples),
        duration_seconds=max(0.0, float(samples[-1]["elapsed_seconds"]) - float(samples[0]["elapsed_seconds"])),
        p95_frame_ms=percentile(frame_times, 0.95),
        p99_frame_ms=percentile(frame_times, 0.99),
        memory_growth_mb=float(samples[-1]["working_set_mb"]) - float(samples[0]["working_set_mb"]),
        memory_slope_mb_per_minute=linear_slope_per_minute(memory_samples),
        max_pending_streaming=max(int(sample["pending_streaming"]) for sample in samples),
        max_queued_completions=max(int(sample["queued_completions"]) for sample in samples),
        loaded_sublevel_transitions=transitions,
    )


def evaluate(
    summary: ReliabilitySummary,
    budgets: dict[str, float | int],
    *,
    require_soak: bool = False,
    require_streaming: bool = False,
) -> list[str]:
    failures: list[str] = []

    def over(key: str, actual: float | int, label: str) -> None:
        if key in budgets and actual > budgets[key]:
            failures.append(f"{label}: {actual} > {budgets[key]}")

    if summary.frames < int(budgets.get("min_frames", 1)):
        failures.append(f"frames: {summary.frames} < {int(budgets.get('min_frames', 1))}")
    over("max_p95_frame_ms", summary.p95_frame_ms, "p95 frame ms")
    over("max_p99_frame_ms", summary.p99_frame_ms, "p99 frame ms")
    over("max_memory_growth_mb", summary.memory_growth_mb, "memory growth MB")
    over("max_memory_slope_mb_per_minute", summary.memory_slope_mb_per_minute, "memory slope MB/min")
    over("max_pending_streaming", summary.max_pending_streaming, "pending streaming")
    over("max_queued_completions", summary.max_queued_completions, "queued completions")

    if require_soak:
        minimum_seconds = float(budgets.get("soak_min_seconds", 0.0))
        if summary.duration_seconds < minimum_seconds:
            failures.append(f"soak duration: {summary.duration_seconds} < {minimum_seconds}")

    if require_streaming:
        minimum_transitions = int(budgets.get("streaming_min_loaded_transitions", 0))
        if summary.loaded_sublevel_transitions < minimum_transitions:
            failures.append(
                f"loaded sublevel transitions: {summary.loaded_sublevel_transitions} < {minimum_transitions}"
            )

    return failures


def load_budgets(path: Path) -> dict[str, float | int]:
    with path.open("r", encoding="utf-8") as stream:
        data = json.load(stream)
    if not isinstance(data, dict):
        raise ValueError("Reliability budget file must contain a JSON object.")
    return data


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Evaluate Ken4lowEngine reliability telemetry.")
    parser.add_argument("--csv", required=True, type=Path)
    parser.add_argument("--budgets", required=True, type=Path)
    parser.add_argument("--require-soak", action="store_true")
    parser.add_argument("--require-streaming", action="store_true")
    args = parser.parse_args(list(argv) if argv is not None else None)

    summary = summarize(load_samples(args.csv))
    budgets = load_budgets(args.budgets)
    failures = evaluate(
        summary,
        budgets,
        require_soak=args.require_soak,
        require_streaming=args.require_streaming,
    )

    print(json.dumps(asdict(summary), indent=2, sort_keys=True))
    if failures:
        print("Reliability gate FAILED:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("Reliability gate PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
