from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def test_editor_uses_existing_graph_asset_compiler_and_runtime():
    source = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    assert "VfxGraphSerializer::Load" in source
    assert "VfxGraphSerializer::Save" in source
    assert "VfxGraphCompiler::Compile" in source
    assert "VfxGraphRuntime::GetInstance()->RegisterGraph" in source


def test_editor_exposes_all_phase20_to_phase23_graph_node_types():
    source = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    expected = [
        "SpawnRate", "Burst", "SpawnPoint", "SpawnSphere", "SpawnBox",
        "Lifetime", "InitialVelocity", "InitialColor", "InitialSize", "Gravity", "Drag",
        "InitialRotation", "RotationRate", "SizeOverLife", "ColorOverLife",
        "Collision", "DeathEvent", "SubEmitter",
        "SpriteRenderer", "RibbonRenderer", "TrailRenderer", "MeshRenderer",
    ]
    for name in expected:
        assert f"VfxGraphNodeType::{name}" in source
    assert "MakeDefaultPayload" in source
    assert "GetExpectedVfxGraphNodeStage(type)" in source


def test_canvas_persists_positions_and_visualizes_edges_without_external_node_library():
    source = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    assert "node.editorPosition" in source
    assert "AddBezierCubic" in source
    assert "ImGuiMouseButton_Middle" in source
    assert "canvasZoom_" in source
    assert "io.MouseWheel" in source
    assert "imnodes" not in source.lower()
    assert "NodeEditor" not in source


def test_editor_can_add_remove_and_connect_graph_nodes():
    header = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.h")
    source = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    for symbol in ("AddNode", "RemoveSelectedNode", "AddEdge", "RemoveEdge", "AllocateNodeId"):
        assert symbol in header
        assert f"VfxGraphEditor::{symbol}" in source
    assert "emitter->edges.push_back" in source
    assert "edge.fromNodeId == removedId || edge.toNodeId == removedId" in source


def test_curve_and_gradient_authoring_respects_existing_schema_limits():
    source = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    assert "DrawFloatCurveEditor" in source
    assert "DrawColorGradientEditor" in source
    assert "VfxGraphDesc::kMaxCurveKeys" in source
    assert "VfxGraphDesc::kMaxGradientKeys" in source
    assert "std::sort(curve.keys.begin()" in source
    assert "std::sort(gradient.keys.begin()" in source


def test_live_preview_reuses_registered_gpu_particle_graph_runtime():
    source = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    assert "PlayLoop(editableGraph_.graphName, previewPosition_)" in source
    assert "StopLoop(previewHandle_)" in source
    assert "SetLoopPosition(previewHandle_, previewPosition_)" in source
    assert "livePreview_ && previewHandle_.IsValid()" in source
    assert "ID3D12Resource" not in source
    assert "CreateCommittedResource" not in source


def test_compile_diagnostics_surface_errors_warnings_and_execution_order():
    source = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    assert "compileResult_.errors" in source
    assert "compileResult_.warnings" in source
    assert "compiledEmitter.executionOrder" in source
    assert '"Compiler Diagnostics"' in source


def test_editor_is_integrated_into_main_editor_lifecycle_and_window_state():
    app = read("Engine/Core/Application/GameApplication.cpp")
    windows = read("Engine/Editor/EditorWindowManager.h")
    menu = read("Engine/Editor/EditorWindowManager.cpp")
    assert '"Engine/Vfx/Graph/Editor/VfxGraphEditor.h"' in app
    assert "VfxGraphEditor::GetInstance()->Initialize()" in app
    assert "VfxGraphEditor::GetInstance()->Draw(&editorWindowState.showVfxGraphEditor)" in app
    assert "showVfxGraphEditor" in windows
    assert '"VFX Graph Editor"' in menu


def test_phase25_showcase_is_a_serializable_connected_graph():
    path = ROOT / "Resources/VfxGraph/Phase25/EditorPreviewShowcase.vfxgraph.json"
    graph = json.loads(path.read_text(encoding="utf-8"))
    assert graph["schemaVersion"] == 1
    assert graph["graphName"] == "Phase25EditorPreviewShowcase"
    emitter = graph["emitters"][0]
    ids = {node["id"] for node in emitter["nodes"]}
    assert len(ids) >= 8
    assert all("editorPosition" in node for node in emitter["nodes"])
    assert all(edge["from"] in ids and edge["to"] in ids for edge in emitter["edges"])


def test_phase25_stays_on_editor_preview_scope_boundary():
    source = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    assert "GpuFluid" not in source
    assert "LightManager" not in source
    assert "PostEffectManager" not in source
    targets = read("Directory.Build.targets")
    assert "Vfx\\Graph\\Editor\\VfxGraphEditor.cpp" in targets
    assert "EditorPreviewShowcase.vfxgraph.json" in targets
