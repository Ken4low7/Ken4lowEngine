from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_10_runtime_and_editor_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/GpuVolumetricFluidManager.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/GpuVolumetricFluidManager.cpp",
        "Engine/Editor/GpuVolumetricFluidDiagnosticsPanel.h",
        "Engine/Editor/GpuVolumetricFluidDiagnosticsPanel.cpp",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_runtime_defaults_off_and_initializes_with_deterministic_reset():
    header = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/"
        "GpuVolumetricFluidManager.h"
    )
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/"
        "GpuVolumetricFluidManager.cpp"
    )

    assert "bool runtimeEnabled_ = false" in header
    assert "SetRuntimeEnabled(bool enabled)" in header
    assert "grid_.Initialize(simulationDesc_.grid)" in source
    assert "InitializePasses()" in source
    assert "ResetSimulation()" in source
    assert "resetPass_.Reset(grid_)" in source
    assert "resetRequested_ = initialized_" in source


def test_runtime_initializes_before_collecting_sources_so_first_enabled_frame_is_not_delayed():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/"
        "GpuVolumetricFluidManager.cpp"
    )
    update = source[source.index("void GpuVolumetricFluidManager::UpdateFromWorld"):]

    initialize = update.index("if (!initialized_)")
    collect = update.index("CollectSceneSources(world)", initialize)
    append = update.index("AppendSyntheticStressSources()", collect)
    assert initialize < collect < append
    assert "initializedThisFrame" in update


def test_runtime_fixed_step_order_is_locked_in_one_manager():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/"
        "GpuVolumetricFluidManager.cpp"
    )
    execute = source[source.index("bool GpuVolumetricFluidManager::ExecuteSimulationStep"):]

    obstacle = execute.index("obstacleRasterPass_.Dispatch")
    emitter = execute.index("emitterInjectionPass_.Dispatch")
    velocity = execute.index("velocityAdvectionPass_.Dispatch")
    first_projection = execute.index("pressureProjectionPass_.Dispatch")
    scalar = execute.index("scalarAdvectionPass_.DispatchAll")
    force = execute.index("forcePass_.DispatchAll")
    second_projection = execute.index("pressureProjectionPass_.Dispatch", first_projection + 1)

    assert obstacle < emitter < velocity < first_projection < scalar < force < second_projection
    assert "++stats_.totalSimulationSteps" in execute


def test_runtime_collects_shared_emitters_colliders_and_skips_duplicate_engine_frame_updates():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/"
        "GpuVolumetricFluidManager.cpp"
    )

    assert "BuildVolumetricEmitterSource()" in source
    assert "GpuVolumetricFluidColliderObstacleAdapter::CollectSources" in source
    assert "GetCurrentFrameIndex()" in source
    assert "GetFrameFenceValue(frameIndex)" in source
    assert "duplicateFrameUpdateSkipCount" in source


def test_actor_world_updates_and_submits_3d_volume_before_transparent_execution():
    source = read("Engine/Scene/Actor/Core/ActorWorld_Draw.cpp")

    include_index = source.index("GpuVolumetricFluidManager.h")
    update_index = source.index("volumetricFluidManager->UpdateFromWorld")
    submit_index = source.index("volumetricFluidManager->SubmitForward")
    transparent_index = source.index("ExecuteBucket(ForwardRenderBucket::Transparent)")

    assert include_index < update_index < submit_index < transparent_index


def test_grid_reconfigure_preserves_xyz_center_and_stress_presets_cover_64_and_128_cubes():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/"
        "GpuVolumetricFluidManager.cpp"
    )

    assert "RecenterDomainForGrid" in source
    assert "oldGrid.depth" in source
    assert "newGrid.depth" in source
    assert "axisW" in source
    assert "RequestGridReconfigure(64, 64, 64, 0.25f, 32)" in source
    assert "RequestGridReconfigure(128, 128, 128, 0.125f, 48)" in source
    assert "syntheticEmitterCount_ = 8" in source
    assert "syntheticEmitterCount_ = 24" in source
    assert "要求されたReconfigure自体は失敗" in source


def test_runtime_stats_expose_dispatch_pressure_culling_memory_and_forward_counters():
    header = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/"
        "GpuVolumetricFluidManager.h"
    )
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/"
        "GpuVolumetricFluidManager.cpp"
    )

    for name in [
        "approximateGpuMemoryBytes",
        "lastInjectedEmitterCount",
        "lastCulledEmitterCount",
        "lastRasterObstacleCount",
        "lastCulledObstacleCount",
        "lastPressureIterationCount",
        "velocityDispatchCount",
        "pressureDispatchCount",
        "scalarDispatchCount",
        "forceDispatchCount",
        "forwardDrawCount",
        "forwardPacketCount",
    ]:
        assert name in header
        assert name in source


def test_debug_render_modes_cover_density_temperature_and_obstacles():
    types = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/"
        "GpuVolumetricFluidRenderTypes.h"
    )
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.PS.hlsl"
    )

    for mode in ["Smoke", "DensityDebug", "TemperatureDebug", "ObstacleDebug"]:
        assert mode in types
    assert "kRenderModeDensityDebug" in shader
    assert "kRenderModeTemperatureDebug" in shader
    assert "kRenderModeObstacleDebug" in shader
    assert "float3(density01, density01, density01)" in shader
    assert "lerp(gRender.coldColor.rgb, gRender.hotColor.rgb" in shader
    assert "LoadObstacle(uvw)" in shader


def test_editor_panel_exposes_runtime_solver_lighting_depth_descriptor_and_stress_controls():
    panel = read("Engine/Editor/GpuVolumetricFluidDiagnosticsPanel.cpp")
    overlay = read("Engine/Editor/EditorLevelOverlay.h")

    assert "ImGuiKey_F8" in panel
    assert "Enable 3D Runtime" in panel
    assert "Domain Axis W" in panel
    assert "Grid Depth" in panel
    assert "Directional Scattering" in panel
    assert "Density Debug" in panel
    assert "Temperature Debug" in panel
    assert "Depth-aware Composition" in panel
    assert "RenderDepthContext::GetInstance()->GetStats()" in panel
    assert "Shared SRV Descriptor Heap" in panel
    assert "Shared Frame Upload Arena" in panel
    assert "Baseline 64^3" in panel
    assert "Heavy 128^3" in panel
    assert "GpuVolumetricFluidDiagnosticsPanel::GetInstance()->Draw()" in overlay


def test_build_and_docs_mark_phase17_complete():
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    for name in [
        "GpuVolumetricFluidManager.cpp",
        "GpuVolumetricFluidManager.h",
        "GpuVolumetricFluidDiagnosticsPanel.cpp",
        "GpuVolumetricFluidDiagnosticsPanel.h",
    ]:
        assert name in props
    assert "- [x] 17.10 Editor / Diagnostics / Stress Test" in docs
    assert "## 17.10 Editor / Diagnostics / Stress Test" in docs
    assert "## Phase 17 completion" in docs
