from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_foundation_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/GpuVolumetricFluidTypes.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Resource/GpuVolumetricFluidGridResource.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Resource/GpuVolumetricFluidGridResource.cpp",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidResetPass.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidResetPass.cpp",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidCommon.hlsli",
        "Docs/Phase17GpuVolumetricFluid.md",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_texture3d_descriptor_helpers_are_available_on_shared_heaps():
    srv_header = read("Engine/Graphics/Descriptor/SRV/SRVManager.h")
    srv_source = read("Engine/Graphics/Descriptor/SRV/SRVManager.cpp")
    uav_header = read("Engine/Graphics/Descriptor/UAV/UAVManager.h")
    uav_source = read("Engine/Graphics/Descriptor/UAV/UAVManager.cpp")

    assert "CreateSRVForTexture3D" in srv_header
    assert "D3D12_SRV_DIMENSION_TEXTURE3D" in srv_source
    assert "CreateSRVForTexture3DOnThisHeap" in uav_header
    assert "CreateUAVForTexture3D" in uav_header
    assert "D3D12_UAV_DIMENSION_TEXTURE3D" in uav_source
    assert "Texture3D.FirstWSlice = 0" in uav_source
    assert "Texture3D.WSize = depth" in uav_source
    assert "GetClearCPUDescriptorHandle(uavIndex)" in uav_source


def test_3d_grid_and_domain_contracts_are_bounded_and_oriented():
    types = read("Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/GpuVolumetricFluidTypes.h")

    assert "struct GpuVolumetricFluidGridDesc" in types
    assert "kMaxDimension = 256" in types
    assert "uint32_t width = 64" in types
    assert "uint32_t height = 64" in types
    assert "uint32_t depth = 64" in types
    assert "GetVoxelCount" in types
    assert "struct GpuVolumetricFluidDomainMapping" in types
    assert "axisU" in types
    assert "axisV" in types
    assert "axisW" in types
    assert "WorldToGrid" in types
    assert "GridToWorld" in types
    assert "WorldVelocityToFluid" in types
    assert "Vector3::Dot(u, v)" in types
    assert "Vector3::Dot(u, w)" in types
    assert "Vector3::Dot(v, w)" in types


def test_cpu_and_hlsl_constants_share_80_byte_3d_layout():
    types = read("Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/GpuVolumetricFluidTypes.h")
    shader = read("Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidCommon.hlsli")

    assert "struct alignas(16) GpuVolumetricFluidSimulationConstants" in types
    assert "static_assert(sizeof(GpuVolumetricFluidSimulationConstants) == 80)" in types
    assert "gridDepth" in types
    assert "invGridDepth" in types
    assert "struct GpuVolumetricFluidSimulationConstants" in shader
    assert "uint gridDepth" in shader
    assert "float invGridDepth" in shader
    assert "GpuVolumetricFluidCellToUvw" in shader
    assert "GpuVolumetricFluidClampUvwToCellCenters" in shader
    assert "GpuVolumetricFluidClampCell" in shader
    assert "GpuVolumetricFluidIsInsideGrid" in shader


def test_volume_grid_uses_native_texture3d_and_expected_formats():
    header = read("Engine/Graphics/Renderer/GpuFluid/Volumetric/Resource/GpuVolumetricFluidGridResource.h")
    source = read("Engine/Graphics/Renderer/GpuFluid/Volumetric/Resource/GpuVolumetricFluidGridResource.cpp")

    assert "GpuVolumetricFluidTexture3D" in header
    assert "GpuVolumetricFluidPingPongField" in header
    assert "D3D12_RESOURCE_DIMENSION_TEXTURE3D" in source
    assert "DepthOrArraySize = static_cast<UINT16>(gridDesc_.depth)" in source
    assert "DXGI_FORMAT_R16G16B16A16_FLOAT" in source
    assert "DXGI_FORMAT_R16_FLOAT" in source
    assert "DXGI_FORMAT_R8_UINT" in source
    assert "CreateSRVForTexture3D" in source
    assert "CreateSRVForTexture3DOnThisHeap" in source
    assert "CreateUAVForTexture3D" in source
    assert "velocity_" in header
    assert "pressure_" in header
    assert "density_" in header
    assert "temperature_" in header
    assert "divergence_" in header
    assert "vorticity_" in header
    assert "obstacle_" in header


def test_volume_memory_estimate_is_39_bytes_per_voxel():
    source = read("Engine/Graphics/Renderer/GpuFluid/Volumetric/Resource/GpuVolumetricFluidGridResource.cpp")

    assert "constexpr uint64_t kBytesPerVoxel" in source
    assert "(8ull * 2ull)" in source
    assert "(2ull * 2ull)" in source
    assert "8ull +" in source
    assert "1ull;" in source
    assert "GetVoxelCount() * kBytesPerVoxel" in source


def test_reset_pass_clears_all_generations_and_restores_pingpong_zero():
    reset = read("Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidResetPass.cpp")

    assert "clearPingPong(grid.GetVelocity())" in reset
    assert "clearPingPong(grid.GetPressure())" in reset
    assert "clearPingPong(grid.GetDensity())" in reset
    assert "clearPingPong(grid.GetTemperature())" in reset
    assert "ClearFloatTexture(commandList, grid.GetDivergence())" in reset
    assert "ClearFloatTexture(commandList, grid.GetVorticity())" in reset
    assert "ClearUintTexture(commandList, grid.GetObstacle())" in reset
    assert "ClearUnorderedAccessViewFloat" in reset
    assert "ClearUnorderedAccessViewUint" in reset
    assert "grid.GetVelocity().Reset()" in reset
    assert "grid.GetPressure().Reset()" in reset
    assert "grid.GetDensity().Reset()" in reset
    assert "grid.GetTemperature().Reset()" in reset


def test_phase17_build_registration_and_foundation_roadmap_are_present():
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    assert "GpuVolumetricFluidGridResource.cpp" in props
    assert "GpuVolumetricFluidResetPass.cpp" in props
    assert "GpuVolumetricFluidTypes.h" in props
    assert "GpuVolumetricFluidCommon.hlsli" in props
    assert "- [x] 17.1 3D base data / domain API" in docs
    assert "- [x] 17.2 Texture3D Grid / Resource / Reset foundation" in docs
