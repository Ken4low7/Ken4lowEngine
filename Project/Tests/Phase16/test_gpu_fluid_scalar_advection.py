from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PASS_HEADER = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidScalarAdvectionPass.h"
PASS_CPP = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidScalarAdvectionPass.cpp"
SHADER = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidScalarAdvection.CS.hlsl"
MANIFEST = PROJECT_ROOT / "Engine/Graphics/Shader/Manifest/GpuFluidShaderManifest.h"
BUILD_PROPS = PROJECT_ROOT / "Directory.Build.props"
DOC = PROJECT_ROOT / "Docs/Phase16GpuFluidDynamics.md"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_phase16_scalar_advection_files_exist():
    assert PASS_HEADER.exists()
    assert PASS_CPP.exists()
    assert SHADER.exists()


def test_scalar_shader_uses_projected_velocity_and_generic_scalar_contract():
    shader = _read(SHADER)
    assert "Texture2D<float2> gVelocityRead : register(t0);" in shader
    assert "Texture2D<float> gScalarRead : register(t1);" in shader
    assert "RWTexture2D<float> gScalarWrite : register(u0);" in shader
    assert "cbuffer ScalarAdvectionCB : register(b1)" in shader
    assert "gScalarDissipation" in shader
    assert "GpuFluidClampUvToCellCenters" in shader
    assert "[numthreads(8, 8, 1)]" in shader


def test_scalar_pass_reuses_density_and_temperature_ping_pong_fields():
    source = _read(PASS_CPP)
    assert "case GpuFluidField::Density:" in source
    assert "return &grid.GetDensity();" in source
    assert "case GpuFluidField::Temperature:" in source
    assert "return &grid.GetTemperature();" in source
    assert "simulationDesc.densityDissipation" in source
    assert "simulationDesc.temperatureDissipation" in source
    assert "scalarField->Swap();" in source


def test_scalar_pass_uses_root_constants_instead_of_second_upload_cb():
    source = _read(PASS_CPP)
    assert "D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS" in source
    assert "Constants.ShaderRegister = 1" in source
    assert "Constants.Num32BitValues = 4" in source
    assert "SetComputeRoot32BitConstants(4, 4, scalarConstants, 0);" in source


def test_dispatch_all_transports_both_scalar_fields():
    header = _read(PASS_HEADER)
    source = _read(PASS_CPP)
    assert "bool DispatchAll(" in header
    assert "GpuFluidField::Density" in source
    assert "GpuFluidField::Temperature" in source
    assert "GetDensityDispatchCount" in header
    assert "GetTemperatureDispatchCount" in header


def test_scalar_advection_is_registered_once_in_shader_manifest():
    manifest = _read(MANIFEST)
    assert "ScalarAdvection" in manifest
    assert "GpuFluidScalarAdvection.CS.hlsl" in manifest
    assert manifest.count("GpuFluidScalarAdvection.CS.hlsl") == 1


def test_scalar_pass_and_shader_are_registered_in_build_graph():
    props = _read(BUILD_PROPS)
    assert "GpuFluidScalarAdvectionPass.cpp" in props
    assert "GpuFluidScalarAdvectionPass.h" in props
    assert "GpuFluidScalarAdvection.CS.hlsl" in props


def test_phase16_document_marks_scalar_advection_complete():
    doc = _read(DOC)
    assert "[x] 16.5 Density / Temperature" in doc
    assert "GpuFluidScalarAdvectionPass" in doc
    assert "DispatchAll()" in doc
    assert "Next implementation target — 16.6" in doc
