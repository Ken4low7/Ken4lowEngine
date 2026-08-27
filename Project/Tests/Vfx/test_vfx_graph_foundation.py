import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase20_graph_schema_is_versioned_and_bounded():
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


def test_foundation_has_four_explicit_execution_stages():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    cpp = read("Engine/Vfx/Graph/Data/VfxGraphTypes.cpp")
    for stage in ("Spawn", "Initialize", "Update", "Render"):
        assert stage in types
        assert f'"{stage}"' in cpp
    assert "GetExpectedVfxGraphNodeStage" in types
    assert "VfxGraphNodeType::Gravity" in cpp
    assert "VfxGraphNodeStage::Update" in cpp
    assert "VfxGraphNodeType::SpriteRenderer" in cpp
    assert "VfxGraphNodeStage::Render" in cpp


def test_serializer_is_strict_about_schema_stage_type_and_payload():
    serializer = read("Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp")
    assert "graph.schemaVersion != VfxGraphDesc::kSchemaVersion" in serializer
    assert "TryParseVfxGraphNodeStage" in serializer
    assert "TryParseVfxGraphNodeType" in serializer
    assert "ReadNodePayload" in serializer
    assert "return false" in serializer
    assert '"editorPosition"' in serializer
    assert '"parameterBindings"' in serializer


def test_compiler_validates_ids_edges_stages_and_cycles():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "duplicate node id" in compiler
    assert "edge references missing node" in compiler
    assert "contains self edge" in compiler
    assert "contains duplicate edge" in compiler
    assert "contains backward stage edge" in compiler
    assert "contains a cycle" in compiler
    assert "wrong stage" in compiler
    assert "payload does not match node type" in compiler


def test_topological_sort_is_deterministic_by_stage_then_node_id():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "sortReady" in compiler
    assert "nodeA->stage != nodeB->stage" in compiler
    assert "return a < b" in compiler
    assert "outOrder.push_back(nodeId)" in compiler


def test_compiler_rejects_ambiguous_foundation_modules():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "duplicate enabled node type" in compiler
    assert "may enable only one spawn shape node" in compiler
    # Later phases add renderer types, but the one-renderer-per-emitter foundation contract remains unchanged.
    assert "requires exactly one enabled renderer node" in compiler


def test_graph_lowers_to_existing_phase13_effect_desc():
    program = read("Engine/Vfx/Graph/Runtime/VfxGraphProgram.h")
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "GpuParticleEffectDesc particleEffect" in program
    assert "CreateDefaultSpriteEmitterDesc" in compiler
    for backend_field in (
        "outEmitter.burstCount",
        "outEmitter.spawnRate",
        "outEmitter.spawnShape",
        "outEmitter.lifeTime",
        "outEmitter.velocity",
        "outEmitter.startColor",
        "outEmitter.startSize",
        "outEmitter.gravity",
        "outEmitter.damping",
        "outEmitter.texturePath",
    ):
        assert backend_field in compiler


def test_user_parameters_reuse_phase13_contract():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    runtime = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.cpp")
    assert "GpuParticleUserParameterDesc" in types
    assert "GpuParticleParameterBindingDesc" in types
    assert "binding references unknown parameter" in compiler
    assert "SetFloatParameter" in runtime
    assert "GpuParticleEffectRuntime::GetInstance()->SetFloatParameter" in runtime


def test_runtime_is_a_facade_over_phase13_not_second_particle_backend():
    header = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h")
    cpp = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.cpp")
    assert "GpuParticleEffectRuntime" in header
    assert "RegisterGraph" in header
    assert "LoadGraph" in header
    assert "ReloadGraph" in header
    assert "PlayLoop" in header
    assert "SetLoopPosition" in header
    assert "GpuParticleEffectRuntime::GetInstance()->RegisterEffect" in cpp
    assert "GpuParticleEffectRuntime::GetInstance()->Play(" in cpp
    assert "GpuParticleManager" not in cpp


def test_sample_energy_burst_is_a_real_stage_ordered_graph():
    sample = json.loads((ROOT / "Resources/VfxGraph/Phase20/EnergyBurst.vfxgraph.json").read_text(encoding="utf-8"))
    assert sample["schemaVersion"] == 1
    assert sample["graphName"] == "Phase20EnergyBurst"
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
    assert emitter["parameterBindings"][0]["target"] == "BurstCount"
    assert emitter["parameterBindings"][1]["target"] == "Size"


def test_phase20_runtime_has_basic_diagnostics():
    header = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h")
    for stat in (
        "registeredGraphs",
        "compileFailures",
        "playRequests",
        "playSuccesses",
        "loopStarts",
        "loopStops",
        "reloads",
    ):
        assert stat in header
    assert "GetStats" in header
    assert "GetLastStatus" in header


def test_phase20_build_module_workflow_and_docs_are_registered():
    build = read("Directory.Build.targets")
    modules = read("Build/Modules/EngineModules.json")
    workflow = read("../.github/workflows/Phase20VfxGraphCI.yml")
    docs = read("Docs/Phase20NiagaraVfxGraphFoundation.md")
    for path in (
        "Engine\\Vfx\\Graph\\Data\\VfxGraphTypes.cpp",
        "Engine\\Vfx\\Graph\\Asset\\VfxGraphSerializer.cpp",
        "Engine\\Vfx\\Graph\\Runtime\\VfxGraphCompiler.cpp",
        "Engine\\Vfx\\Graph\\Runtime\\VfxGraphRuntime.cpp",
    ):
        assert path in build
    assert '"Engine/Vfx/Graph"' in modules
    assert "Tests/Phase20/run_phase20_static_tests.py" in workflow
    assert "- [x] 20.10" in docs
    assert "Phase23: ribbon / trail" in docs
