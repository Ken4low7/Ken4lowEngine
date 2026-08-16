from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8-sig")


def test_graph_asset_has_bounds_lod_and_budget_authoring():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    assert "enum class VfxGraphBoundsMode" in types
    assert "struct VfxGraphScalabilityDesc" in types
    assert "fixedBoundsRadius" in types
    assert "frustumCulling" in types
    assert "lodNearDistance" in types and "lodFarDistance" in types
    assert "lodMidScale" in types and "lodFarScale" in types
    assert "budgetCost" in types


def test_serializer_round_trips_phase27_scalability_without_schema_break():
    serializer = read("Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp")
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    assert "kSchemaVersion = 1u" in types
    assert 'root.find("scalability")' in serializer
    assert 'root["scalability"]' in serializer
    assert '"boundsMode"' in serializer
    assert '"maxDrawDistance"' in serializer
    assert '"budgetCost"' in serializer


def test_compiler_builds_conservative_bounds_and_validates_lod_contract():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    program = read("Engine/Vfx/Graph/Runtime/VfxGraphProgram.h")
    assert "EstimateAutomaticBounds" in compiler
    assert "particleExtent" in compiler
    assert "VfxGraphBoundsMode::FixedSphere" in compiler
    assert "LOD scales must satisfy 0 < far <= mid <= 1" in compiler
    assert "budgetCost must be > 0" in compiler
    assert "BoundingSphere localBounds" in program
    assert "VfxGraphScalabilityDesc scalability" in program


def test_lod_scales_real_gpu_emission_instead_of_creating_second_backend():
    runtime = read("Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h")
    assert "SetLoopRuntimeScale" in runtime
    assert "runtimeScale = std::clamp(runtimeScale, 0.0f, 1.0f)" in runtime
    assert "emitterDesc.emission.spawnRate * spawnRateFactor * runtimeScale" in runtime
    assert "ScaleCount(emitterDesc.update.subEmitterCount, runtimeScale)" in runtime
    assert "GpuParticleManager::GetInstance()" in runtime


def test_graph_runtime_reuses_active_camera_frustum_and_unified_budget():
    runtime = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.cpp")
    header = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h")
    budget = read("Engine/Vfx/Runtime/VfxRuntimeTypes.h")
    assert "CameraManager::GetInstance()" in runtime
    assert "Frustum frustum" in runtime
    assert "frustum.Intersects(worldBounds)" in runtime
    assert "VfxCueRuntime::GetInstance()->GetBudget()" in runtime
    assert "maxVfxGraphStartCostPerFrame" in budget
    assert "maxActiveVfxGraphLoopCost" in budget
    assert "activeLoopCost" in header
    assert "budgetRejectedPlays" in header


def test_loop_culling_updates_particle_and_phase18_integration_scale():
    graph_runtime = read("Engine/Vfx/Graph/Runtime/VfxGraphRuntime.cpp")
    cue_runtime = read("Engine/Vfx/Runtime/VfxCueRuntime.cpp")
    assert "UpdateScalability" in graph_runtime
    assert "SetLoopRuntimeScale" in graph_runtime
    assert "SetRuntimeScale(state.handle.integrationHandle, scale)" in graph_runtime
    assert "resolved.intensityScale *= instance.runtimeScale" in cue_runtime
    assert "loopCullTransitions" in graph_runtime


def test_frame_lifecycle_resets_budget_before_gameplay_and_updates_after_camera():
    app = read("Engine/Core/Application/GameApplication.cpp")
    begin = app.index("VfxGraphRuntime::GetInstance()->BeginFrame();")
    scene_update = app.index("sceneManager_->Update();")
    scalability = app.index("VfxGraphRuntime::GetInstance()->UpdateScalability();")
    cue_update = app.index("VfxCueRuntime::GetInstance()->Update")
    assert begin < scene_update < scalability < cue_update


def test_editor_exposes_phase27_scalability_controls():
    editor = read("Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp")
    assert 'TreeNode("Phase27 Scalability")' in editor
    assert '"Bounds Mode"' in editor
    assert '"Frustum Culling"' in editor
    assert '"LOD Mid Scale"' in editor
    assert '"Budget Cost"' in editor


def test_phase27_sample_and_docs_cover_all_four_scope_items():
    sample = read("Resources/VfxGraph/Phase27/ScalableIntegratedExplosion.vfxgraph.json")
    docs = read("Docs/Phase27LodBoundsBudgetCulling.md")
    assert '"scalability"' in sample
    assert '"boundsMode": "Automatic"' in sample
    assert '"frustumCulling": true' in sample
    assert '"lodMidScale": 0.6' in sample
    assert '"budgetCost": 4' in sample
    for word in ("LOD", "Bounds", "Budget", "Culling"):
        assert word in docs


def test_phase28_diagnostics_stays_outside_phase27_scope():
    docs = read("Docs/Phase27LodBoundsBudgetCulling.md")
    assert "Phase28" in docs
    assert "profiling UI" in docs
