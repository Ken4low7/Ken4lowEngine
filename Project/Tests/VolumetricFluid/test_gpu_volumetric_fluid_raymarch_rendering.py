from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_8_raymarch_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/GpuVolumetricFluidRenderTypes.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/GpuVolumetricFluidRaymarchRenderer.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/GpuVolumetricFluidRaymarchRenderer.cpp",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/GpuVolumetricFluidForwardRenderBridge.h",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.VS.hlsl",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.PS.hlsl",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_render_contract_is_384_bytes_and_contains_quality_controls():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/"
        "GpuVolumetricFluidRenderTypes.h"
    )

    assert "enum class GpuVolumetricFluidRenderMode" in source
    assert "ObstacleDebug" in source
    assert "struct GpuVolumetricFluidRenderDesc" in source
    for name in [
        "absorption", "emissionStrength", "stepScale",
        "earlyExitTransmittance", "maxSteps",
        "scatteringStrength", "ambientScattering", "anisotropy",
        "selfShadowStrength", "shadowSampleDistanceCells",
    ]:
        assert name in source
    assert "maxSteps <= 1024u" in source
    assert "struct alignas(16) GpuVolumetricFluidRenderConstants" in source
    assert "static_assert(sizeof(GpuVolumetricFluidRenderConstants) == 384)" in source
    assert "inverseViewProjection" in source
    assert "depthViewportAnisotropy" in source
    assert "lightDirectionIntensity" in source
    assert "gridDimensionsShadowDistance" in source
    assert "static_cast<float>(grid.depth)" in source


def test_renderer_uses_texture3d_depth_srv_active_view_and_vertexless_cube():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/"
        "GpuVolumetricFluidRaymarchRenderer.cpp"
    )

    assert "GetActiveViewProjectionMatrix()" in source
    assert "GetActiveCameraPosition()" in source
    assert "Matrix4x4::TryInverse" in source
    assert "grid.GetDensity().Read()" in source
    assert "grid.GetTemperature().Read()" in source
    assert "grid.GetObstacle()" in source
    assert source.count("D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE") >= 3
    assert "GetActiveDepthSrvIndex" in source
    assert "D3D12_ROOT_PARAMETER rootParameters[5]" in source
    assert "SetGraphicsRootDescriptorTable(\n\t\t4" in source
    assert "IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)" in source
    assert "DrawInstanced(36, 1, 0, 0)" in source
    assert "D3D12_CULL_MODE_NONE" in source
    assert "depthStencilDesc.DepthEnable = FALSE" in source
    assert "D3D12_DEPTH_WRITE_MASK_ZERO" in source
    assert "BlendMode::kBlendModeNormal" in source


def test_vertex_shader_builds_oriented_cube_from_u_v_w_axes():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.VS.hlsl"
    )

    assert "const float3 corners[36]" in shader
    assert "SV_VertexID" in shader
    assert "float4x4 inverseViewProjection" in shader
    assert "domainAxisUWidth" in shader
    assert "domainAxisVHeight" in shader
    assert "domainAxisWDepth" in shader
    assert "gridDimensionsShadowDistance" in shader
    assert "domainOriginAbsorption.xyz" in shader
    assert "output.worldPosition = worldPosition" in shader
    assert "mul(float4(worldPosition, 1.0f), gRender.viewProjection)" in shader


def test_pixel_shader_intersects_oriented_box_and_avoids_double_surface_integration():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.PS.hlsl"
    )

    assert "bool IntersectSlab" in shader
    assert "bool IntersectVolume" in shader
    assert "WorldToVolumeDistance" in shader
    assert "dot(rayDirectionWorld, gRender.domainAxisUWidth.xyz)" in shader
    assert "cameraInside = IsInsideVolumeDistance(origin)" in shader
    assert "cameraInside ? tFar : max(tNear, 0.0f)" in shader
    assert "abs(proxyDistance - targetProxyDistance) > proxyTolerance" in shader
    assert "discard" in shader


def test_pixel_shader_raymarches_density_temperature_front_to_back_with_early_exit():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.PS.hlsl"
    )

    assert "Texture3D<float> gDensity : register(t0)" in shader
    assert "Texture3D<float> gTemperature : register(t1)" in shader
    assert "Texture3D<uint> gObstacle : register(t2)" in shader
    assert "Texture2D<float> gSceneDepth : register(t3)" in shader
    assert "SamplerState gLinearClampSampler : register(s0)" in shader
    assert "desiredStep = max(cellSize * gRender.simulationScales.y" in shader
    assert "stepCount = min(maxSteps, desiredStepCount)" in shader
    assert "SampleLevel(gLinearClampSampler, uvw, 0.0f)" in shader
    assert "1.0f - exp(" in shader
    assert "accumulatedColor += transmittance * sampleAlpha * sampleColor" in shader
    assert "transmittance *= 1.0f - sampleAlpha" in shader
    assert "transmittance <= gRender.emissionEarlyExitStepsMode.y" in shader
    assert "straightColor = accumulatedColor / max(integratedAlpha" in shader


def test_obstacle_debug_is_render_only_and_uses_existing_mask():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.PS.hlsl"
    )
    render_types = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/"
        "GpuVolumetricFluidRenderTypes.h"
    )

    assert "ObstacleDebug" in render_types
    assert "LoadObstacle" in shader
    assert "gRender.gridDimensionsShadowDistance.xyz" in shader
    assert "gObstacle.Load" in shader
    assert "gRender.obstacleColor" in shader


def test_forward_bridge_submits_transparent_packet_and_sorts_by_3d_center():
    bridge = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/"
        "GpuVolumetricFluidForwardRenderBridge.h"
    )

    assert "MaterialBlendMode::Transparent" in bridge
    assert "MakeForwardRenderItem" in bridge
    assert "queue.Submit(item)" in bridge
    assert "gridDesc.depth" in bridge
    assert "axisW" in bridge
    assert "GetActiveCameraPosition()" in bridge
    assert "GetActiveCameraForward()" in bridge
    assert "packet->renderer->Draw(" in bridge


def test_manifest_build_and_docs_keep_phase17_8_registered():
    manifest_types = read("Engine/Graphics/Shader/Manifest/ShaderManifestTypes.h")
    manifest = read("Engine/Graphics/Shader/Manifest/GpuVolumetricFluidShaderManifest.h")
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    assert "GpuVolumetricFluid" in manifest_types
    assert "GpuVolumetricFluidGraphicsShaderId" in manifest
    assert "RaymarchVS" in manifest
    assert "RaymarchPS" in manifest
    assert "GetGraphics" in manifest
    assert "RootSignatureType::GpuVolumetricFluid" in manifest
    for name in [
        "GpuVolumetricFluidRenderTypes.h",
        "GpuVolumetricFluidRaymarchRenderer.cpp",
        "GpuVolumetricFluidRaymarchRenderer.h",
        "GpuVolumetricFluidForwardRenderBridge.h",
        "GpuVolumetricFluidRaymarch.VS.hlsl",
        "GpuVolumetricFluidRaymarch.PS.hlsl",
    ]:
        assert name in props
    assert "- [x] 17.8 Volume Raymarch Rendering" in docs
    assert "## 17.8 Volume Raymarch Rendering" in docs
