from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_6_emitter_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/GpuVolumetricFluidEmitterTypes.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidEmitterInjectionPass.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidEmitterInjectionPass.cpp",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidEmitterInjection.CS.hlsl",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_emitter_gpu_data_is_64_bytes_and_world_source_maps_to_xyz_grid():
    types = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/"
        "GpuVolumetricFluidEmitterTypes.h"
    )

    assert "struct GpuVolumetricFluidEmitterSource" in types
    assert "struct alignas(16) GpuVolumetricFluidEmitterGpuData" in types
    assert "static_assert(sizeof(GpuVolumetricFluidEmitterGpuData) == 64)" in types
    assert "domain.WorldToGrid(source.worldPosition, grid.cellSize)" in types
    assert "domain.WorldVelocityToFluid(source.worldVelocity)" in types
    assert "centerCellZ" in types
    assert "velocityZ" in types
    assert "source.radius / grid.cellSize" in types
    assert "center.z + radiusCells < 0.0f" in types
    assert "center.z - radiusCells > static_cast<float>(grid.depth)" in types
    assert "outData.invRadiusCells = 1.0f / radiusCells" in types


def test_emitter_pass_culls_sources_caps_batch_and_uploads_structured_data():
    header = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidEmitterInjectionPass.h"
    )
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidEmitterInjectionPass.cpp"
    )

    assert "kMaxSourcesPerDispatch = 256" in header
    assert "GetLastInjectedSourceCount" in header
    assert "GetLastCulledSourceCount" in header
    assert "BuildGpuVolumetricFluidEmitterGpuData" in source
    assert "activeSources.size() >= kMaxSourcesPerDispatch" in source
    assert "++lastCulledSourceCount_" in source
    assert "uploadArena.Allocate(emitterBytes, alignof(GpuVolumetricFluidEmitterGpuData))" in source
    assert "std::memcpy(emitterAllocation.cpuAddress, activeSources.data(), emitterBytes)" in source
    assert "SetComputeRootShaderResourceView(5, emitterAllocation.gpuAddress)" in source


def test_emitter_dispatch_updates_three_texture3d_fields_in_one_xyz_dispatch():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidEmitterInjectionPass.cpp"
    )

    assert "grid.GetVelocity()" in source
    assert "grid.GetDensity()" in source
    assert "grid.GetTemperature()" in source
    assert "kThreadGroupSizeZ = 4" in read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidEmitterInjectionPass.h"
    )
    assert "groupCountZ" in source
    assert "Dispatch(groupCountX, groupCountY, groupCountZ)" in source
    for field in ["velocityWrite", "densityWrite", "temperatureWrite"]:
        assert f"InsertUavBarrier(commandList, {field}.resource.Get())" in source
    first_swap = source.index("velocity.Swap()")
    for field in ["velocityWrite", "densityWrite", "temperatureWrite"]:
        assert source.index(f"InsertUavBarrier(commandList, {field}.resource.Get())") < first_swap
    assert "velocity.Swap()" in source
    assert "density.Swap()" in source
    assert "temperature.Swap()" in source


def test_emitter_shader_uses_spherical_falloff_and_injects_velocity_density_temperature():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/"
        "GpuVolumetricFluidEmitterInjection.CS.hlsl"
    )

    assert "StructuredBuffer<GpuVolumetricFluidEmitterGpuData> gEmitters : register(t3)" in shader
    assert "Texture3D<float4> gVelocityRead : register(t0)" in shader
    assert "Texture3D<float> gDensityRead : register(t1)" in shader
    assert "Texture3D<float> gTemperatureRead : register(t2)" in shader
    assert "RWTexture3D<float4> gVelocityWrite : register(u0)" in shader
    assert "RWTexture3D<float> gDensityWrite : register(u1)" in shader
    assert "RWTexture3D<float> gTemperatureWrite : register(u2)" in shader
    assert "[numthreads(8, 8, 4)]" in shader
    assert "const float3 cellCenter = float3(dispatchThreadId) + 0.5f" in shader
    assert "length(cellCenter - emitterCenter)" in shader
    assert "pow(" in shader
    assert "gFluid.deltaTime * falloff" in shader
    assert "velocity += float3(emitter.velocityX, emitter.velocityY, emitter.velocityZ)" in shader
    assert "density += emitter.densityRate * sourceDelta" in shader
    assert "temperature += emitter.temperatureRate * sourceDelta" in shader


def test_fluid_emitter_component_reuses_same_scene_settings_for_volumetric_source():
    header = read("Engine/Scene/Actor/Components/FluidEmitterComponent.h")
    source = read("Engine/Scene/Actor/Components/FluidEmitterComponent.cpp")

    assert "GpuVolumetricFluidEmitterSource" in header
    assert "BuildVolumetricEmitterSource() const" in header
    assert "GpuVolumetricFluidEmitterSource FluidEmitterComponent::BuildVolumetricEmitterSource() const" in source
    assert "source.worldPosition = GetWorldPosition()" in source
    assert source.count("source.worldVelocity = sourceVelocity_") >= 2
    assert source.count("source.radius = radius_") >= 2
    assert "2D or 3D fluid domains" in source


def test_manifest_build_and_docs_register_phase17_6():
    manifest = read("Engine/Graphics/Shader/Manifest/GpuVolumetricFluidShaderManifest.h")
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    assert "EmitterInjection" in manifest
    assert "GpuVolumetricFluidEmitterInjection.CS.hlsl" in manifest
    for name in [
        "GpuVolumetricFluidEmitterTypes.h",
        "GpuVolumetricFluidEmitterInjectionPass.cpp",
        "GpuVolumetricFluidEmitterInjectionPass.h",
        "GpuVolumetricFluidEmitterInjection.CS.hlsl",
    ]:
        assert name in props
    assert "- [x] 17.6 3D Emitter injection" in docs
    assert "## 17.6 3D Emitter injection" in docs
