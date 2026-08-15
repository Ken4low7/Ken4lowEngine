from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_4_pressure_projection_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidPressureProjectionPass.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidPressureProjectionPass.cpp",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidDivergence.CS.hlsl",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidPressureJacobi.CS.hlsl",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidProjection.CS.hlsl",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_projection_pass_clears_pressure_then_dispatches_divergence_jacobi_projection():
    header = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidPressureProjectionPass.h"
    )
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidPressureProjectionPass.cpp"
    )

    assert "kThreadGroupSizeX = 8" in header
    assert "kThreadGroupSizeY = 8" in header
    assert "kThreadGroupSizeZ = 4" in header
    assert "gridDesc.depth != simulationDesc.grid.depth" in source
    assert "ClearPressure(commandList, grid)" in source
    assert "DispatchDivergence(commandList, grid" in source
    assert "DispatchPressureJacobi(" in source
    assert "DispatchProjection(commandList, grid" in source
    assert source.index("ClearPressure(commandList, grid)") < source.index("DispatchDivergence(commandList, grid")
    assert source.index("DispatchDivergence(commandList, grid") < source.index("DispatchPressureJacobi(")
    assert source.index("DispatchPressureJacobi(") < source.index("DispatchProjection(commandList, grid")
    assert "groupCountZ" in source
    assert "Dispatch(groupCountX, groupCountY, groupCountZ)" in source


def test_pressure_clear_resets_both_pingpong_generations():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidPressureProjectionPass.cpp"
    )

    assert "pressure.Reset()" in source
    assert "for (GpuVolumetricFluidTexture3D& texture : pressure.textures)" in source
    assert "ClearUnorderedAccessViewFloat" in source
    assert "GetClearCPUDescriptorHandle(texture.uavIndex)" in source
    assert "InsertUavBarrier(commandList, texture.resource.Get())" in source


def test_pressure_projection_root_contract_and_pingpong_barriers():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidPressureProjectionPass.cpp"
    )

    assert "D3D12_ROOT_PARAMETER rootParameters[4]" in source
    assert "D3D12_ROOT_PARAMETER_TYPE_CBV" in source
    assert "D3D12_DESCRIPTOR_RANGE srvRanges[2]" in source
    assert "D3D12_DESCRIPTOR_RANGE_TYPE_SRV" in source
    assert "D3D12_DESCRIPTOR_RANGE_TYPE_UAV" in source
    assert "divergence.computeSrvIndex" in source
    assert "read.computeSrvIndex" in source
    assert "pressure.computeSrvIndex" in source
    assert "velocityWrite.uavIndex" in source
    assert source.index("InsertUavBarrier(commandList, write.resource.Get())") < source.index("pressure.Swap()")
    assert source.index("InsertUavBarrier(commandList, velocityWrite.resource.Get())") < source.index("velocity.Swap()")
    assert "++dispatchCount_" in source
    assert "lastPressureIterationCount_ = simulationDesc.pressureIterations" in source


def test_3d_divergence_uses_six_velocity_neighbors_and_zero_outside_domain():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidDivergence.CS.hlsl"
    )

    assert "Texture3D<float4> gVelocity : register(t0)" in shader
    assert "RWTexture3D<float> gDivergence : register(u0)" in shader
    assert "[numthreads(8, 8, 4)]" in shader
    for name in [
        "velocityLeft", "velocityRight", "velocityBottom",
        "velocityTop", "velocityBack", "velocityFront",
    ]:
        assert name in shader
    assert "velocityRight.x - velocityLeft.x" in shader
    assert "velocityTop.y - velocityBottom.y" in shader
    assert "velocityFront.z - velocityBack.z" in shader
    assert "0.5f * gFluid.invCellSize" in shader


def test_3d_jacobi_uses_six_neighbor_poisson_stencil_and_neumann_outer_boundary():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidPressureJacobi.CS.hlsl"
    )

    assert "Texture3D<float> gDivergence : register(t0)" in shader
    assert "Texture3D<float> gPressureRead : register(t1)" in shader
    assert "RWTexture3D<float> gPressureWrite : register(u0)" in shader
    assert "const float centerPressure" in shader
    for name in [
        "pressureLeft = centerPressure", "pressureRight = centerPressure",
        "pressureBottom = centerPressure", "pressureTop = centerPressure",
        "pressureBack = centerPressure", "pressureFront = centerPressure",
    ]:
        assert name in shader
    assert "divergence * cellSizeSquared" in shader
    assert "/ 6.0f" in shader


def test_3d_projection_subtracts_xyz_pressure_gradient_and_closes_six_domain_faces():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidProjection.CS.hlsl"
    )

    assert "Texture3D<float4> gVelocityRead : register(t0)" in shader
    assert "Texture3D<float> gPressure : register(t1)" in shader
    assert "RWTexture3D<float4> gVelocityWrite : register(u0)" in shader
    assert "const float3 pressureGradient" in shader
    assert "pressureRight - pressureLeft" in shader
    assert "pressureTop - pressureBottom" in shader
    assert "pressureFront - pressureBack" in shader
    assert "projectedVelocity.x = 0.0f" in shader
    assert "projectedVelocity.y = 0.0f" in shader
    assert "projectedVelocity.z = 0.0f" in shader
    assert "dispatchThreadId.z + 1 >= gFluid.gridDepth" in shader
    assert "float4(projectedVelocity, 0.0f)" in shader


def test_manifest_build_and_docs_register_phase17_4():
    manifest = read("Engine/Graphics/Shader/Manifest/GpuVolumetricFluidShaderManifest.h")
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    assert "Divergence" in manifest
    assert "PressureJacobi" in manifest
    assert "Projection" in manifest
    assert "GpuVolumetricFluidDivergence.CS.hlsl" in manifest
    assert "GpuVolumetricFluidPressureJacobi.CS.hlsl" in manifest
    assert "GpuVolumetricFluidProjection.CS.hlsl" in manifest
    assert "GpuVolumetricFluidPressureProjectionPass.cpp" in props
    assert "GpuVolumetricFluidPressureProjectionPass.h" in props
    assert "GpuVolumetricFluidDivergence.CS.hlsl" in props
    assert "GpuVolumetricFluidPressureJacobi.CS.hlsl" in props
    assert "GpuVolumetricFluidProjection.CS.hlsl" in props
    assert "- [x] 17.4 3D Divergence / Pressure / Projection" in docs
    assert "## 17.4 3D Divergence / Pressure / Projection" in docs
