import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_graph_schema_is_versioned_and_bounded():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    assert "kSchemaVersion = 1u" in types
    assert "kMaxEmitters = 32u" in types
    assert "kMaxNodesPerEmitter = 128u" in types
    assert "kMaxEdgesPerEmitter = 256u" in types
    assert "graphName" in types
    assert "editorPosition" in types


def test_graph_nodes_use_typed_variant_payloads():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    assert "using VfxGraphNodePayload = std::variant" in types
    for payload in (
        "VfxGraphSpawnRateNode",
        "VfxGraphBurstNode",
        "VfxGraphSpawnPointNode",
        "VfxGraphSpawnSphereNode",
        "VfxGraphSpawnBoxNode",
        "VfxGraphLifetimeNode",
        "VfxGraphInitialVelocityNode",
        "VfxGraphInitialColorNode",
        "VfxGraphInitialSizeNode",
        "VfxGraphGravityNode",
        "VfxGraphDragNode",
        "VfxGraphSpriteRendererNode",
    ):
        assert payload in types


def test_graph_has_explicit_execution_stages():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    cpp = read("Engine/Vfx/Graph/Data/VfxGraphTypes.cpp")
    for stage in ("Spawn", "Initialize", "Update", "Render"):
        assert stage in types
        assert f'"{stage}"' in cpp
    assert "GetExpectedVfxGraphNodeStage" in types


def test_serializer_and_compiler_reject_invalid_graphs():
    serializer = read("Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp")
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "graph.schemaVersion != VfxGraphDesc::kSchemaVersion" in serializer
    assert "TryParseVfxGraphNodeStage" in serializer
    assert "TryParseVfxGraphNodeType" in serializer
    assert "ReadNodePayload" in serializer
    for marker in (
        "duplicate node id",
        "edge references missing node",
        "contains self edge",
        "contains duplicate edge",
        "contains backward stage edge",
        "contains a cycle",
        "wrong stage",
        "payload does not match node type",
    ):
        assert marker in compiler


def test_topological_sort_is_deterministic():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "sortReady" in compiler
    assert "nodeA->stage != nodeB->stage" in compiler
    assert "return a < b" in compiler
    assert "outOrder.push_back(nodeId)" in compiler


def test_runtime_is_facade_over_gpu_particle_effect_runtime():
    header = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h")
    cpp = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.cpp")
    assert "GpuParticleEffectRuntime" in header
    for marker in ("RegisterGraph", "LoadGraph", "ReloadGraph", "PlayLoop", "SetLoopPosition"):
        assert marker in header
    assert "GpuParticleEffectRuntime::GetInstance()->RegisterEffect" in cpp
    assert "GpuParticleEffectRuntime::GetInstance()->Play(" in cpp
    assert "GpuParticleManager" not in cpp


def test_energy_burst_sample_is_stage_ordered():
    sample_path = ROOT / "Resources/VfxGraph/Samples/EnergyBurst.vfxgraph.json"
    sample = json.loads(sample_path.read_text(encoding="utf-8"))
    assert sample["schemaVersion"] == 1
    assert sample["userParameters"][0]["name"] == "Intensity"
    emitter = sample["emitters"][0]
    node_types = [node["type"] for node in emitter["nodes"]]
    assert node_types == [
        "Burst",
        "SpawnSphere",
        "Lifetime",
        "InitialVelocity",
        "InitialColor",
        "InitialSize",
        "Gravity",
        "Drag",
        "SpriteRenderer",
    ]
    assert len(emitter["edges"]) == len(emitter["nodes"]) - 1


def test_vfx_graph_module_is_registered_in_build_and_module_manifest():
    build = read("Directory.Build.targets")
    modules = read("Build/Modules/EngineModules.json")
    for path in (
        "Engine\\Vfx\\Graph\\Data\\VfxGraphTypes.cpp",
        "Engine\\Vfx\\Graph\\Asset\\VfxGraphSerializer.cpp",
        "Engine\\Vfx\\Graph\\Runtime\\VfxGraphCompiler.cpp",
        "Engine\\Vfx\\Graph\\Runtime\\VfxGraphRuntime.cpp",
    ):
        assert path in build
    assert '"Engine/Vfx/Graph"' in modules
    # CIは個別開発工程ではなくTests配下のSubsystemを自動探索する。
    workflow = read("../.github/workflows/DebugReleseBuild.yml")
    assert 'for test_dir in Tests/*' in workflow
