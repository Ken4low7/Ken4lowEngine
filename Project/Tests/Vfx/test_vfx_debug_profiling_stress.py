from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8-sig")


def test_diagnostics_owns_bounded_frame_history():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    assert "kHistoryCapacity = 240u" in diagnostics
    assert "std::array<VfxDiagnosticsFrameSample, kHistoryCapacity> history_" in diagnostics
    assert "historyHead_ = (historyHead_ + 1u) % kHistoryCapacity" in diagnostics
    assert "GetSampleFromOldest" in diagnostics
    assert "BuildSummary() const" in diagnostics


def test_capture_reuses_existing_runtime_counters_without_gpu_waits():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    for marker in (
        "GetCompletedFrameTiming()",
        "GpuParticleManager::GetInstance()",
        "VfxGraphRuntime::GetInstance()->GetStats()",
        "VfxCueRuntime::GetInstance()->GetStats()",
        "GetEstimatedActiveParticleCount()",
        "GetLastDrawCallCount()",
        "GetEmitDispatchCount()",
    ):
        assert marker in diagnostics
    for forbidden in ("WaitForGPU", "WaitForGpu", "SetEventOnCompletion"):
        assert forbidden not in diagnostics


def test_stress_runner_has_caps_submission_and_loop_cleanup():
    diagnostics = read("Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h")
    assert "kMaxStressOneShots = 512u" in diagnostics
    assert "kMaxStressLoops = 128u" in diagnostics
    assert "runtime->Play(config.graphName" in diagnostics
    assert "runtime->PlayLoop(config.graphName" in diagnostics
    assert "stressLoopHandles_.push_back(handle)" in diagnostics
    assert "runtime->StopLoop(handle)" in diagnostics
    assert "BuildGridPosition" in diagnostics


def test_editor_uses_localized_tabs_and_feature_named_sample():
    window = read("Engine/Vfx/Graph/Editor/VfxDiagnosticsWindow.h")
    for tab in ("概要", "履歴", "負荷確認", "実行予算"):
        assert f'BeginTabItem("{tab}")' in window
    assert 'Button("負荷確認を実行")' in window
    assert 'Button("負荷確認Loopを停止")' in window
    assert 'Button("負荷確認サンプルを読み込む")' in window
    assert '"ScalableIntegratedExplosion"' in window
    assert "Resources/VfxGraph/Samples/ScalableIntegratedExplosion.vfxgraph.json" in window
    assert "Phase27" not in window


def test_scalable_sample_has_clean_runtime_identity_and_scalability_settings():
    sample = read("Resources/VfxGraph/Samples/ScalableIntegratedExplosion.vfxgraph.json")
    assert '"graphName": "ScalableIntegratedExplosion"' in sample
    assert '"boundsMode": "Automatic"' in sample
    assert '"frustumCulling": true' in sample
    assert '"budgetCost": 4' in sample
    assert "Phase27" not in sample


def test_application_captures_after_completed_frame_and_draws_diagnostics():
    app = read("Engine/Core/Application/GameApplication.cpp")
    scalability = app.index("VfxGraphRuntime::GetInstance()->UpdateScalability();")
    cue_update = app.index("VfxCueRuntime::GetInstance()->Update")
    final_end_frame = app.rindex("GameTimer::GetInstance()->EndFrame();")
    capture = app.index("VfxGraphDiagnostics::GetInstance()->CaptureFrame();")
    assert scalability < cue_update < final_end_frame < capture
    assert app.count("VfxGraphDiagnostics::GetInstance()->CaptureFrame();") == 1
    assert "VfxDiagnosticsWindow::GetInstance()->Draw(editorWindowState.showVfxGraphEditor);" in app


def test_runtime_budget_is_exposed_in_diagnostics_editor():
    window = read("Engine/Vfx/Graph/Editor/VfxDiagnosticsWindow.h")
    assert "GetEditableBudget()" in window
    assert "maxVfxGraphStartCostPerFrame" in window
    assert "maxActiveVfxGraphLoopCost" in window
    assert "maxActiveInstances" in window
    assert "maxActiveTracks" in window
