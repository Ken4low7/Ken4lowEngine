from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def test_phase26_appends_integration_node_types_without_reordering_phase23_values():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    assert "MeshRenderer,\n\tFluidOutput,\n\tLightOutput,\n\tPostEffectOutput" in types
    assert "VfxGraphFluidOutputNode" in types
    assert "VfxGraphLightOutputNode" in types
    assert "VfxGraphPostEffectOutputNode" in types


def test_integration_node_stages_keep_fluid_in_update_and_presentation_outputs_in_render():
    source = read("Engine/Vfx/Graph/Data/VfxGraphTypes.cpp")
    assert "case VfxGraphNodeType::FluidOutput:" in source
    assert "case VfxGraphNodeType::LightOutput:" in source
    assert "case VfxGraphNodeType::PostEffectOutput:" in source
    assert "VfxGraphFluidDomain::Volumetric3D" in source


def test_integration_compiler_lowers_to_existing_phase18_track_contracts():
    source = read("Engine/Vfx/Graph/Runtime/VfxGraphIntegrationCompiler.cpp")
    assert "VfxCueTrackType::Fluid2D" in source
    assert "VfxCueTrackType::VolumetricFluid" in source
    assert "VfxCueTrackType::Light" in source
    assert "VfxCueTrackType::PostEffect" in source
    assert "VfxFluidTrackPayload" in source
    assert "VfxLightTrackPayload" in source
    assert "VfxPostEffectTrackPayload" in source
    assert "VfxCueBindingTarget::IntensityScale" in source
    assert "VfxCueBindingTarget::RadiusScale" in source
    assert "GpuFluidManager" not in source
    assert "LightManager" not in source
    assert "PostEffectManager" not in source


def test_graph_compiler_keeps_particle_backend_and_adds_integration_side_program():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    program = read("Engine/Vfx/Graph/Runtime/VfxGraphProgram.h")
    assert "VfxGraphIntegrationCompiler::Compile" in compiler
    assert "GpuParticleEffectDesc particleEffect" in program
    assert "integrationOneShotCue" in program
    assert "integrationLoopCue" in program
    assert "HasIntegrationTracks" in program


def test_runtime_reuses_phase13_particles_and_phase18_cue_runtime():
    runtime = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.cpp")
    header = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h")
    assert "GpuParticleEffectRuntime::GetInstance()->Play" in runtime
    assert "VfxCueRuntime::GetInstance()->Play" in runtime
    assert "VfxCueHandle integrationHandle" in header
    assert "SetWorldPosition(handle.integrationHandle" in runtime
    assert "SetFloatParameter(handle.integrationHandle" in runtime
    assert "Stop(handle.integrationHandle" in runtime
    assert "GpuFluidManager" not in runtime
    assert "LightManager" not in runtime
    assert "PostEffectManager" not in runtime


def test_loop_start_rolls_back_particle_handle_when_integration_start_fails():
    runtime = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.cpp")
    marker = "Phase18 runtime failed to start loop graph integrations"
    assert marker in runtime
    failure_area = runtime[runtime.index(marker) - 500: runtime.index(marker) + 200]
    assert "StopLoop(particleHandle)" in failure_area


def test_serializer_round_trips_phase26_payload_fields():
    serializer = read("Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp")
    for key in ("domain", "localOffset", "localVelocity", "duration", "radius", "velocityStrength", "densityRate", "temperatureRate", "falloffExponent", "intensityParameter", "radiusParameter", "effectName", "weight"):
        assert f'\"{key}\"' in serializer
    assert "TryParseVfxGraphFluidDomain" in serializer


def test_phase25_editor_exposes_phase26_outputs_without_direct_subsystem_ownership():
    editor = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    assert "std::array<VfxGraphNodeType, 25>" in editor
    for name in ("FluidOutput", "LightOutput", "PostEffectOutput"):
        assert f"VfxGraphNodeType::{name}" in editor
    assert "VfxGraphFluidOutputNode" in editor
    assert "VfxGraphLightOutputNode" in editor
    assert "VfxGraphPostEffectOutputNode" in editor
    assert "GpuFluidManager" not in editor
    assert "LightManager" not in editor
    assert "PostEffectManager" not in editor


def test_phase26_showcase_contains_particle_fluid_light_and_posteffect_outputs():
    graph = json.loads((ROOT / "Resources/VfxGraph/Phase26/IntegratedExplosion.vfxgraph.json").read_text(encoding="utf-8"))
    assert graph["schemaVersion"] == 1
    assert graph["graphName"] == "Phase26IntegratedExplosion"
    nodes = graph["emitters"][0]["nodes"]
    types = {node["type"] for node in nodes}
    assert {"SpriteRenderer", "FluidOutput", "LightOutput", "PostEffectOutput"}.issubset(types)
    fluid = next(node for node in nodes if node["type"] == "FluidOutput")
    assert fluid["params"]["domain"] == "Volumetric3D"
    assert fluid["params"]["intensityParameter"] == "Intensity"
    assert fluid["params"]["radiusParameter"] == "Radius"


def test_phase26_keeps_phase27_bounds_lod_budget_scope_separate():
    docs = read("Docs/Phase26FluidLightPostEffectIntegration.md")
    assert "Bounds, LOD, budget, and culling remain Phase27" in docs
    integration = read("Engine/Vfx/Graph/Runtime/VfxGraphIntegrationCompiler.cpp")
    assert "Cull" not in integration
    assert "LOD" not in integration
