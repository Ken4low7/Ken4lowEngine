from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPO = ROOT.parent


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8-sig")


def test_diagnostics_owns_bounded_240_frame_ring_history():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    assert "kHistoryCapacity = 240u" in diagnostics
    assert "std::array<VfxDiagnosticsFrameSample, kHistoryCapacity> history_" in diagnostics
    assert "historyHead_ = (historyHead_ + 1u) % kHistoryCapacity" in diagnostics
    assert "GetSampleFromOldest" in diagnostics
    assert "BuildSummary() const" in diagnostics


def test_capture_reuses_existing_frame_and_runtime_counters():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    assert "GetCompletedFrameTiming()" in diagnostics
    assert "GpuParticleManager::GetInstance()" in diagnostics
    assert "VfxGraphRuntime::GetInstance()->GetStats()" in diagnostics
    assert "VfxCueRuntime::GetInstance()->GetStats()" in diagnostics
    assert "GetEstimatedActiveParticleCount()" in diagnostics
    assert "GetLastDrawCallCount()" in diagnostics
    assert "GetEmitDispatchCount()" in diagnostics


def test_counter_deltas_cover_dispatch_graph_culling_budget_lod_and_cues():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    assert "emitDispatchesThisFrame = Delta" in diagnostics
    assert "graphPlayRequestsThisFrame = Delta" in diagnostics
    assert "graphCullsThisFrame = Delta" in diagnostics
    assert "graphBudgetRejectsThisFrame = Delta" in diagnostics
    assert "graphLodSelectionsThisFrame = Delta" in diagnostics
    assert "cueTrackStartsThisFrame = Delta" in diagnostics
    assert "cueBudgetRejectsThisFrame = Delta" in diagnostics
    assert "cueBudgetDelaysThisFrame = Delta" in diagnostics


def test_phase28_does_not_add_synchronous_gpu_readback_to_diagnostics():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    assert "Readback" not in diagnostics
    assert "WaitForGPU" not in diagnostics
    assert "WaitForGpu" not in diagnostics
    assert "SetEventOnCompletion" not in diagnostics
    assert "GetCompletedValue" not in diagnostics


def test_stress_runner_has_caps_real_graph_submission_and_loop_cleanup():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    assert "kMaxStressOneShots = 512u" in diagnostics
    assert "kMaxStressLoops = 128u" in diagnostics
    assert "runtime->Play(config.graphName" in diagnostics
    assert "runtime->PlayLoop(config.graphName" in diagnostics
    assert "stressLoopHandles_.push_back(handle)" in diagnostics
    assert "runtime->StopLoop(handle)" in diagnostics
    assert "BuildGridPosition" in diagnostics


def test_stress_result_reports_budget_culling_and_particle_pressure():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    assert "graphBudgetRejects" in diagnostics
    assert "graphCulls" in diagnostics
    assert "estimatedActiveParticlesAfterStart" in diagnostics
    assert "activeEmittersAfterStart" in diagnostics
    assert "graphWasRegistered" in diagnostics


def test_editor_exposes_overview_history_stress_and_budget_tabs():
    window = read("Engine/Vfx/Graph/Editor/VfxDiagnosticsWindow.h")
    for tab in ("Overview", "History", "Stress", "Budget"):
        assert f'BeginTabItem("{tab}")' in window
    assert 'PlotLines("Frame ms"' in window
    assert 'Button("Run Stress Burst")' in window
    assert 'Button("Stop Stress Loops")' in window
    assert 'Button("Load Phase27 Sample")' in window
    assert "GetEditableBudget()" in window


def test_application_captures_after_vfx_runtime_updates_and_draws_companion_window():
    app = read("Engine/Core/Application/GameApplication.cpp")
    scalability = app.index("VfxGraphRuntime::GetInstance()->UpdateScalability();")
    cue_update = app.index("VfxCueRuntime::GetInstance()->Update")
    capture = app.index("VfxGraphDiagnostics::GetInstance()->CaptureFrame();")
    assert scalability < cue_update < capture
    assert "VfxDiagnosticsWindow::GetInstance()->Draw(editorWindowState.showVfxGraphEditor);" in app


def test_temporary_phase28_integration_helpers_are_removed():
    assert not (REPO / ".github/scripts/phase28_integrate_diagnostics.py").exists()
    assert not (REPO / ".github/workflows/Phase28IntegrateDiagnostics.yml").exists()


def test_docs_define_nonblocking_contract_and_phase29_boundary():
    docs = read("Docs/Phase28DebugProfilingStressTest.md")
    assert "240" in docs
    assert "no GPU fence wait" in docs
    assert "no synchronous GPU readback" in docs
    assert "512 one-shots" in docs
    assert "128 loops" in docs
    assert "Phase29" in docs
    assert "Production VFX Library" in docs
