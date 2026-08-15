from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
TYPES = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Data/GpuFluidTypes.h"
GRID_H = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Resource/GpuFluidGridResource.h"
GRID_CPP = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Resource/GpuFluidGridResource.cpp"
PASS_H = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidForcePass.h"
PASS_CPP = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidForcePass.cpp"
MANIFEST = PROJECT_ROOT / "Engine/Graphics/Shader/Manifest/GpuFluidShaderManifest.h"
CURL = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidVorticityCurl.CS.hlsl"
CONFINE = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidVorticityConfinement.CS.hlsl"
BUOYANCY = PROJECT_ROOT / "Resources/Shaders/GpuFluid/GpuFluidBuoyancy.CS.hlsl"
PROPS = PROJECT_ROOT / "Directory.Build.props"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_phase16_force_files_exist():
    for path in (PASS_H, PASS_CPP, CURL, CONFINE, BUOYANCY):
        assert path.exists()


def test_vorticity_has_dedicated_r16_field():
    types = _read(TYPES)
    header = _read(GRID_H)
    source = _read(GRID_CPP)
    assert "Vorticity," in types
    assert "GetVorticity()" in header
    assert "GpuFluidTexture2D vorticity_" in header
    assert 'CreateTexture(vorticity_, DXGI_FORMAT_R16_FLOAT, L"GpuFluid.Vorticity")' in source
    assert "2ull +          // Vorticity R16F" in source


def test_force_manifest_registers_three_compute_shaders():
    manifest = _read(MANIFEST)
    for shader_id in ("VorticityCurl", "VorticityConfinement", "Buoyancy"):
        assert f"GpuFluidComputeShaderId::{shader_id}" in manifest
    assert "GpuFluidVorticityCurl.CS.hlsl" in manifest
    assert "GpuFluidVorticityConfinement.CS.hlsl" in manifest
    assert "GpuFluidBuoyancy.CS.hlsl" in manifest


def test_curl_and_confinement_use_centered_neighbors():
    curl = _read(CURL)
    confinement = _read(CONFINE)
    assert "velocityRight.y - velocityLeft.y" in curl
    assert "velocityTop.x - velocityBottom.x" in curl
    assert "0.5f * gFluid.invCellSize" in curl
    assert "omegaRight - omegaLeft" in confinement
    assert "omegaTop - omegaBottom" in confinement
    assert "gFluid.vorticityStrength" in confinement
    assert "gFluid.deltaTime" in confinement


def test_buoyancy_uses_temperature_and_density_feedback():
    shader = _read(BUOYANCY)
    assert "temperature - gFluid.ambientTemperature" in shader
    assert "gFluid.smokeWeight * density" in shader
    assert "velocity.y += buoyancyForce * gFluid.deltaTime" in shader


def test_force_pass_writes_velocity_through_ping_pong_and_can_dispatch_all():
    source = _read(PASS_CPP)
    assert "DispatchCurl(commandList, grid" in source
    assert "DispatchVorticityConfinement(commandList, grid" in source
    assert "DispatchBuoyancyInternal(commandList, grid" in source
    assert source.count("velocity.Swap();") >= 2
    assert source.count("InsertUavBarrier") >= 3
    assert "GetDensity().Read()" in source
    assert "GetTemperature().Read()" in source


def test_build_props_register_force_sources_and_shaders():
    props = _read(PROPS)
    assert "GpuFluidForcePass.cpp" in props
    assert "GpuFluidForcePass.h" in props
    assert "GpuFluidVorticityCurl.CS.hlsl" in props
    assert "GpuFluidVorticityConfinement.CS.hlsl" in props
    assert "GpuFluidBuoyancy.CS.hlsl" in props
