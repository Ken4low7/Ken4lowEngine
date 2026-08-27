import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_graph_declares_ribbon_trail_and_mesh_renderer_payloads():
    header = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    cpp = read("Engine/Vfx/Graph/Data/VfxGraphTypes.cpp")
    for symbol in (
        "VfxGraphRibbonRendererNode",
        "VfxGraphTrailRendererNode",
        "VfxGraphMeshRendererNode",
    ):
        assert symbol in header
    for node_type in ("RibbonRenderer", "TrailRenderer", "MeshRenderer"):
        assert f"VfxGraphNodeType::{node_type}" in cpp
        assert f'"{node_type}"' in cpp
    assert "VfxGraphNodeStage::Render" in cpp


def test_render_type_enum_preserves_sprite_mesh_ribbon_and_trail():
    desc = read("Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEffectDesc.h")
    assert "Sprite = 0" in desc
    assert "Mesh = 1" in desc
    assert "Ribbon = 2" in desc
    assert "Trail = 3" in desc


def test_compiler_accepts_one_renderer_and_lowers_each_mode():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "requires exactly one enabled renderer node" in compiler
    assert "IsRendererNode" in compiler
    assert "outEmitter.renderType = GpuParticleRenderType::Ribbon" in compiler
    assert "outEmitter.renderType = GpuParticleRenderType::Trail" in compiler
    assert "outEmitter.renderType = GpuParticleRenderType::Mesh" in compiler
    assert "outEmitter.startSize = { p.width, p.length }" in compiler
    assert "outEmitter.meshPath = p.meshPath" in compiler
    assert "outEmitter.meshSubMeshIndex = p.subMeshIndex" in compiler
    assert "outEmitter.startScale3D = p.startScale" in compiler
    assert "outEmitter.angularVelocity = p.angularVelocity" in compiler


def test_ribbon_and_trail_use_previous_position_history():
    runtime = read("Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h")
    particle_data = read("Resources/Shaders/GpuParticle/GpuParticleData.hlsli")
    update_shader = read("Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl")
    vertex_shader = read("Resources/Shaders/GpuParticle/GpuParticle.VS.hlsl")
    assert "GpuParticleRenderType::Ribbon" in runtime
    assert "GpuParticleRenderType::Trail" in runtime
    assert "info.kind = GpuParticleKind::Ribbon" in runtime
    assert "info.billboardFlags = BillboardMode::Ribbon" in runtime
    assert "float3 previousTranslate" in particle_data
    assert "p.previousTranslate = p.translate" in update_shader
    assert "float3 segment = particle.translate - historyPosition" in vertex_shader
    assert "float3 side = cross(camForward, tangent)" in vertex_shader


def test_mesh_renderer_uses_mesh_asset_and_draw_pipeline():
    runtime = read("Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h")
    renderer = read("Engine/Graphics/Renderer/GpuParticle/Renderer/GpuParticleRenderer.cpp")
    mesh_vs = read("Resources/Shaders/GpuParticle/GpuParticleMesh.VS.hlsl")
    assert "ResolveMeshId" in runtime
    assert 'info.textureFilePath = "Mesh:" + std::to_string(meshId)' in runtime
    assert "LoadMeshAssetsFromAssimp" in runtime
    assert "DrawMesh" in renderer
    assert "RotateEulerXYZ" in mesh_vs
    assert "particle.rotation3D" in mesh_vs


def test_serializers_round_trip_renderer_parameters():
    graph_serializer = read("Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp")
    effect_serializer = read("Engine/Graphics/Renderer/GpuParticle/Preset/GpuParticleEffectSerializer.cpp")
    for symbol in ("VfxGraphRibbonRendererNode", "VfxGraphTrailRendererNode", "VfxGraphMeshRendererNode"):
        assert symbol in graph_serializer
    for key in ('"width"', '"length"', '"meshPath"', '"subMeshIndex"', '"startScale"', '"endScale"'):
        assert key in graph_serializer
    for render_type in ("Ribbon", "Trail"):
        assert f'if (text == "{render_type}") return GpuParticleRenderType::{render_type}' in effect_serializer


def test_particle_history_layout_stays_explicit():
    particle = read("Engine/Graphics/Renderer/GpuParticle/Buffers/GpuParticleBuffers.h")
    emitter = read("Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEmitterData.h")
    particle_data = read("Resources/Shaders/GpuParticle/GpuParticleData.hlsli")
    assert "static_assert(sizeof(ParticleCS) == 544)" in particle
    assert "static_assert(sizeof(GpuEmitterCBData) == 624)" in emitter
    assert "Vector3 previousTranslate" in particle
    assert "float3 previousTranslate" in particle_data


def test_renderer_showcase_contains_ribbon_trail_and_mesh_emitters():
    sample_path = ROOT / "Resources/VfxGraph/Samples/RibbonTrailMeshShowcase.vfxgraph.json"
    sample = json.loads(sample_path.read_text(encoding="utf-8"))
    assert sample["schemaVersion"] == 1
    assert len(sample["emitters"]) == 3
    renderers = set()
    for emitter in sample["emitters"]:
        nodes = emitter["nodes"]
        renderer_nodes = [node for node in nodes if node["stage"] == "Render"]
        assert len(renderer_nodes) == 1
        renderers.add(renderer_nodes[0]["type"])
        assert len(emitter["edges"]) == len(nodes) - 1
    assert renderers == {"RibbonRenderer", "TrailRenderer", "MeshRenderer"}
