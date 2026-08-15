from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase16_10_runtime_and_editor_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidResetPass.h",
        "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidResetPass.cpp",
        "Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.h",
        "Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.cpp",
        "Engine/Editor/GpuFluidDiagnosticsPanel.h",
        "Engine/Editor/GpuFluidDiagnosticsPanel.cpp",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_runtime_owns_final_fixed_step_order_and_frame_guard():
    header = read("Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.h")
    source = read("Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.cpp")

    assert "GpuFluidVelocityAdvectionPass velocityAdvectionPass_" in header
    assert "GpuFluidPressureProjectionPass pressureProjectionPass_" in header
    assert "GpuFluidScalarAdvectionPass scalarAdvectionPass_" in header
    assert "GpuFluidForcePass forcePass_" in header
    assert "GpuFluidEmitterInjectionPass emitterInjectionPass_" in header
    assert "GpuFluidObstacleRasterPass obstacleRasterPass_" in header
    assert "GpuFluidResetPass resetPass_" in header

    obstacle = source.index("obstacleRasterPass_.Dispatch(")
    emitter = source.index("emitterInjectionPass_.Dispatch(")
    velocity = source.index("velocityAdvectionPass_.Dispatch(")
    projection_first = source.index("pressureProjectionPass_.Dispatch(")
    scalar = source.index("scalarAdvectionPass_.DispatchAll(")
    force = source.index("forcePass_.DispatchAll(")
    projection_second = source.index("pressureProjectionPass_.Dispatch(", projection_first + 1)
    assert obstacle < emitter < velocity < projection_first < scalar < force < projection_second

    assert "GetCurrentFrameIndex()" in source
    assert "GetFrameFenceValue(frameIndex)" in source
    assert "lastUpdateFrameIndex_ == frameIndex" in source
    assert "lastUpdateFrameFenceValue_ == frameFenceValue" in source
    assert "const bool worldChanged = activeWorld_ != &world" in source


def test_runtime_supports_fixed_step_pause_step_reset_and_safe_reconfigure():
    source = read("Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.cpp")
    header = read("Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.h")
    types = read("Engine/Graphics/Renderer/GpuFluid/Data/GpuFluidTypes.h")

    assert "SetPaused(bool paused)" in header
    assert "RequestSingleStep()" in header
    assert "RequestReset()" in header
    assert "accumulatorSeconds_ >= fixedDeltaTime" in source
    assert "simulationDesc_.maxSubsteps" in source
    assert "ExecuteSimulationStep(fixedDeltaTime)" in source
    assert "resetPass_.Reset(grid_)" in source
    assert "pendingReconfigure_.pending = true" in source
    assert "fence->WaitForValue(fence->GetCurrentValue())" in source
    assert "const Vector3 domainCenter" in source
    assert "domain_.origin = domainCenter -" in source
    assert "kMaxDimension = 2048" in types
    assert "width <= kMaxDimension" in types
    assert "height <= kMaxDimension" in types
    assert "kMaxPressureIterations = 256" in types
    assert "pressureIterations <= kMaxPressureIterations" in types


def test_reset_pass_clears_all_fields_and_resets_ping_pong_generation():
    source = read("Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidResetPass.cpp")

    assert "ClearUnorderedAccessViewFloat" in source
    assert "ClearUnorderedAccessViewUint" in source
    assert "grid.GetVelocity()" in source
    assert "grid.GetPressure()" in source
    assert "grid.GetDensity()" in source
    assert "grid.GetTemperature()" in source
    assert "grid.GetDivergence()" in source
    assert "grid.GetVorticity()" in source
    assert "grid.GetObstacle()" in source
    assert "grid.GetVelocity().Reset()" in source
    assert "grid.GetPressure().Reset()" in source
    assert "grid.GetDensity().Reset()" in source
    assert "grid.GetTemperature().Reset()" in source


def test_stress_presets_use_real_source_contracts():
    source = read("Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.cpp")

    assert "RequestGridReconfigure(256, 256, 0.10f, 40)" in source
    assert "RequestGridReconfigure(512, 512, 0.075f, 60)" in source
    assert "RequestGridReconfigure(1024, 1024, 0.05f, 80)" in source
    assert "syntheticEmitterCount_ = 8" in source
    assert "syntheticEmitterCount_ = 24" in source
    assert "syntheticEmitterCount_ = 64" in source
    assert "GpuFluidEmitterSource source{}" in source
    assert "GpuFluidObstacleSource obstacle{}" in source
    assert "GpuFluidObstacleShape::Sphere" in source


def test_editor_panel_exposes_runtime_visualization_upload_diagnostics_and_stress_controls():
    panel = read("Engine/Editor/GpuFluidDiagnosticsPanel.cpp")
    overlay = read("Engine/Editor/EditorLevelOverlay.h")

    assert "ImGuiKey_F12" in panel
    assert 'ImGui::Checkbox("Paused"' in panel
    assert 'ImGui::Button("Step")' in panel
    assert 'ImGui::Button("Reset")' in panel
    assert 'ImGui::Combo("Render Mode"' in panel
    assert 'ImGui::InputInt("Grid Width"' in panel
    assert 'ImGui::SliderInt("Pressure Iterations"' in panel
    assert "approximateGpuMemoryBytes" in panel
    assert "GetFrameUploadArena().GetStats()" in panel
    assert "overflowAllocationCount" in panel
    assert "obstacleDispatchCount" in panel
    assert "emitterDispatchCount" in panel
    assert 'ImGui::Button("Medium")' in panel
    assert 'ImGui::Button("Heavy")' in panel
    assert 'ImGui::Button("Extreme")' in panel
    assert "GpuFluidDiagnosticsPanel::GetInstance()->Draw()" in overlay


def test_actor_world_and_framework_integrate_runtime_lifecycle():
    actor_world = read("Engine/Scene/Actor/Core/ActorWorld_Draw.cpp")
    framework = read("Engine/Core/Application/Framework.cpp")

    assert "GpuFluidManager::GetInstance()" in actor_world
    assert "UpdateFromWorld(*this, GameTimer::GetInstance()->GetDeltaTime())" in actor_world
    assert "SubmitForward(*forwardQueue)" in actor_world
    assert "GpuFluidManager::GetInstance()->Initialize()" in framework
    assert "GpuFluidManager::GetInstance()->Finalize()" in framework
    assert framework.index("GpuFluidManager::GetInstance()->Finalize()") < framework.index("UAVManager::GetInstance()->Finalize()")


def test_phase16_10_build_and_docs_close_phase16():
    props = read("Directory.Build.props")
    docs = read("Docs/Phase16GpuFluidDynamics.md")

    assert "GpuFluidResetPass.cpp" in props
    assert "GpuFluidManager.cpp" in props
    assert "GpuFluidDiagnosticsPanel.cpp" in props
    assert "GpuFluidResetPass.h" in props
    assert "GpuFluidManager.h" in props
    assert "GpuFluidDiagnosticsPanel.h" in props
    assert "- [x] 16.10 Editor / Diagnostics / Stress Test" in docs
    assert "Phase 16 complete" in docs
    assert "Phase 17" in docs
