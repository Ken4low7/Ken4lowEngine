import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase22_graph_has_collision_event_and_sub_emitter_types():
    header = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    cpp = read("Engine/Vfx/Graph/Data/VfxGraphTypes.cpp")
    for symbol in (
        "VfxParticleEventType",
        "VfxCollisionShape",
        "VfxCollisionResponse",
        "VfxGraphCollisionNode",
        "VfxGraphDeathEventNode",
        "VfxGraphSubEmitterNode",
        "kMaxSubEmitterSpawnCount = 64u",
    ):
        assert symbol in header
    for node_type in ("Collision", "DeathEvent", "SubEmitter"):
        assert f"VfxGraphNodeType::{node_type}" in cpp
    assert "VfxGraphNodeStage::Update" in cpp


def test_compiler_rejects_sub_emitters_without_matching_event_producers():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "SubEmitter requires a Collision node with generateEvent=true" in compiler
    assert "SubEmitter requires an enabled DeathEvent node" in compiler
    assert "sub emitter count must be 1-kMaxSubEmitterSpawnCount" in compiler
    assert "plane normal must be non-zero" in compiler
    assert "sphere radius must be > 0" in compiler
    assert "restitution must be in [0, 1]" in compiler
    assert "friction must be in [0, 1]" in compiler


def test_compiler_lowers_phase22_nodes_to_existing_gpu_particle_backend():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    backend = read("Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEffectDesc.h")
    for token in (
        "outEmitter.collisionShape",
        "outEmitter.collisionResponse",
        "outEmitter.eventMask",
        "outEmitter.subEmitterEventMask",
        "outEmitter.subEmitterCount",
        "outEmitter.subEmitterLifeTime",
        "outEmitter.subEmitterSpeed",
    ):
        assert token in compiler
    for token in (
        "GpuParticleCollisionShape",
        "GpuParticleCollisionResponse",
        "kGpuParticleEventCollision",
        "kGpuParticleEventDeath",
        "subEmitterEventMask",
    ):
        assert token in backend


def test_serializer_round_trips_collision_event_and_sub_emitter_payloads():
    serializer = read("Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp")
    for key in (
        '"shape"',
        '"response"',
        '"planeNormal"',
        '"sphereCenter"',
        '"restitution"',
        '"friction"',
        '"generateEvent"',
        '"sourceEvent"',
        '"inheritVelocity"',
    ):
        assert key in serializer
    assert "TryParseVfxCollisionShape" in serializer
    assert "TryParseVfxCollisionResponse" in serializer
    assert "TryParseVfxParticleEventType" in serializer


def test_cpu_hlsl_phase22_layout_contracts_are_explicit():
    emitter_data = read("Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEmitterData.h")
    buffers = read("Engine/Graphics/Renderer/GpuParticle/Buffers/GpuParticleBuffers.h")
    hlsl = read("Resources/Shaders/GpuParticle/GpuParticleData.hlsli")
    assert "static_assert(sizeof(GpuEmitterCBData) == 624)" in emitter_data
    assert "static_assert(sizeof(ParticleCS) == 528)" in buffers
    for token in (
        "collisionShape",
        "collisionResponse",
        "eventMask",
        "subEmitterEventMask",
        "collisionPlaneNormal",
        "collisionSphereCenter",
        "subEmitterStartColor",
        "subEmitterEndColor",
    ):
        assert token in emitter_data
        assert token in buffers
        assert token in hlsl


def test_update_shader_executes_plane_sphere_responses_and_event_latching():
    shader = read("Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl")
    data = read("Resources/Shaders/GpuParticle/GpuParticleData.hlsli")
    assert "ResolveAuthoredCollision" in shader
    assert "GPU_PARTICLE_COLLISION_PLANE" in shader
    assert "GPU_PARTICLE_COLLISION_SPHERE" in shader
    assert "GPU_PARTICLE_COLLISION_KILL" in shader
    assert "GPU_PARTICLE_COLLISION_SLIDE" in shader
    assert "collisionRestitution" in shader
    assert "collisionFriction" in shader
    assert "GPU_PARTICLE_CUSTOM_COLLISION_LATCHED" in shader
    assert "GPU_PARTICLE_CUSTOM_COLLISION_LATCHED" in data


def test_sub_emitters_allocate_from_gpu_free_list_without_cpu_readback():
    shader = read("Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl")
    manager = read("Engine/Graphics/Renderer/GpuParticle/Manager/GpuParticleManager.cpp")
    assert "SpawnSubEmitter" in shader
    assert "TryAllocateParticle" in shader
    assert "InterlockedAdd(gFreeListIndex[0], -1, top)" in shader
    assert "child.eventMask = 0u" in shader
    assert "child.subEmitterEventMask = 0u" in shader
    assert "child.subEmitterCount = 0u" in shader
    assert "Readback" not in shader
    assert "DispatchUpdate" in manager


def test_collision_and_natural_death_both_emit_gpu_local_events():
    shader = read("Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl")
    assert "SpawnSubEmitter(p, GPU_PARTICLE_EVENT_COLLISION" in shader
    assert "SpawnSubEmitter(p, GPU_PARTICLE_EVENT_DEATH" in shader
    assert "p.currentTime >= p.lifeTime" in shader
    assert "collisionKill" in shader


def test_phase22_sample_exercises_collision_and_death_sub_emitters():
    sample = json.loads(
        (ROOT / "Resources/VfxGraph/Phase22/CollisionSubEmitterBurst.vfxgraph.json").read_text(encoding="utf-8")
    )
    assert sample["schemaVersion"] == 1
    assert sample["graphName"] == "Phase22CollisionSubEmitterBurst"
    assert len(sample["emitters"]) == 2

    collision_emitter = sample["emitters"][0]
    collision_nodes = {node["type"]: node for node in collision_emitter["nodes"]}
    assert collision_nodes["Collision"]["params"]["shape"] == "Plane"
    assert collision_nodes["Collision"]["params"]["response"] == "Bounce"
    assert collision_nodes["Collision"]["params"]["generateEvent"] is True
    assert collision_nodes["SubEmitter"]["params"]["sourceEvent"] == "Collision"

    death_emitter = sample["emitters"][1]
    death_nodes = {node["type"]: node for node in death_emitter["nodes"]}
    assert "DeathEvent" in death_nodes
    assert death_nodes["SubEmitter"]["params"]["sourceEvent"] == "Death"


def test_phase22_keeps_phase23_features_out_of_collision_backend():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    assert "RibbonRenderer" not in types
    assert "TrailRenderer" not in types
    assert "MeshParticleRenderer" not in types
