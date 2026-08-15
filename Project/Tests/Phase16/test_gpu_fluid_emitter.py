from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_emitter_component_contract():
    header = read("Engine/Scene/Actor/Components/FluidEmitterComponent.h")
    source = read("Engine/Scene/Actor/Components/FluidEmitterComponent.cpp")

    assert "class FluidEmitterComponent : public SceneComponent" in header
    assert "BuildEmitterSource() const" in header
    assert "ComponentPropertyUtility::ToJson" in source
    assert "ComponentPropertyUtility::FromJson" in source
    assert "RegisterComponentType" in source
    for property_name in (
        "EmissionEnabled",
        "Radius",
        "SourceVelocity",
        "VelocityStrength",
        "DensityRate",
        "TemperatureRate",
        "FalloffExponent",
    ):
        assert property_name in source


def test_emitter_cpu_mapping_and_gpu_layout():
    emitter_types = read("Engine/Graphics/Renderer/GpuFluid/Data/GpuFluidEmitterTypes.h")

    assert "struct GpuFluidDomainMapping" in emitter_types
    assert "WorldToGrid" in emitter_types
    assert "WorldVelocityToFluid" in emitter_types
    assert "struct GpuFluidEmitterSource" in emitter_types
    assert "struct alignas(16) GpuFluidEmitterGpuData" in emitter_types
    assert "static_assert(sizeof(GpuFluidEmitterGpuData) == 48)" in emitter_types
    assert "完全にGrid外のSourceはUpload配列へ入れず" in emitter_types


def test_emitter_injection_batches_sources_and_ping_pongs():
    source = read("Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidEmitterInjectionPass.cpp")

    assert "std::vector<GpuFluidEmitterGpuData> activeSources" in source
    assert "uploadArena.Allocate(emitterBytes" in source
    assert "std::memcpy(emitterAllocation.cpuAddress" in source
    assert "SetComputeRootShaderResourceView(5, emitterAllocation.gpuAddress)" in source
    assert "SetComputeRoot32BitConstants(1, 4" in source
    assert "grid.GetVelocity()" in source
    assert "grid.GetDensity()" in source
    assert "grid.GetTemperature()" in source
    assert "D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE" in source
    assert "D3D12_RESOURCE_STATE_UNORDERED_ACCESS" in source
    assert "velocity.Swap()" in source
    assert "density.Swap()" in source
    assert "temperature.Swap()" in source
    assert source.count("commandList->Dispatch(") == 1


def test_emitter_shader_contract():
    shader = read("Resources/Shaders/GpuFluid/GpuFluidEmitterInjection.CS.hlsl")

    assert "register(b0)" in shader
    assert "register(b1)" in shader
    assert "register(t0)" in shader
    assert "register(t1)" in shader
    assert "register(t2)" in shader
    assert "register(t3)" in shader
    assert "register(u0)" in shader
    assert "register(u1)" in shader
    assert "register(u2)" in shader
    assert "StructuredBuffer<GpuFluidEmitterGpuData>" in shader
    assert "for (uint emitterIndex = 0; emitterIndex < gEmitterCount; ++emitterIndex)" in shader
    assert "gFluid.deltaTime" in shader
    assert "gVelocityWrite[cell] = velocity" in shader
    assert "gDensityWrite[cell] = density" in shader
    assert "gTemperatureWrite[cell] = temperature" in shader


def test_emitter_build_and_manifest_registration():
    props = read("Directory.Build.props")
    manifest = read("Engine/Graphics/Shader/Manifest/GpuFluidShaderManifest.h")
    docs = read("Docs/Phase16GpuFluidDynamics.md")

    assert "GpuFluidEmitterInjectionPass.cpp" in props
    assert "FluidEmitterComponent.cpp" in props
    assert "GpuFluidEmitterTypes.h" in props
    assert "GpuFluidEmitterInjection.CS.hlsl" in props
    assert "EmitterInjection" in manifest
    assert "GpuFluidEmitterInjection.CS.hlsl" in manifest
    assert "[x] 16.7 FluidEmitterComponent" in docs
