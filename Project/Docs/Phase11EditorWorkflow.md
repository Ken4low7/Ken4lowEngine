# Phase 11 — Editor Workflow

## Goal

Phase 11 turns the existing runtime-oriented editor into a safer production workflow. The focus is not adding more rendering features; it is making edits reversible, inspectable, scalable across large worlds, and easier to diagnose.

## Planned steps

### 11.1 Undo / Redo — implemented transaction foundation

The engine already had `EditorCommandHistory`, value/state/lambda commands, Ctrl+Z / Ctrl+Y shortcuts, transform-gizmo history, and structural Actor/Component commands. Phase 11 therefore hardens that existing path instead of creating a second undo system.

`EditorCommandHistory` now adds explicit transactions:

- `BeginTransaction(name)` opens one editor operation group
- `Execute` and `PushExecuted` append commands to the active transaction after the visible edit succeeds
- `CommitTransaction()` stores the group as one `EditorCompositeCommand`
- `CancelTransaction()` undoes the pending commands in reverse execution order and stores no history entry
- Undo/Redo are disabled while a transaction is open, preventing the history cursor from crossing partially recorded edits

`EditorCompositeCommand` replays child commands in forward order and undoes them in reverse order. This gives multi-property inspector edits, future prefab operations, and other batch changes a single user-facing Undo entry without hiding the actual command boundaries.

Replay state is now exception-safe. A small RAII replay scope guarantees that `IsReplaying()` is reset even if a command throws. Undo also restores its previous cursor when the command fails, so a failed Undo remains available instead of leaving the history cursor ahead of the actual world state. Redo advances the cursor only after successful execution.

The history exposes additional read-only diagnostics (`GetHistorySize`, capacity, active transaction name/count) so a later Editor diagnostics/history window can display the real command state without duplicating history bookkeeping.

#### Validation

`Tests/Phase11/test_editor_command_history.py` protects the source contract and builds a portable C++20 runtime test when a compiler is available. Runtime coverage includes:

- basic Undo/Redo and Redo-branch truncation
- transaction commit as one history entry
- transaction cancel restoring state in reverse order
- capacity trimming
- Undo exception recovery of cursor/replay state
- Redo exception recovery without losing the Redo entry

The existing Editor shortcuts and command producers remain source-compatible; callers can migrate to transactions only where an operation truly contains multiple sub-edits.

### 11.2 Prefab Diff — implemented semantic diff foundation

`EditorPrefabDiff` provides a deterministic, read-only comparison between a prefab base Actor JSON and the live serialized state of one prefab instance. The diff model is separate from `PrefabReferenceResolver`'s RFC 7396 persistence layer so the Editor can explain changes without mutating either source document or inventing a second runtime prefab representation.

The semantic diff classifies changes into:

- Actor property changes outside the `Components` array
- Component additions
- Component removals
- Component property changes

Components are matched by serialized component name instead of array position. This prevents harmless component-order changes from appearing as prefab overrides. Components without a name fall back to class + deterministic occurrence identity. Replacing the class of a same-name component is represented as Remove + Add rather than as a writable property change.

Nested object properties are compared recursively and report dot-separated property paths such as `Settings.CastShadow`. Arrays and scalar values remain atomic values, matching the current serialized property model. Each entry records whether the value exists on the base and/or instance side, so a removed property is distinguishable from a property whose explicit JSON value is `null`.

The Actor Details inspector now hosts a `Prefab Diff` section for actors tracked by `PrefabInstanceRegistry`. The panel loads the current prefab source through `PrefabReferenceResolver::LoadBaseActor`, serializes the selected live Actor, computes the semantic diff, and displays:

- source prefab path
- total/Actor/Added/Removed/Modified counts
- each semantic diff entry
- Before / After JSON previews

The panel snapshots on selection change and provides `Refresh Prefab Diff` for explicit recomputation after edits. This keeps file I/O and Actor serialization out of every editor frame while still making the current override state inspectable.

The panel is intentionally read-only in 11.2. Existing Level save/load compatibility remains unchanged: `LevelSerializer` still persists prefab overrides through the established JSON Merge Patch contract. A later prefab-apply/revert workflow can consume the semantic entries and Phase 11.1 transactions without changing how old Level files resolve.

#### Validation

`Tests/Phase11/test_prefab_diff.py` protects the source and inspector integration contract. When a portable C++20 compiler is available it builds `EditorPrefabDiffRuntimeTests.cpp` directly against the header-only diff model.

Runtime coverage includes:

- Actor property, Component add/remove, and Component property classification
- component-order changes producing no false diff
- nested property removal with explicit existence tracking
- same-name Component class replacement becoming Remove + Add
- deterministic semantic matching independent of serialized array position

### 11.3 World Partition Editor — implemented shared-grid foundation

Phase 11.3 adds editor tooling around the existing `WorldPartitionManager` and `SubLevelManager` instead of introducing a separate editor-only spatial model.

`WorldPartitionGrid` is now the shared pure calculation layer for both runtime streaming and editor diagnostics. It owns:

- world X/Z to stable integer cell conversion
- negative-coordinate floor behavior
- sanitized cell size and load/unload radii
- Chebyshev cell distance
- Load / Retain / Unload hysteresis decisions
- Always Loaded classification

`WorldPartitionManager::Update` now uses that same grid helper and stores the latest streaming-source world position and source cell. This means the editor displays the exact cell identity used by runtime residency decisions rather than recomputing a parallel approximation.

The manager also exposes non-destructive editor update paths:

- `ApplyEditorSettings` updates Enabled, Cell Size, and Load/Unload Radius values, sanitizes them, and immediately re-evaluates the current streaming source
- `UpdateSubLevelEditorMetadata` updates Cell X/Z, Priority, and Always Loaded metadata
- `SubLevelManager::UpdateReferenceMetadata` changes only reference metadata and preserves current load state, in-flight request generation, and streamed Actor handles

The `World Partition` inspector section shows:

- current streaming-source position and source cell
- loaded SubLevel count
- current Load / Retain / Unload radius policy
- each SubLevel's Cell X/Z and Chebyshev distance
- real `SubLevelState` (`Unloaded`, `Loading`, `Loaded`, `Failed`)
- the residency decision produced by `WorldPartitionGrid`
- editable Cell X/Z, Priority, and Always Loaded values
- manual Load / Unload / Retry controls for diagnostics
- streaming errors from `SubLevelManager`

Persistent metadata edits mark the Level dirty, so the existing `LevelSerializer::CaptureWorld` path saves the manager's updated settings and SubLevel metadata. Manual Load/Unload buttons are explicitly diagnostic and are not serialized; automatic streaming may override them on the next residency evaluation.

#### Validation

`Tests/Phase11/test_world_partition_editor.py` checks that runtime and editor use the same grid helper, editor metadata changes do not reset `SubLevelManager`, and the panel reads real runtime state instead of maintaining a duplicate load-state cache.

When a portable C++20 compiler is available, `WorldPartitionGridRuntimeTests.cpp` verifies:

- positive and negative cell boundaries
- invalid/zero cell size sanitization
- non-finite world-coordinate handling
- Chebyshev distance
- Always Loaded / Load / Retain / Unload decisions
- radius sanitization and hysteresis behavior

### 11.4 Asset Graph — implemented Phase 8 manifest integration

`EditorAssetGraph` consumes the deterministic Phase 8 `Generated/AssetPipeline/AssetManifest.json` instead of rescanning source files and rebuilding dependency truth inside the editor. When available, it also consumes `Generated/Packages/PackageManifest.json` so rebuild impact can be connected to runtime chunk ownership.

The graph indexes each cook asset by:

- AssetId
- LogicalKey/source path, including variant-aware source matching
- build metadata path
- cooked output paths
- dependency paths
- optional runtime chunk id from `AssetToChunk`

From those indexes the editor can answer both directions:

- which source/dependency paths feed the selected cook asset
- which cook assets directly depend on the selected source or cooked output
- which downstream assets are transitively invalidated when produced outputs feed later assets
- which runtime chunks contain the affected assets when a PackageManifest exists

The transitive impact walk follows produced output paths into the reverse dependency index. It does not infer dependencies from file names or directory proximity, so the editor stays aligned with the same graph used by incremental build/package tooling.

Content Browser selection now opens an `Asset Graph` diagnostic window for non-folder assets. The panel loads the manifest lazily on the first selection and exposes an explicit `Reload Manifest` action, keeping manifest JSON I/O out of every editor frame. It shows:

- selected source/resource path
- matched cook asset records
- asset type and logical key
- package chunk when available
- missing-output diagnostics
- direct dependency paths
- direct dependent cook assets
- transitive rebuild impact
- affected runtime chunks

Files that are valid runtime/editor resources but are not part of the Phase 8 cook manifest are reported as unregistered rather than being assigned invented graph edges. A missing PackageManifest only disables chunk impact; dependency inspection continues to work from the AssetManifest alone.

#### Validation

`Tests/Phase11/test_asset_graph.py` protects the manifest/package contract and Content Browser integration. When a portable C++20 compiler is available it builds `EditorAssetGraphRuntimeTests.cpp` directly against the header-only graph model.

Runtime coverage includes:

- source path and Windows-style slash normalization
- variant LogicalKey selection
- source -> cook asset -> cooked output transitive invalidation
- direct dependent discovery
- cooked output producer inspection without incorrectly rebuilding the producer
- unrelated asset isolation
- optional `AssetToChunk` impact aggregation
- unregistered resource paths producing no false relation

### 11.5 Profiler UI — planned

Expose the existing frame, render, job/system, descriptor/cache, and world diagnostics in a unified editor-facing profiler. Phase 11 should consume existing instrumentation rather than creating duplicate counters.

## Compatibility strategy

Editor workflow changes remain additive. Existing command producers continue working, Play-in-Editor keeps its current world isolation rules, and no editor command may silently mutate runtime-only state while replaying.

Structural edits that recreate Components still need special care because older commands can contain raw Component references. Phase 11.1 does not pretend that lifetime problem is solved by transactions; later hardening should migrate those paths toward stable editor object identity or serialized targets before removing their conservative history invalidation.

Prefab Diff is diagnostic in 11.2: it does not silently apply, revert, or rewrite instance data. Existing RFC 7396 Level override serialization remains the compatibility source of truth while the semantic layer explains the change at Actor/Component/property granularity.

World Partition editing in 11.3 reuses the runtime manager state and Level capture path. It does not create editor-only cell ownership or a second load-state machine. Path/Id authoring is intentionally left unchanged; the editor foundation only changes safe cell/priority/always-loaded metadata while preserving live SubLevel state.

Asset Graph in 11.4 is read-only and derives its state from generated Phase 8 manifests. It does not rewrite build metadata, package assignments, or source dependencies, and it reports missing/stale generated manifests rather than silently synthesizing replacement dependency data.

## Boundary with Phase 12

Phase 11 owns authoring workflow and editor diagnostics. Phase 12 remains responsible for production readiness: crash dumps, deterministic replay, compatibility/release validation, soak/performance testing, and shipping checks.
