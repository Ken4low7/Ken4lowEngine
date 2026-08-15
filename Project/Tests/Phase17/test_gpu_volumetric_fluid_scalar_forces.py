from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_5_scalar_and_force_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidScalarAdvectionPass.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidScalarAdvectionPass.cpp",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidForcePass.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidForcePass.cpp",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidScalarAdvection.CS.hlsl",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidVorticityCurl.CS.hlsl",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidVorticityConfinement.CS.hlsl",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidBuoyancy.CS.hlsl",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_scalar_advection_reuses_xyz_velocity_and_swaps_after_barrier():
    header = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidScalarAdvectionPass.h"
    )
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidScalarAdvectionPass.cpp"
    )
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/"
        "GpuVolumetricFluidScalarAdvection.CS.hlsl"
    )

    assert "DispatchAll" in header
    assert "GetDensityDispatchCount" in header
    assert "GetTemperatureDispatchCount" in header
    assert "kThreadGroupSizeZ = 4" in header
    assert "GpuVolumetricFluidField::Density" in source
    assert "GpuVolumetricFluidField::Temperature" in source
    assert "simulationDesc.densityDissipation" in source
    assert "simulationDesc.temperatureDissipation" in source
    assert "SetComputeRoot32BitConstants(4, 4" in source
    assert "groupCountZ" in source
    assert source.index("InsertUavBarrier(commandList, scalarWrite.resource.Get())") < source.index("scalarField->Swap()")

    assert "Texture3D<float4> gVelocityRead : register(t0)" in shader
    assert "Texture3D<float> gScalarRead : register(t1)" in shader
    assert "RWTexture3D<float> gScalarWrite : register(u0)" in shader
    assert "SamplerState gLinearClampSampler : register(s0)" in shader
    assert "[numthreads(8, 8, 4)]" in shader
    assert "GpuVolumetricFluidCellToUvw" in shader
    assert "velocity * gFluid.deltaTime * gFluid.invCellSize" in shader
    assert "GpuVolumetricFluidClampUvwToCellCenters" in shader
    assert "advectedScalar * gScalarDissipation" in shader


def test_3d_curl_is_vector_centered_difference():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/"
        "GpuVolumetricFluidVorticityCurl.CS.hlsl"
    )

    assert "Texture3D<float4> gVelocityRead : register(t0)" in shader
    assert "RWTexture3D<float4> gVorticityWrite : register(u0)" in shader
    assert "[numthreads(8, 8, 4)]" in shader
    for name in [
        "velocityLeft", "velocityRight", "velocityBottom",
        "velocityTop", "velocityBack", "velocityFront",
    ]:
        assert name in shader
    assert "velocityTop.z - velocityBottom.z" in shader
    assert "velocityFront.y - velocityBack.y" in shader
    assert "velocityFront.x - velocityBack.x" in shader
    assert "velocityRight.z - velocityLeft.z" in shader
    assert "velocityRight.y - velocityLeft.y" in shader
    assert "velocityTop.x - velocityBottom.x" in shader
    assert "float4(curl, 0.0f)" in shader


def test_vorticity_confinement_uses_gradient_magnitude_cross_curl_and_closed_faces():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/"
        "GpuVolumetricFluidVorticityConfinement.CS.hlsl"
    )

    assert "Texture3D<float4> gVorticityRead : register(t1)" in shader
    assert "const float centerMagnitude = length(omega)" in shader
    assert "magnitudeRight - magnitudeLeft" in shader
    assert "magnitudeTop - magnitudeBottom" in shader
    assert "magnitudeFront - magnitudeBack" in shader
    assert "cross(normal, omega)" in shader
    assert "gFluid.vorticityStrength" in shader
    assert "confinementForce * gFluid.deltaTime" in shader
    assert "velocity.x = 0.0f" in shader
    assert "velocity.y = 0.0f" in shader
    assert "velocity.z = 0.0f" in shader


def test_buoyancy_uses_temperature_density_and_y_force():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/"
        "GpuVolumetricFluidBuoyancy.CS.hlsl"
    )

    assert "Texture3D<float4> gVelocityRead : register(t0)" in shader
    assert "Texture3D<float> gDensityRead : register(t1)" in shader
    assert "Texture3D<float> gTemperatureRead : register(t2)" in shader
    assert "gFluid.buoyancy * (temperature - gFluid.ambientTemperature)" in shader
    assert "gFluid.smokeWeight * density" in shader
    assert "velocity.y += buoyancyForce * gFluid.deltaTime" in shader
    assert "float4(velocity, 0.0f)" in shader


def test_force_pass_dispatches_curl_confinement_buoyancy_and_swaps_velocity():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidForcePass.cpp"
    )

    assert "DispatchCurl(commandList, grid" in source
    assert "DispatchVorticityConfinement(commandList, grid" in source
    assert "DispatchBuoyancyInternal(commandList, grid" in source
    assert source.index("DispatchCurl(commandList, grid") < source.index("DispatchVorticityConfinement(commandList, grid")
    assert source.index("DispatchVorticityConfinement(commandList, grid") < source.index("DispatchBuoyancyInternal(commandList, grid")
    assert "grid.GetVorticity()" in source
    assert "grid.GetDensity().Read()" in source
    assert "grid.GetTemperature().Read()" in source
    assert source.count("velocity.Swap()") >= 2
    assert "groupCountZ" in source
    assert "++dispatchCount_" in source


def test_manifest_build_and_docs_register_phase17_5():
    manifest = read("Engine/Graphics/Shader/Manifest/GpuVolumetricFluidShaderManifest.h")
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    for shader_id in [
        "ScalarAdvection", "VorticityCurl", "VorticityConfinement", "Buoyancy",
    ]:
        assert shader_id in manifest
    for name in [
        "GpuVolumetricFluidScalarAdvectionPass.cpp",
        "GpuVolumetricFluidForcePass.cpp",
        "GpuVolumetricFluidScalarAdvection.CS.hlsl",
        "GpuVolumetricFluidVorticityCurl.CS.hlsl",
        "GpuVolumetricFluidVorticityConfinement.CS.hlsl",
        "GpuVolumetricFluidBuoyancy.CS.hlsl",
    ]:
        assert name in props
    assert "- [x] 17.5 3D Density / Temperature / Vorticity / Buoyancy" in docs
    assert "## 17.5 3D Density / Temperature / Vorticity / Buoyancy" in docs
    assert "## Next implementation target — 17.6" in docs
