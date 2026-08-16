from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def manager_update_body() -> str:
    manager = read("Engine/Graphics/Renderer/GpuParticle/Manager/GpuParticleManager.cpp")
    return manager.split("void GpuParticleManager::Update(float deltaTime)", 1)[1].split(
        "void GpuParticleManager::Draw()", 1
    )[0]


def test_execution_graph_preserves_update_before_emit_contract():
    graph = read("Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleExecutionGraph.h")
    assert "enum class GpuParticleExecutionPassType" in graph
    assert "Update = 0" in graph
    assert "Emit," in graph
    update_push = "passes_.push_back({ GpuParticleExecutionPassType::Update, 0 })"
    emit_push = "passes_.push_back({ GpuParticleExecutionPassType::Emit, emitterCbAddress })"
    assert update_push in graph
    assert emit_push in graph
    assert graph.index(update_push) < graph.index(emit_push)


def test_execution_graph_exposes_transition_and_barrier_cost_estimates():
    graph = read("Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleExecutionGraph.h")
    assert "EstimateLegacyTransitionCount" in graph
    assert "GetPassCount() * 2u" in graph
    assert "EstimateBatchedTransitionCount" in graph
    assert "GetPassCount() > 0u ? 2u : 0u" in graph
    assert "EstimateUavBarrierCount" in graph
    assert "passCount - 1u" in graph
    assert "EstimatePipelineSwitchCount" in graph


def test_manager_culls_idle_update_and_builds_emit_passes():
    body = manager_update_body()
    assert "GetEstimatedActiveParticleCount() > 0u" in body
    assert "executionGraph.SetUpdateRequired(hadActiveParticles)" in body
    assert "executionGraph.AddEmit(" in body
    assert "if (!executionGraph.HasWork())" in body
    assert "DispatchUpdate();" not in body
    assert "DispatchEmit(" not in body


def test_manager_batches_particle_state_transitions():
    body = manager_update_body()
    assert body.count("ResourceTransition(") == 2
    assert body.count("D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE") == 2
    assert body.count("D3D12_RESOURCE_STATE_UNORDERED_ACCESS") == 2
    assert body.count("UAVManager::GetInstance()->PreDispatch()") == 1
    assert body.count("SetComputeRootSignature(") == 1
    assert body.count("GetParticleUavIndex()") == 1


def test_manager_inserts_global_uav_ordering_barriers_between_passes():
    body = manager_update_body()
    assert "if (passIndex + 1u < passes.size())" in body
    assert "D3D12_RESOURCE_BARRIER_TYPE_UAV" in body
    assert "uavBarrier.UAV.pResource = nullptr" in body
    assert "commandList->ResourceBarrier(1, &uavBarrier)" in body


def test_manager_batches_pipeline_binding_and_preserves_emit_diagnostics():
    body = manager_update_body()
    assert "updatePipelineBound" in body
    assert "emitPipelineBound" in body
    assert body.count("GetCsUpdatePSO()") == 1
    assert body.count("GetCsEmitPSO()") == 1
    assert "++emitDispatchCount_" in body
    assert "SetComputeRootConstantBufferView(2, pass.emitterCbAddress)" in body


def test_phase24_keeps_phase23_particle_layout_and_trail_history():
    buffers = read("Engine/Graphics/Renderer/GpuParticle/Buffers/GpuParticleBuffers.h")
    particle_data = read("Resources/Shaders/GpuParticle/GpuParticleData.hlsli")
    assert "static_assert(sizeof(ParticleCS) == 544)" in buffers
    assert "previousTranslate" in buffers
    assert "previousTranslate" in particle_data


def test_execution_graph_does_not_create_a_second_particle_backend():
    graph = read("Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleExecutionGraph.h")
    manager = read("Engine/Graphics/Renderer/GpuParticle/Manager/GpuParticleManager.cpp")
    assert "GpuParticleExecutionPassType" in graph
    assert "GetParticleBuffer()" in manager_update_body()
    assert "GetCsUpdatePSO()" in manager
    assert "GetCsEmitPSO()" in manager
    assert "ID3D12Resource" not in graph
