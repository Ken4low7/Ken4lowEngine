from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PASS_HEADER = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidPressureProjectionPass.h"
PASS_CPP = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidPressureProjectionPass.cpp"
COMMON_SHADER = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidCommon.hlsli"
DIVERGENCE_SHADER = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidDivergence.CS.hlsl"
PRESSURE_SHADER = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidPressureJacobi.CS.hlsl"
PROJECTION_SHADER = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidProjection.CS.hlsl"
MANIFEST = PROJECT_ROOT / "Engine/Graphics/Shader/Manifest/GpuFluidShaderManifest.h"
BUILD_PROPS = PROJECT_ROOT / "Directory.Build.props"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_phase16_pressure_projection_files_exist():
    for path in (
        PASS_HEADER,
        PASS_CPP,
        COMMON_SHADER,
        DIVERGENCE_SHADER,
        PRESSURE_SHADER,
        PROJECTION_SHADER,
        MANIFEST,
        BUILD_PROPS,
    ):
        assert path.exists()


def test_manifest_registers_all_pressure_projection_compute_shaders():
    manifest = _read(MANIFEST)
    for shader_id in ("Divergence", "PressureJacobi", "Projection"):
        assert f"GpuFluidComputeShaderId::{shader_id}" in manifest

    for shader_file in (
        "GpuFluidDivergence.CS.hlsl",
        "GpuFluidPressureJacobi.CS.hlsl",
        "GpuFluidProjection.CS.hlsl",
    ):
        assert shader_file in manifest


def test_divergence_uses_centered_velocity_difference():
    shader = _read(DIVERGENCE_SHADER)
    assert "velocityRight.x - velocityLeft.x" in shader
    assert "velocityTop.y - velocityBottom.y" in shader
    assert "0.5f * gFluid.invCellSize" in shader
    assert "GpuFluidClampCell" in shader
    assert "Texture2D<uint> gObstacle : register(t2)" in shader


def test_pressure_jacobi_uses_poisson_stencil():
    shader = _read(PRESSURE_SHADER)
    assert "pressureLeft + pressureRight + pressureBottom + pressureTop" in shader
    assert "divergence * cellSizeSquared" in shader
    assert "* 0.25f" in shader
    assert "Texture2D<float> gDivergence : register(t0);" in shader
    assert "Texture2D<float> gPressureRead : register(t1);" in shader
    assert "Texture2D<uint> gObstacle : register(t2);" in shader
    assert "RWTexture2D<float> gPressureWrite : register(u0);" in shader


def test_projection_subtracts_pressure_gradient_and_closes_domain_boundary():
    shader = _read(PROJECTION_SHADER)
    assert "projectedVelocity = gVelocityRead.Load(int3(cell, 0)) - pressureGradient" in shader
    assert "projectedVelocity.x = 0.0f" in shader
    assert "projectedVelocity.y = 0.0f" in shader
    assert "pressureRight - pressureLeft" in shader
    assert "pressureTop - pressureBottom" in shader
    assert "solidLeft" in shader
    assert "solidTop" in shader


def test_pressure_pass_clears_pressure_iterates_and_swaps_velocity():
    source = _read(PASS_CPP)
    assert "ClearUnorderedAccessViewFloat" in source
    assert "GetClearCPUDescriptorHandle" in source
    assert "for (uint32_t iteration = 0; iteration < iterationCount; ++iteration)" in source
    assert "pressure.Swap();" in source
    assert "velocity.Swap();" in source
    assert "simulationDesc.pressureIterations" in source
    assert source.count("InsertUavBarrier") >= 4
    assert "GetObstacle()" in source


def test_pressure_projection_uses_one_shared_root_signature_contract():
    source = _read(PASS_CPP)
    assert "D3D12_ROOT_PARAMETER rootParameters[5]" in source
    assert "D3D12_DESCRIPTOR_RANGE srvRanges[3]" in source
    assert "BaseShaderRegister = i" in source
    assert "D3D12_DESCRIPTOR_RANGE_TYPE_UAV" in source
    assert "SetComputeRootSignature(rootSignature_.Get())" in source


def test_common_shader_exposes_clamped_neighbor_helper():
    common = _read(COMMON_SHADER)
    assert "int2 GpuFluidClampCell" in common
    assert "int(fluid.gridWidth) - 1" in common
    assert "int(fluid.gridHeight) - 1" in common


def test_directory_build_props_registers_phase16_4_sources():
    props = _read(BUILD_PROPS)
    assert "GpuFluidPressureProjectionPass.cpp" in props
    assert "GpuFluidPressureProjectionPass.h" in props
    assert "GpuFluidDivergence.CS.hlsl" in props
    assert "GpuFluidPressureJacobi.CS.hlsl" in props
    assert "GpuFluidProjection.CS.hlsl" in props
