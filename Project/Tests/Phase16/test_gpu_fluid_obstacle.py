from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase16_8_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Data/GpuFluidObstacleTypes.h",
        "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidObstacleRasterPass.h",
        "Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidObstacleRasterPass.cpp",
        "Engine/Scene/Actor/Components/GpuFluidColliderObstacleAdapter.h",
        "Engine/Scene/Actor/Components/GpuFluidColliderObstacleAdapter.cpp",
        "Resources/Shaders/GpuFluid/GpuFluidObstacleRaster.CS.hlsl",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_obstacle_gpu_contract_and_adapter():
    types = read("Engine/Graphics/Renderer/GpuFluid/Data/GpuFluidObstacleTypes.h")
    adapter = read("Engine/Scene/Actor/Components/GpuFluidColliderObstacleAdapter.cpp")
    assert "static_assert(sizeof(GpuFluidObstacleGpuData) == 96)" in types
    assert "static_assert(sizeof(GpuFluidObstacleRasterConstants) == 48)" in types
    assert "ECollisionShapeType::Sphere" in adapter
    assert "ECollisionShapeType::AABB" in adapter
    assert "ECollisionShapeType::OBB" in adapter
    assert "IsCollisionEnabledForPhysics()" in adapter
    assert "CollectSources" in adapter


def test_obstacle_raster_rebuilds_mask():
    cpp = read("Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidObstacleRasterPass.cpp")
    shader = read("Resources/Shaders/GpuFluid/GpuFluidObstacleRaster.CS.hlsl")
    assert "ClearUnorderedAccessViewUint" in cpp
    assert "BuildGpuFluidObstacleRasterConstants" in cpp
    assert "SetComputeRootShaderResourceView" in cpp
    assert "GetObstacle()" in cpp
    assert "StructuredBuffer<GpuFluidObstacleGpuData>" in shader
    assert "RWTexture2D<uint> gObstacleMask" in shader
    assert "IsInsideObstacle" in shader
    assert "gObstacleMask[dispatchThreadId.xy] = solid" in shader


def test_pressure_projection_respects_obstacles():
    cpp = read("Engine/Graphics/Renderer/GpuFluid/Pass/GpuFluidPressureProjectionPass.cpp")
    divergence = read("Resources/Shaders/GpuFluid/GpuFluidDivergence.CS.hlsl")
    jacobi = read("Resources/Shaders/GpuFluid/GpuFluidPressureJacobi.CS.hlsl")
    projection = read("Resources/Shaders/GpuFluid/GpuFluidProjection.CS.hlsl")
    assert "GetObstacle()" in cpp
    assert "Texture2D<uint> gObstacle : register(t2)" in divergence
    assert "gDivergence[dispatchThreadId.xy] = 0.0f" in divergence
    assert "centerPressure" in jacobi
    assert "solidLeft ? centerPressure" in jacobi
    assert "projectedVelocity.x = 0.0f" in projection
    assert "projectedVelocity.y = 0.0f" in projection


def test_advection_force_and_emitter_use_mask():
    velocity = read("Resources/Shaders/GpuFluid/GpuFluidVelocityAdvection.CS.hlsl")
    scalar = read("Resources/Shaders/GpuFluid/GpuFluidScalarAdvection.CS.hlsl")
    curl = read("Resources/Shaders/GpuFluid/GpuFluidVorticityCurl.CS.hlsl")
    confinement = read("Resources/Shaders/GpuFluid/GpuFluidVorticityConfinement.CS.hlsl")
    buoyancy = read("Resources/Shaders/GpuFluid/GpuFluidBuoyancy.CS.hlsl")
    emitter = read("Resources/Shaders/GpuFluid/GpuFluidEmitterInjection.CS.hlsl")
    assert "Texture2D<uint> gObstacle" in velocity
    assert "sourceUv = uv" in velocity
    assert "Texture2D<uint> gObstacle" in scalar
    assert "gScalarWrite[cell] = 0.0f" in scalar
    assert "Texture2D<uint> gObstacle : register(t3)" in curl
    assert "Texture2D<uint> gObstacle : register(t3)" in confinement
    assert "Texture2D<uint> gObstacle : register(t3)" in buoyancy
    assert "Texture2D<uint> gObstacle : register(t4)" in emitter
    assert "gDensityWrite[cell] = 0.0f" in emitter


def test_build_manifest_and_docs_mark_phase_complete():
    props = read("Directory.Build.props")
    manifest = read("Engine/Graphics/Shader/Manifest/GpuFluidShaderManifest.h")
    docs = read("Docs/Phase16GpuFluidDynamics.md")
    assert "GpuFluidObstacleRasterPass.cpp" in props
    assert "GpuFluidColliderObstacleAdapter.cpp" in props
    assert "GpuFluidObstacleRaster.CS.hlsl" in props
    assert "ObstacleRaster" in manifest
    assert "GpuFluidObstacleRaster.CS.hlsl" in manifest
    assert "- [x] 16.8 Collider / Obstacle" in docs
    assert "## 16.8 Collider / Obstacle flow" in docs
