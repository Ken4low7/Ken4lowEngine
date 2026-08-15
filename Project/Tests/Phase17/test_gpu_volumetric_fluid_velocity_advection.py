from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_3_velocity_advection_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidVelocityAdvectionPass.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidVelocityAdvectionPass.cpp",
        "Engine/Graphics/Shader/Manifest/GpuVolumetricFluidShaderManifest.h",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidVelocityAdvection.CS.hlsl",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_velocity_advection_dispatches_xyz_and_swaps_after_uav_barrier():
    header = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidVelocityAdvectionPass.h"
    )
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidVelocityAdvectionPass.cpp"
    )

    assert "kThreadGroupSizeX = 8" in header
    assert "kThreadGroupSizeY = 8" in header
    assert "kThreadGroupSizeZ = 4" in header
    assert "gridDesc.depth != simulationDesc.grid.depth" in source
    assert "groupCountZ" in source
    assert "Dispatch(groupCountX, groupCountY, groupCountZ)" in source
    assert "D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE" in source
    assert "D3D12_RESOURCE_STATE_UNORDERED_ACCESS" in source
    assert "InsertUavBarrier(commandList, write.resource.Get())" in source
    assert source.index("InsertUavBarrier(commandList, write.resource.Get())") < source.index("velocity.Swap()")
    assert "++dispatchCount_" in source


def test_velocity_advection_root_contract_is_cbv_srv_uav_with_linear_clamp_sampler():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidVelocityAdvectionPass.cpp"
    )

    assert "D3D12_ROOT_PARAMETER rootParameters[3]" in source
    assert "D3D12_ROOT_PARAMETER_TYPE_CBV" in source
    assert "D3D12_DESCRIPTOR_RANGE_TYPE_SRV" in source
    assert "D3D12_DESCRIPTOR_RANGE_TYPE_UAV" in source
    assert "D3D12_FILTER_MIN_MAG_MIP_LINEAR" in source
    assert "sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP" in source
    assert "sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP" in source
    assert "sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP" in source
    assert "read.computeSrvIndex" in source
    assert "write.uavIndex" in source


def test_velocity_shader_uses_texture3d_semi_lagrangian_trilinear_sampling():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/"
        "GpuVolumetricFluidVelocityAdvection.CS.hlsl"
    )

    assert "Texture3D<float4> gVelocityRead : register(t0)" in shader
    assert "RWTexture3D<float4> gVelocityWrite : register(u0)" in shader
    assert "SamplerState gLinearClampSampler : register(s0)" in shader
    assert "[numthreads(8, 8, 4)]" in shader
    assert "dispatchThreadId.z >= gFluid.gridDepth" in shader
    assert "GpuVolumetricFluidCellToUvw" in shader
    assert "currentVelocity * gFluid.deltaTime * gFluid.invCellSize" in shader
    assert "GpuVolumetricFluidClampUvwToCellCenters" in shader
    assert "SampleLevel(gLinearClampSampler, sourceUvw, 0.0f)" in shader
    assert "advectedVelocity * gFluid.velocityDissipation" in shader
    assert "float4(" in shader


def test_volumetric_manifest_build_and_docs_register_velocity_advection():
    manifest = read("Engine/Graphics/Shader/Manifest/GpuVolumetricFluidShaderManifest.h")
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    assert "GpuVolumetricFluidComputeShaderId" in manifest
    assert "VelocityAdvection = 0" in manifest
    assert "GpuVolumetricFluidVelocityAdvectionCS" in manifest
    assert "GpuVolumetricFluidVelocityAdvection.CS.hlsl" in manifest
    assert "RootSignatureType::Compute" in manifest
    assert "GpuVolumetricFluidVelocityAdvectionPass.cpp" in props
    assert "GpuVolumetricFluidVelocityAdvectionPass.h" in props
    assert "GpuVolumetricFluidShaderManifest.h" in props
    assert "GpuVolumetricFluidVelocityAdvection.CS.hlsl" in props
    assert "- [x] 17.3 3D Velocity Advection" in docs
    assert "## 17.3 3D Velocity Advection" in docs
