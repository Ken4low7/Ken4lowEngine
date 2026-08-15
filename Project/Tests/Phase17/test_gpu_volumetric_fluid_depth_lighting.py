from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_9_depth_context_files_exist():
    for relative in [
        "Engine/Graphics/RenderTarget/Depth/RenderDepthContext.h",
        "Engine/Graphics/RenderTarget/Depth/RenderDepthContext.cpp",
    ]:
        assert (ROOT / relative).is_file(), relative


def test_depth_context_creates_typeless_depth_readonly_dsv_and_r24_srv():
    header = read("Engine/Graphics/RenderTarget/Depth/RenderDepthContext.h")
    source = read("Engine/Graphics/RenderTarget/Depth/RenderDepthContext.cpp")

    assert "CreateShaderReadableDepth24" in header
    assert "RenderDepthContextStats" in header
    assert "DXGI_FORMAT_R24G8_TYPELESS" in source
    assert "DXGI_FORMAT_D24_UNORM_S8_UINT" in source
    assert "D3D12_DSV_FLAG_READ_ONLY_DEPTH" in source
    assert "D3D12_DSV_FLAG_READ_ONLY_STENCIL" in source
    assert "CreateSRVForDepthBuffer" in source
    assert "D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE" in source
    assert "D3D12_RESOURCE_STATE_DEPTH_WRITE" in source
    assert "PrepareForShaderRead" in source
    assert "RestoreDepthWrite" in source


def test_main_render_target_registers_shader_readable_depth_and_releases_on_resize():
    source = read("Engine/Graphics/RenderTarget/Main/MainRenderTarget.cpp")

    assert "RenderDepthContext::CreateShaderReadableDepth24" in source
    assert "RenderDepthBindingDesc depthBinding" in source
    assert "depthBinding.colorRtv = rtvHandle" in source
    assert "depthBinding.writableDsv = dsvHandle" in source
    assert "SetDefaultTarget(depthBinding)" in source
    assert source.count("ReleaseAttachment(depthStencilResource_.Get())") >= 2


def test_forward_queue_switches_depth_once_for_transparent_and_restores_after_additive():
    queue = read("Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h")

    assert "bucket == ForwardRenderBucket::Transparent" in queue
    assert "RenderDepthContext::GetInstance()->PrepareForShaderRead()" in queue
    assert "bucket == ForwardRenderBucket::Additive" in queue
    assert "RenderDepthContext::GetInstance()->RestoreDepthWrite()" in queue
    assert queue.index("PrepareForShaderRead()") < queue.index("RestoreDepthWrite()")


def test_render_view_override_without_depth_override_fails_safe_instead_of_using_main_depth():
    source = read("Engine/Graphics/RenderTarget/Depth/RenderDepthContext.cpp")

    assert "cameraManager->HasRenderViewOverride() && overrides_.empty()" in source
    assert "Main Depthを誤バインドせずVolume側だけ安全に抑止" in source
    guard = source.index("cameraManager->HasRenderViewOverride() && overrides_.empty()")
    main_binding = source.index("const RenderDepthBindingDesc* binding = GetActiveBinding()")
    assert guard < main_binding


def test_renderer_binds_scene_depth_and_resolves_directional_light_at_draw_execution():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/"
        "GpuVolumetricFluidRaymarchRenderer.cpp"
    )

    assert "Matrix4x4::TryInverse(viewProjection, inverseViewProjection)" in source
    assert "GetActiveDepthSrvIndex()" in source
    assert "GetActiveViewport()" in source
    assert "GetActiveClearDepth()" in source
    assert "LightManager::GetInstance()" in source
    assert "light.lightType != 1u" in source
    assert "-light.direction" in source
    assert "strongestDirectionalIntensity" in source
    assert "D3D12_ROOT_PARAMETER rootParameters[5]" in source
    assert "srvRanges[4]" in source
    assert "GetGPUDescriptorHandle(sceneDepthSrvIndex)" in source
    assert "depthStencilDesc.DepthEnable = FALSE" in source


def test_pixel_shader_reconstructs_scene_depth_and_clips_ray_to_opaque_surface():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.PS.hlsl"
    )

    assert "Texture2D<float> gSceneDepth : register(t3)" in shader
    assert "float ReconstructSceneRayDistance" in shader
    assert "gSceneDepth.Load(int3(pixel, 0))" in shader
    assert "abs(sceneDepth - clearDepth) <= 1.0e-5f" in shader
    assert "mul(clipPosition, gRender.inverseViewProjection)" in shader
    assert "dot(sceneWorld - gRender.cameraPositionOpacity.xyz, rayDirection)" in shader
    assert "hasOpaqueSurface ? min(tFar, sceneRayDistance) : tFar" in shader


def test_pixel_shader_uses_directional_phase_scattering_and_one_tap_self_shadow():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.PS.hlsl"
    )

    assert "EvaluateHenyeyGreenstein" in shader
    assert "12.5663706f" in shader
    assert "EvaluateLightTransmittance" in shader
    assert "shadowSample" not in shader  # implementation keeps only one extra density lookup, not a nested march
    assert "shadowWorld = sampleWorld + lightDirectionToLight * shadowDistance" in shader
    assert "gDensity.SampleLevel(gLinearClampSampler, WorldToUvw(shadowWorld), 0.0f)" in shader
    assert "directScattering" in shader
    assert "gRender.lightDirectionIntensity.w" in shader
    assert "gRender.lightColorScattering.w" in shader
    assert "gRender.ambientSelfShadow.rgb + directScattering" in shader


def test_build_and_docs_register_phase17_9_and_advance_to_diagnostics():
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    assert "RenderDepthContext.cpp" in props
    assert "RenderDepthContext.h" in props
    assert "- [x] 17.9 Depth-aware composition / lighting" in docs
    assert "## 17.9 Depth-aware composition / lighting" in docs
    assert "## Next implementation target — 17.10" in docs
