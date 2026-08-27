from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_scene_depth_resource_is_typeless_and_shader_readable():
    manager = read("Engine/Graphics/PostEffect/Manager/PostEffectRenderTargetManager.cpp")

    assert "RenderDepthContext::CreateShaderReadableDepth24" in manager
    assert "CreateSRVForDepthBuffer(depthSrvIndex_" in manager
    assert "CreateDSVForTexture2D(depthDsvIndex_" in manager
    assert "ReleaseAttachment(depthResource_.Get())" in manager


def test_scene_render_target_pushes_depth_override_before_world_draw():
    executor = read("Engine/Graphics/PostEffect/Manager/PostEffectExecutor.cpp")

    begin = executor[executor.index("void PostEffectExecutor::BeginDraw()"):
                     executor.index("void PostEffectExecutor::EndDraw()")]
    assert "RenderDepthBindingDesc depthBinding" in begin
    assert "depthBinding.resource = renderTargetManager_->GetDepthResource()" in begin
    assert "depthBinding.colorRtv = renderTarget.rtvHandle" in begin
    assert "depthBinding.writableDsv = dsvHandle" in begin
    assert "depthBinding.viewport = renderTargetManager_->GetViewport()" in begin
    assert "sceneDepthOverrideActive_ = depthContext->PushOverride(depthBinding)" in begin


def test_scene_render_target_never_falls_back_to_stale_swapchain_rtv():
    executor = read("Engine/Graphics/PostEffect/Manager/PostEffectExecutor.cpp")

    begin = executor[executor.index("void PostEffectExecutor::BeginDraw()"):
                     executor.index("void PostEffectExecutor::EndDraw()")]
    assert "if (!sceneDepthOverrideActive_)" in begin
    assert "depthContext->ClearDefaultTarget()" in begin
    assert "D3D12 #904" in begin


def test_scene_depth_override_is_popped_before_post_effect_depth_read():
    executor = read("Engine/Graphics/PostEffect/Manager/PostEffectExecutor.cpp")

    end = executor[executor.index("void PostEffectExecutor::EndDraw()"):
                   executor.index("void PostEffectExecutor::RenderPostEffect()")]
    pop = end.index("RenderDepthContext::GetInstance()->PopOverride()")
    transition = end.index("TransitionDepthTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)")
    assert pop < transition


def test_executor_tracks_override_lifetime_explicitly():
    header = read("Engine/Graphics/PostEffect/Manager/PostEffectExecutor.h")

    assert "bool sceneDepthOverrideActive_ = false" in header


def test_forward_queue_only_prepares_depth_when_an_item_requests_it():
    queue = read("Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h")

    assert "bool requiresShaderReadableDepth = false" in queue
    assert "std::any_of(" in queue
    assert "return item.requiresShaderReadableDepth" in queue
    assert "if (requiresDepthRead)" in queue
    assert "PrepareForShaderRead()" in queue


def test_volumetric_forward_packet_explicitly_requests_scene_depth():
    bridge = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/"
        "GpuVolumetricFluidForwardRenderBridge.h"
    )

    assert "CalculateSortDepth(grid, domain),\n\t\t\ttrue" in bridge
    assert "Depth SRV化を明示要求" in bridge
