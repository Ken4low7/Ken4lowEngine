# Phase 14 - Windows / DX12 Acceptance

## Purpose

This checklist is the final machine-dependent gate for Phase 14 after automated tests, DXC shader validation, and Debug/Release translation-unit compilation are green.

The goal is not to invent a universal GPU budget. The goal is to verify correctness on a real Windows/DX12 device and capture a repeatable baseline for the actual target hardware.

## Preconditions

- Build the current `feature/phase14-gpu-driven-particle-rendering` revision in Debug and Release.
- Enable the D3D12 debug layer for the Debug run.
- Use a fixed resolution, VSync state, and scene/effect setup for every comparison.
- Record GPU model, driver version, resolution, build configuration, and commit SHA before collecting timings.

## Visual Smoke Matrix

| Case | Expected result | Result |
| --- | --- | --- |
| Additive sprite effect | Visible particles render with no ordering regression or dead-particle flashes. | Pending |
| Alpha sprite overlap | Transparent particles remain visually back-to-front while the camera moves through the effect. | Pending |
| Multiply sprite effect | Effect renders without unnecessary alpha-sort artifacts. | Pending |
| Mesh particle effect | Mesh instances use the compacted visible-index path and preserve transforms/textures. | Pending |
| Multiple render groups | Different texture/material/blend groups do not leak particles into one another. | Pending |
| Burst to zero | After particle lifetime expires, no stale instances remain visible. | Pending |
| Editor/Game transition | Starting/stopping preview or gameplay does not corrupt particle state. | Pending |
| Resize / viewport change | Particle output remains stable after window or editor viewport resize. | Pending |

## Stress Pass

Run at least three representative loads:

1. Sparse: a small number of visible particles with several mostly-empty render groups.
2. Dense additive: a high visible-particle count without alpha sorting.
3. Dense alpha: a high visible-particle count with overlapping transparent particles.

For each load, keep the camera path and run duration consistent. A 60-second capture after a short warm-up is a reasonable first baseline; longer captures can be used if the workload is bursty.

Record the exposed Phase 14 metrics:

- GPU compaction last / EMA / max ms
- GPU alpha-sort last / EMA / max ms
- GPU graphics last / EMA / max ms
- GPU total last / EMA / max ms
- draw requests
- compaction dispatches
- scanned particle slots
- alpha-sort groups
- alpha-sort dispatches
- indirect draws

## Correctness / Stability Gate

Phase 14 passes the machine-dependent gate when all of the following are true:

- no D3D12 debug-layer errors or resource-state warnings are produced by the particle path;
- Sprite and Mesh particles render through `ExecuteIndirect` without visible stale/dead instances;
- normal alpha blending is visually back-to-front under camera motion;
- additive and multiply effects remain stable without alpha-sort regressions;
- repeated effect start/stop and scene/editor transitions do not produce descriptor, fence, or resource-state corruption;
- stress runs complete without device removal, crash, or unbounded frame-time growth;
- the captured metrics are saved as the baseline for the tested GPU instead of being compared against an invented universal threshold.

## Baseline Record Template

```text
Date:
Commit:
Configuration: Debug / Release
GPU:
Driver:
Resolution:
VSync:
Scene / Effect:
Duration:

Compaction ms (last / EMA / max):
Alpha Sort ms (last / EMA / max):
Graphics ms (last / EMA / max):
Total ms (last / EMA / max):

Draw Requests:
Compaction Dispatches:
Particle Slots Scanned:
Alpha Sort Groups:
Alpha Sort Dispatches:
Indirect Draws:

D3D12 Debug Errors:
Visual Issues:
Notes:
```

## Current Automated Gate

GitHub Actions run #449 for commit `82db34fe7544e221a5c7a1984c1254c2c26f5d38` completed successfully with:

- Project Validation and Tests: success
- GPU Particle HLSL DXC validation: success
- Compile Debug: success
- Compile Release: success

The remaining gate is this real Windows/DX12 runtime acceptance pass.
