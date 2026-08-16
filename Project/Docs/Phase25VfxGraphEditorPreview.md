# Phase25 — VFX Graph Editor / Preview

## Goal

Phase25 turns the Phase20-24 VFX Graph authoring data into an engine editor workflow without creating a second particle runtime. The editor edits `VfxGraphDesc`, validates with `VfxGraphCompiler`, saves through `VfxGraphSerializer`, and previews through `VfxGraphRuntime` / the existing GPU Particle backend.

## Architecture

```text
VFX Graph Editor (ImGui)
        |
        +-- edit VfxGraphDesc / editorPosition / edges
        +-- Load / Save -> VfxGraphSerializer
        +-- Live Compile -> VfxGraphCompiler
        +-- Diagnostics -> errors / warnings / executionOrder
        +-- Preview -> VfxGraphRuntime::RegisterGraph / PlayLoop
                              |
                              v
                    existing GPU Particle Runtime
```

The editor owns authoring state only. It does not own GPU particle buffers, compute pipelines, collision events, ribbon history, mesh instances, or Phase24 execution scheduling.

## 25.1 Editor window and lifecycle

- `VfxGraphEditor` is an engine singleton, matching the existing VFX Timeline editor style.
- It is initialized/finalized from `GameApplication`.
- `EditorWindowState::showVfxGraphEditor` controls visibility.
- `Window > Rendering > VFX Graph Editor` can reopen the window.

## 25.2 Asset workflow

The toolbar supports:

- editable `.vfxgraph.json` path
- Load
- Save
- explicit Compile
- Live Compile toggle
- Live Preview toggle

No editor-only graph file format is introduced. Existing `schemaVersion = 1` files remain the source of truth.

## 25.3 Graph canvas

The canvas uses ImGui `ImDrawList` rather than adding an external node-editor dependency.

- persistent `editorPosition`
- draggable node cards
- Bezier edge visualization
- middle-mouse pan
- Ctrl + wheel zoom
- Frame All reset
- selected-node highlighting
- graph edge endpoints

The existing `VfxGraphEdgeDesc` remains the execution dependency representation.

## 25.4 Emitter and node authoring

The editor can:

- add/remove emitters
- edit emitter name, max particle count, loop and duration
- add all Phase20-23 node types
- remove nodes and connected edges safely
- start/cancel a connection and connect it to another node
- edit node name, enabled state, and editor position

New nodes use `GetExpectedVfxGraphNodeStage()` and the existing payload type for that module.

## 25.5 Module inspector

The Phase25 inspector covers all 22 graph node types available through Phase23, including:

- spawn rate / burst / spawn shapes
- lifetime / velocity / color / size
- gravity / drag
- rotation modules
- size curve / color gradient
- collision / death event / sub emitter
- sprite / ribbon / trail / mesh renderers

Curve and gradient key counts continue to respect `VfxGraphDesc::kMaxCurveKeys` and `kMaxGradientKeys`.

## 25.6 Live compiler diagnostics

Every authoring change can be sent through the existing compiler. The editor displays:

- compile success/failure
- warnings
- errors
- compiled emitter execution order

This intentionally avoids a separate editor validator that could disagree with runtime compilation.

## 25.7 Live preview

Preview uses the production runtime:

1. compile editable graph
2. register graph in `VfxGraphRuntime`
3. `PlayLoop()` at the preview position
4. while authoring, successful live compilation can restart the preview
5. moving the preview position calls `SetLoopPosition()`
6. Stop calls `StopLoop()`

The preview therefore exercises the same Phase20-24 GPU particle implementation used by the game.

## 25.8 Sample

`Resources/VfxGraph/Phase25/EditorPreviewShowcase.vfxgraph.json` is the default Phase25 editor graph. It demonstrates connected Spawn / Initialize / Update / Render modules, a size curve, color gradient, and sprite rendering.

## 25.9 Validation

`Tests/Phase25/test_vfx_graph_editor_preview.py` locks the following contracts:

- existing serializer/compiler/runtime reuse
- all 22 node types available to authoring
- persistent position + edge canvas behavior
- node/edge mutation
- curve/gradient authoring limits
- runtime-backed live preview
- compiler diagnostics
- application/editor lifecycle integration
- valid connected showcase asset
- Phase26 fluid/light/post-effect scope is not pulled into Phase25

## Phase boundary

Phase25 intentionally does **not** implement Fluid / Light / PostEffect integration. Those systems remain Phase26. It also does not replace the Phase24 GPU Execution Graph or create a second particle backend.

## Completion checklist

- [x] 25.1 VFX Graph Editor window
- [x] 25.2 Load / Save workflow
- [x] 25.3 Node canvas with pan / zoom / drag
- [x] 25.4 Edge visualization and connection editing
- [x] 25.5 Emitter authoring
- [x] 25.6 All Phase20-23 module inspectors
- [x] 25.7 Curve / Gradient editor
- [x] 25.8 Live compiler diagnostics
- [x] 25.9 Runtime-backed preview controls
- [x] 25.10 Sample / static tests / CI
