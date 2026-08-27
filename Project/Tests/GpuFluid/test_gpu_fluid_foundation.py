from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
TYPES_HEADER = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Data/GpuFluidTypes.h"
RESOURCE_HEADER = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Resource/GpuFluidGridResource.h"
RESOURCE_CPP = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Resource/GpuFluidGridResource.cpp"
SHADER_COMMON = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidCommon.hlsli"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_phase16_foundation_files_exist():
    assert TYPES_HEADER.exists()
    assert RESOURCE_HEADER.exists()
    assert RESOURCE_CPP.exists()
    assert SHADER_COMMON.exists()


def test_cpu_and_hlsl_constant_layout_keep_same_member_order():
    cpu = _read(TYPES_HEADER)
    hlsl = _read(SHADER_COMMON)
    members = [
        "gridWidth",
        "gridHeight",
        "invGridWidth",
        "invGridHeight",
        "cellSize",
        "invCellSize",
        "deltaTime",
        "elapsedTime",
        "velocityDissipation",
        "densityDissipation",
        "temperatureDissipation",
        "vorticityStrength",
        "ambientTemperature",
        "buoyancy",
        "smokeWeight",
        "padding",
    ]

    cpu_positions = [cpu.index(member) for member in members]
    hlsl_positions = [hlsl.index(member) for member in members]
    assert cpu_positions == sorted(cpu_positions)
    assert hlsl_positions == sorted(hlsl_positions)
    assert "static_assert(sizeof(GpuFluidSimulationConstants) == 64);" in cpu


def test_advected_fields_are_ping_pong_resources():
    header = _read(RESOURCE_HEADER)
    for field in ("velocity_", "pressure_", "density_", "temperature_"):
        assert f"GpuFluidPingPongField {field}" in header

    assert "GpuFluidTexture2D divergence_" in header
    assert "GpuFluidTexture2D obstacle_" in header


def test_grid_uses_expected_phase16_formats_and_barriers():
    source = _read(RESOURCE_CPP)
    assert "DXGI_FORMAT_R16G16_FLOAT" in source
    assert source.count("DXGI_FORMAT_R16_FLOAT") >= 4
    assert "DXGI_FORMAT_R8_UINT" in source
    assert "D3D12_RESOURCE_BARRIER_TYPE_TRANSITION" in source
    assert "D3D12_RESOURCE_BARRIER_TYPE_UAV" in source
