from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
RESOURCE_HEADER = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Resource/GpuFluidGridResource.h"
RESOURCE_CPP = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Resource/GpuFluidGridResource.cpp"
PASS_HEADER = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidVelocityAdvectionPass.h"
PASS_CPP = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidVelocityAdvectionPass.cpp"
SHADER = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidVelocityAdvection.CS.hlsl"
SHADER_COMMON = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidCommon.hlsli"
MANIFEST = PROJECT_ROOT / "Engine/Graphics/Shader/Manifest/GpuFluidShaderManifest.h"
BUILD_PROPS = PROJECT_ROOT / "Directory.Build.props"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_phase16_3_velocity_advection_files_exist():
    for path in (PASS_HEADER, PASS_CPP, SHADER, SHADER_COMMON, MANIFEST, BUILD_PROPS):
        assert path.exists(), path


def test_compute_read_srv_and_write_uav_share_uav_manager_heap():
    header = _read(RESOURCE_HEADER)
    resource_cpp = _read(RESOURCE_CPP)
    pass_cpp = _read(PASS_CPP)

    assert "computeSrvIndex" in header
    assert "CreateSRVForTexture2DOnThisHeap" in resource_cpp
    assert "GetGPUDescriptorHandle(read.computeSrvIndex)" in pass_cpp
    assert "GetGPUDescriptorHandle(write.uavIndex)" in pass_cpp
    assert "descriptorManager->PreDispatch();" in pass_cpp


def test_velocity_shader_uses_semi_lagrangian_backtrace_and_dissipation():
    shader = _read(SHADER)
    common = _read(SHADER_COMMON)

    assert "[numthreads(8, 8, 1)]" in shader
    assert "currentVelocity * gFluid.deltaTime * gFluid.invCellSize" in shader
    assert "uv - backtraceUvOffset" in shader
    assert "GpuFluidClampUvToCellCenters" in shader
    assert "advectedVelocity * gFluid.velocityDissipation" in shader
    assert "float2 GpuFluidClampUvToCellCenters" in common


def test_velocity_pass_transitions_barriers_dispatches_and_swaps():
    source = _read(PASS_CPP)

    assert "D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE" in source
    assert "D3D12_RESOURCE_STATE_UNORDERED_ACCESS" in source
    assert "SetComputeRootConstantBufferView(0" in source
    assert "SetComputeRootDescriptorTable(" in source
    assert "commandList->Dispatch(groupCountX, groupCountY, 1);" in source
    assert "GpuFluidGridResource::InsertUavBarrier" in source
    assert "velocity.Swap();" in source


def test_velocity_shader_is_registered_in_manifest():
    manifest = _read(MANIFEST)

    assert "GpuFluidComputeShaderId::VelocityAdvection" in manifest
    assert "GpuFluidVelocityAdvection.CS.hlsl" in manifest
    assert "L\"cs_6_0\"" in manifest
    assert "RootSignatureType::Compute" in manifest


def test_phase16_cpp_files_are_registered_for_main_engine_build():
    props = _read(BUILD_PROPS)

    assert "'$(MSBuildProjectName)' == 'Ken4lowEngine'" in props
    assert "GpuFluidGridResource.cpp" in props
    assert "GpuFluidVelocityAdvectionPass.cpp" in props
