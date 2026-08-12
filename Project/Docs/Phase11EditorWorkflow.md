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

### 11.2 Prefab Diff — planned

Add a deterministic comparison layer between a prefab source and an instance/override state. The Editor should be able to show added, removed, and modified properties/components without rewriting unrelated serialized data.

### 11.3 World Partition Editor — planned

Build tooling around world-cell ownership, visibility/loading state, and spatial editing. Runtime streaming decisions and editor visualization should share stable cell identity rather than maintaining separate spatial truth.

### 11.4 Asset Graph — planned

Visualize asset dependencies and reverse dependencies from the Phase 8 content pipeline. The graph should help answer why an asset rebuilds, what will be invalidated, and which runtime packages/chunks reference it.

### 11.5 Profiler UI — planned

Expose the existing frame, render, job/system, descriptor/cache, and world diagnostics in a unified editor-facing profiler. Phase 11 should consume existing instrumentation rather than creating duplicate counters.

## Compatibility strategy

Editor workflow changes remain additive. Existing command producers continue working, Play-in-Editor keeps its current world isolation rules, and no editor command may silently mutate runtime-only state while replaying.

Structural edits that recreate Components still need special care because older commands can contain raw Component references. Phase 11.1 does not pretend that lifetime problem is solved by transactions; later hardening should migrate those paths toward stable editor object identity or serialized targets before removing their conservative history invalidation.

## Boundary with Phase 12

Phase 11 owns authoring workflow and editor diagnostics. Phase 12 remains responsible for production readiness: crash dumps, deterministic replay, compatibility/release validation, soak/performance testing, and shipping checks.
