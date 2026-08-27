import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_graph_has_collision_event_and_sub_emitter_types():
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


def test_compiler_rejects_invalid_event_and_collision_configurations():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    for marker in (
        "SubEmitter requires a Collision node with generateEvent=true",
        "SubEmitter requires an enabled DeathEvent node",
        "sub emitter count must be 1-kMaxSubEmitterSpawnCount",
        "plane normal must be non-zero",
        "sphere radius must be > 0",
        "restitution must be in [0, 1]",
        "friction must be in [0, 1]",
    ):
        assert marker in compiler


def test_collision_nodes_lower_to_gpu_particle_backend():
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


def test_serializer_round_trips_collision_event_payloads():
    serializer = read("Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp")
    for key in (
        '"shape"', '"response"', '"planeNormal"', '"sphereCenter"',
        '"restitution"', '"friction"', '"generateEvent"', '"sourceEvent"', '"inheritVelocity"',
    ):
        assert key in serializer
    assert "TryParseVfxCollisionShape" in serializer
    assert "TryParseVfxCollisionResponse" in serializer
    assert "TryParseVfxParticleEventType" in serializer


def test_cpu_hlsl_collision_layout_contracts_match():
    emitter_data = read("Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEmitterData.h")
    buffers = read("Engine/Graphics/Renderer/GpuParticle/Buffers/GpuParticleBuffers.h")
    hlsl = read("Resources/Shaders/GpuParticle/GpuParticleData.hlsli")
    assert "static_assert(sizeof(GpuEmitterCBData) == 624)" in emitter_data
    assert "static_assert(sizeof(ParticleCS) == 544)" in buffers
    for token in (
        "collisionShape", "collisionResponse", "eventMask", "subEmitterEventMask",
        "collisionPlaneNormal", "collisionSphereCenter", "subEmitterStartColor", "subEmitterEndColor",
    ):
        assert token in emitter_data
        assert token in buffers
        assert token in hlsl


def test_update_shader_executes_collision_and_gpu_local_events():
    shader = read("Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl")
    data = read("Resources/Shaders/GpuParticle/GpuParticleData.hlsli")
    for marker in (
        "ResolveAuthoredCollision", "GPU_PARTICLE_COLLISION_PLANE", "GPU_PARTICLE_COLLISION_SPHERE",
        "GPU_PARTICLE_COLLISION_KILL", "GPU_PARTICLE_COLLISION_SLIDE", "collisionRestitution",
        "collisionFriction", "GPU_PARTICLE_CUSTOM_COLLISION_LATCHED", "SpawnSubEmitter",
        "TryAllocateParticle", "SpawnSubEmitter(p, GPU_PARTICLE_EVENT_COLLISION",
        "SpawnSubEmitter(p, GPU_PARTICLE_EVENT_DEATH",
    ):
        assert marker in shader or marker in data
    assert "Readback" not in shader


def test_collision_sample_exercises_collision_and_death_sub_emitters():
    sample = json.loads(
        (ROOT / "Resources/VfxGraph/Samples/CollisionSubEmitterBurst.vfxgraph.json").read_text(encoding="utf-8")
    )
    assert sample["schemaVersion"] == 1
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
