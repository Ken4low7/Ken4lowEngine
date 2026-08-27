from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase16_9_forward_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Data/GpuFluidRenderTypes.h",
        "Engine/Graphics/Renderer/GpuFluid/Renderer/GpuFluidForwardRenderer.h",
        "Engine/Graphics/Renderer/GpuFluid/Renderer/GpuFluidForwardRenderer.cpp",
        "Engine/Graphics/Renderer/GpuFluid/Renderer/GpuFluidForwardRenderBridge.h",
        "Resources/Shaders/GpuFluid/GpuFluidForward.VS.hlsl",
        "Resources/Shaders/GpuFluid/GpuFluidForward.PS.hlsl",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_render_constants_and_modes_have_fixed_contract():
    types = read("Engine/Graphics/Renderer/GpuFluid/Data/GpuFluidRenderTypes.h")
    assert "enum class GpuFluidRenderMode" in types
    assert "Density = 0" in types
    assert "Temperature" in types
    assert "Obstacle" in types
    assert "struct alignas(16) GpuFluidRenderConstants" in types
    assert "static_assert(sizeof(GpuFluidRenderConstants) == 192)" in types
    assert "grid.width" in types
    assert "grid.height" in types
    assert "grid.cellSize" in types


def test_forward_vertex_shader_builds_world_quad_without_vertex_buffer():
    shader = read("Resources/Shaders/GpuFluid/GpuFluidForward.VS.hlsl")
    assert "SV_VertexID" in shader
    assert "domainOriginOpacity.xyz" in shader
    assert "domainAxisUDensityScale.xyz * uv.x" in shader
    assert "domainAxisVTemperatureScale.xyz * uv.y" in shader
    assert "mul(float4(worldPosition, 1.0f), gRender.viewProjection)" in shader


def test_forward_pixel_shader_exposes_density_temperature_and_obstacle_modes():
    shader = read("Resources/Shaders/GpuFluid/GpuFluidForward.PS.hlsl")
    assert "Texture2D<float> gDensity : register(t0)" in shader
    assert "Texture2D<float> gTemperature : register(t1)" in shader
    assert "Texture2D<uint> gObstacle : register(t2)" in shader
    assert "DrawDensity" in shader
    assert "DrawTemperature" in shader
    assert "DrawObstacle" in shader
    assert "gRender.renderMode == 1u" in shader
    assert "gRender.renderMode == 2u" in shader


def test_renderer_uses_graphics_srv_heap_alpha_blend_and_depth_test():
    source = read("Engine/Graphics/Renderer/GpuFluid/Renderer/GpuFluidForwardRenderer.cpp")
    assert "D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE" in source
    assert "SRVManager::GetInstance()" in source
    assert "descriptors->PreDraw();" in source
    assert "density.srvIndex" in source
    assert "temperature.srvIndex" in source
    assert "obstacle.srvIndex" in source
    assert "BlendMode::kBlendModeNormal" in source
    assert "D3D12_DEPTH_WRITE_MASK_ZERO" in source
    assert "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB" in source
    assert "DXGI_FORMAT_D24_UNORM_S8_UINT" in source
    assert "DrawInstanced(6, 1, 0, 0)" in source


def test_forward_bridge_submits_stable_transparent_packets():
    bridge = read("Engine/Graphics/Renderer/GpuFluid/Renderer/GpuFluidForwardRenderBridge.h")
    assert "std::deque<RenderPacket>" in bridge
    assert "MaterialBlendMode::Transparent" in bridge
    assert "CalculateSortDepth" in bridge
    assert "GetActiveCameraPosition" in bridge
    assert "GetActiveCameraForward" in bridge
    assert "packet->renderer->Draw" in bridge


def test_manifest_build_and_docs_register_forward_renderer():
    manifest = read("Engine/Graphics/Shader/Manifest/GpuFluidShaderManifest.h")
    manifest_types = read("Engine/Graphics/Shader/Manifest/ShaderManifestTypes.h")
    props = read("Directory.Build.props")
    docs = read("Docs/Phase16GpuFluidDynamics.md")

    assert "GpuFluidGraphicsShaderId" in manifest
    assert "ForwardVS" in manifest
    assert "ForwardPS" in manifest
    assert "GpuFluidForward.VS.hlsl" in manifest
    assert "GpuFluidForward.PS.hlsl" in manifest
    assert "RootSignatureType::GpuFluid" in manifest
    assert "GpuFluid" in manifest_types
    assert "GpuFluidForwardRenderer.cpp" in props
    assert "GpuFluidForwardRenderBridge.h" in props
    assert "GpuFluidForward.VS.hlsl" in props
    assert "GpuFluidForward.PS.hlsl" in props
    assert "- [x] 16.9 Forward Rendering" in docs
