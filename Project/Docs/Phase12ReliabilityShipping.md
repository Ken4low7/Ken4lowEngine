# Phase 12 — Reliability / Shipping

## Goal

Phase 12 turns the engine from a development build that is convenient to debug into a build that can be diagnosed after failure, regression-tested across save versions, stressed for long sessions, measured against explicit budgets, and packaged with a reproducible shipping manifest.

The implementation deliberately reuses existing runtime systems instead of creating test-only copies of Level migration, World Partition state, frame timing, or allocation tracking.

## 12.1 Crash Dump

`CrashReporter` is installed at the beginning of `WinMain`, before `GameApplication` is constructed. Unhandled Windows exceptions write a timestamped `.dmp` under `CrashReports/` through `MiniDumpWriteDump`.

The dump includes thread information, unloaded modules, handle data, and data segments. Standard C++ exceptions that escape the engine loop are caught at `WinMain` and receive a manual diagnostic/stack report before the process exits with an error code.

## 12.2 Symbol / Stack Trace

The same `CrashReporter` captures up to 64 stack frames and resolves addresses through DbgHelp (`SymInitialize` / `SymFromAddr`). Each crash or manual report writes a sibling `.stack.txt` file.

Shipping symbols are not placed in the public release archive. `PackageRelease.ps1` creates a separate symbol archive from generated PDB files so production dumps can still be symbolized without distributing debug symbols with the game package.

## 12.3 Engine Diagnostic Report

`EngineDiagnosticReport` writes portable crash context next to the dump:

- build profile
- process and thread IDs
- local timestamp
- hardware thread count
- physical memory pressure and available memory
- process command line
- current working directory
- caller-supplied failure reason

This report is also usable for manual reliability captures; it is not tied only to SEH crashes.

## 12.4 Deterministic Replay

`DeterministicReplay` defines a versioned `K4REPLAY` recording format. Every recorded simulation frame owns:

- contiguous frame index
- fixed delta time in microseconds
- RNG state
- state hash
- opaque input payload bytes

Playback validates the file magic, format version, payload safety limit, contiguous frame order, and expected simulation-frame index before exposing input data. The model intentionally does not decide how gameplay input or world state is serialized; systems can provide their own payload and deterministic state hash while using one shared recording contract.

`Tests/Phase12/DeterministicReplayRuntimeTests.cpp` performs actual save/load/playback checks with a portable C++20 compiler in CI.

## 12.5 Save Compatibility Tests

`SaveCompatibility` inspects a `Ken4lowLevel` document without maintaining a second compatibility table. It asks the production `LevelVersionMigration::MigrateToCurrent` path whether a copy can reach the current schema.

A save reports:

- source version
- current target version
- whether it is compatible
- whether migration is required
- failure reason when it cannot be loaded

Future-version saves remain rejected instead of silently dropping unknown data.

## 12.6 Level Migration Tests

The existing migration chain remains the source of truth:

- Version 1 → Version 2
- Version 2 → Version 3

Phase 12 compiles that real migration source in `SaveCompatibilityRuntimeTests.cpp` and verifies that Version 1 and Version 2 documents reach `LevelDocument::kCurrentVersion`, while current saves remain unchanged and future versions fail closed.

When the Level schema is incremented later, CI should fail until the new migration edge and corresponding compatibility fixture are added.

## 12.7 Streaming Stress Test

Reliability telemetry can be enabled with:

- `KEN4LOW_STREAMING_STRESS=1`

When active and World Partition is configured, the reliability path applies a deterministic nine-point route spanning positive and negative grid cells. Normal camera-driven residency evaluation still occurs, so the synthetic source adds deliberate churn rather than replacing the real runtime path.

The portable `StreamingStressRuntimeTests.cpp` also executes hundreds of thousands of shared `WorldPartitionGrid` evaluations and exercises Load, Retain hysteresis, Unload, Always Loaded, and negative-coordinate behavior without requiring DirectX.

## 12.8 Memory Leak Test

`ReliabilityTelemetry` records process Working Set and the existing per-frame allocation byte count into CSV. `AnalyzeReliabilityTelemetry.py` evaluates both:

- total memory growth over the capture
- least-squares memory trend in MB/minute

Keeping both metrics avoids classifying one intentional cache warm-up as a continuing leak while still detecting gradual growth across a long run.

The current default gate in `Config/ReliabilityBudgets.json` allows at most 64 MB total growth and 8 MB/minute positive trend. These values are initial engineering budgets and should be tightened after representative content is profiled on target hardware.

## 12.9 Performance Budget

The same CSV records frame time, pending streaming requests, queued completions, and loaded SubLevel count. The analyzer calculates p95 and p99 frame time rather than relying on average FPS.

Initial budgets are:

- p95 frame time: 33.333 ms
- p99 frame time: 50 ms
- pending streaming requests: 256
- queued streaming completions: 256
- minimum capture: 300 frames

Budget evaluation exits non-zero, making it usable as a local release gate or CI step when a runnable packaged build is available.

## 12.10 Release Packaging

`Tools/Scripts/PackageRelease.ps1` stages:

- the selected executable
- `Resources/`
- runtime DLLs located beside the executable
- `PackageManifest.json`

The manifest records package format/version, Build Profile, UTC creation time, relative path, size, and SHA-256 for every staged payload file. The public package is zipped separately from PDB symbols.

The script exposes `-DryRun`, which CI uses to validate packaging inputs and PowerShell syntax even though the hosted runner currently performs compile-only validation and cannot produce the repository's complete linked Debug binary.

Example:

```powershell
pwsh Project/Tools/Scripts/PackageRelease.ps1 -BuildProfile Shipping
```

## 12.11 Build Profile

`BuildProfile.h` provides one compile-time classification shared by diagnostics and release tooling:

- `_DEBUG` → Debug
- neither debug nor release macro → Development
- `NDEBUG` → Shipping

The current Visual Studio Release configuration defines `NDEBUG`, so production diagnostics report `Shipping`. A future dedicated Development configuration can reuse the same enum without changing crash-report format.

## 12.12 Soak Test

`Tools/Scripts/RunSoakTest.ps1` launches the actual executable with reliability environment variables, waits for the configured duration, and then runs the same telemetry analyzer used by shorter performance/memory captures.

Example 30-minute soak with streaming churn:

```powershell
pwsh Project/Tools/Scripts/RunSoakTest.ps1 `
  -ExecutablePath .\Generated\outputs\x64\Release\Ken4lowEngine.exe `
  -DurationSeconds 1800 `
  -StreamingStress
```

The runtime samples telemetry once per completed frame through `FrameAllocationTracker::EndFrame`. This path remains active in Release even though CRT allocation counting itself is disabled there. When the requested soak duration expires, the main-thread reliability hook posts a normal quit message so shutdown still runs through the engine's existing `Finalize` sequence.

The default release gate requires at least 1800 seconds when `--require-soak` is requested and at least eight loaded-SubLevel transitions when `--require-streaming` is requested.

## Reliability CSV

Set `KEN4LOW_RELIABILITY_CSV` to enable capture. The stable columns are:

```text
frame,elapsed_seconds,frame_time_ms,working_set_mb,frame_allocated_bytes,pending_streaming,queued_completions,loaded_sublevels
```

A standalone captured CSV can be evaluated with:

```bash
python Project/Tools/Scripts/AnalyzeReliabilityTelemetry.py \
  --csv Generated/Reliability/soak.csv \
  --budgets Project/Config/ReliabilityBudgets.json \
  --require-soak \
  --require-streaming
```

## CI validation

Phase 12 adds `Tests/Phase12` to the existing project-validation job. Portable runtime tests exercise Deterministic Replay, save migration, and World Partition stress logic. Python tests protect crash-report, telemetry, memory/performance budget, packaging, build-profile, and soak contracts.

Windows CI also parses the new PowerShell scripts and executes release packaging in `-DryRun` mode before compiling every C++ translation unit in both Debug and Release configurations.

A hosted compile job is not a substitute for a real long-running soak on a representative machine. The automation is therefore split intentionally: CI proves the harness and source contracts on every commit; a release candidate must still pass the requested real runtime duration and budget file before shipping.

## Phase 12 completion criteria

Phase 12 is complete when all of the following remain true:

1. an unhandled production crash leaves a dump, stack report, and diagnostic report;
2. historical supported saves migrate through the production migration chain and future schemas fail closed;
3. deterministic replay files reject malformed or out-of-order data;
4. runtime reliability capture can stress streaming and expose memory/frame/queue trends;
5. performance, memory, streaming, and soak thresholds are machine-readable release gates;
6. a Shipping package is hash-manifested and symbols are archived separately;
7. Phase 12 tests and Debug/Release translation-unit compilation pass in CI;
8. the actual release candidate completes its configured soak test without exceeding the selected budgets.
