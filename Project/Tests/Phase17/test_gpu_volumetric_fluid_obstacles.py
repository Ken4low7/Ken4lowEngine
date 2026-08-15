from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_phase17_7_obstacle_files_exist():
    required = [
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/GpuVolumetricFluidObstacleTypes.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidObstacleRasterPass.h",
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidObstacleRasterPass.cpp",
        "Engine/Scene/Actor/Components/GpuVolumetricFluidColliderObstacleAdapter.h",
        "Engine/Scene/Actor/Components/GpuVolumetricFluidColliderObstacleAdapter.cpp",
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidObstacleRaster.CS.hlsl",
    ]
    for relative in required:
        assert (ROOT / relative).is_file(), relative


def test_obstacle_gpu_and_raster_constant_layouts_are_mirrored_and_bounded():
    types = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Data/"
        "GpuVolumetricFluidObstacleTypes.h"
    )
    header = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidObstacleRasterPass.h"
    )

    assert "enum class GpuVolumetricFluidObstacleShape" in types
    assert "Sphere = 0" in types
    assert "Box" in types
    assert "static_assert(sizeof(GpuVolumetricFluidObstacleGpuData) == 96)" in types
    assert "static_assert(sizeof(GpuVolumetricFluidObstacleRasterConstants) == 64)" in types
    assert "axisWX" in types
    assert "IntersectsGpuVolumetricFluidDomain" in types
    assert "std::sqrt(Vector3::LengthSquared(source.halfSize))" in types
    assert "kMaxObstaclesPerDispatch = 256" in header
    assert "GetLastObstacleCount" in header
    assert "GetLastCulledObstacleCount" in header


def test_obstacle_raster_uploads_sources_culls_outside_and_clears_when_empty():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidObstacleRasterPass.cpp"
    )

    assert "BuildGpuVolumetricFluidObstacleGpuData" in source
    assert "IntersectsGpuVolumetricFluidDomain" in source
    assert "activeObstacles.size() >= kMaxObstaclesPerDispatch" in source
    assert "++lastCulledObstacleCount_" in source
    assert "activeObstacles.empty()" in source
    assert "ClearObstacle(commandList, grid)" in source
    assert "ClearUnorderedAccessViewUint" in source
    assert "uploadArena.Allocate(" in source
    assert "std::memcpy(obstacleAllocation.cpuAddress, activeObstacles.data(), obstacleBytes)" in source
    assert "SetComputeRootShaderResourceView(2, obstacleAllocation.gpuAddress)" in source
    assert "groupCountZ" in source
    assert "Dispatch(groupCountX, groupCountY, groupCountZ)" in source
    assert "InsertUavBarrier(commandList, obstacle.resource.Get())" in source


def test_obstacle_shader_rasterizes_world_space_sphere_and_obb_to_texture3d():
    shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidObstacleRaster.CS.hlsl"
    )

    assert "StructuredBuffer<GpuVolumetricFluidObstacleGpuData> gObstacles : register(t0)" in shader
    assert "RWTexture3D<uint> gObstacleMask : register(u0)" in shader
    assert "[numthreads(8, 8, 4)]" in shader
    assert "gDomainAxisU * (cellCenter.x * gDomainCellSize)" in shader
    assert "gDomainAxisV * (cellCenter.y * gDomainCellSize)" in shader
    assert "gDomainAxisW * (cellCenter.z * gDomainCellSize)" in shader
    assert "dot(offset, offset) <= obstacle.radius * obstacle.radius" in shader
    assert "abs(dot(offset, obstacle.axisX))" in shader
    assert "abs(dot(offset, obstacle.axisY))" in shader
    assert "abs(dot(offset, obstacle.axisZ))" in shader
    assert "gObstacleMask[dispatchThreadId] = solid" in shader


def test_physics_adapter_supports_sphere_aabb_obb_and_rejects_capsule_segment():
    adapter = read("Engine/Scene/Actor/Components/GpuVolumetricFluidColliderObstacleAdapter.cpp")

    assert "ECollisionShapeType::Sphere" in adapter
    assert "ECollisionShapeType::AABB" in adapter
    assert "ECollisionShapeType::OBB" in adapter
    assert "GpuVolumetricFluidObstacleShape::Sphere" in adapter
    assert "GpuVolumetricFluidObstacleShape::Box" in adapter
    assert "obb.orientations[0]" in adapter
    assert "obb.orientations[1]" in adapter
    assert "obb.orientations[2]" in adapter
    assert "ECollisionShapeType::Capsule" in adapter
    assert "ECollisionShapeType::Segment" in adapter
    assert "IsCollisionEnabledForPhysics()" in adapter
    assert "dynamic_cast<const ColliderComponent*>" in adapter


def test_velocity_and_scalar_advection_are_obstacle_aware():
    velocity_source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidVelocityAdvectionPass.cpp"
    )
    velocity_shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidVelocityAdvection.CS.hlsl"
    )
    scalar_source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidScalarAdvectionPass.cpp"
    )
    scalar_shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidScalarAdvection.CS.hlsl"
    )

    assert "grid.GetObstacle()" in velocity_source
    assert "D3D12_ROOT_PARAMETER rootParameters[4]" in velocity_source
    assert "Texture3D<uint> gObstacle : register(t1)" in velocity_shader
    assert "gVelocityWrite[cell] = 0.0f" in velocity_shader
    assert "sourceUvw = uvw" in velocity_shader

    assert "grid.GetObstacle()" in scalar_source
    assert "D3D12_ROOT_PARAMETER rootParameters[6]" in scalar_source
    assert "Texture3D<uint> gObstacle : register(t2)" in scalar_shader
    assert "gScalarWrite[cell] = 0.0f" in scalar_shader
    assert "sourceUvw = uvw" in scalar_shader


def test_pressure_projection_uses_obstacle_neumann_boundary_and_zero_normal_velocity():
    source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidPressureProjectionPass.cpp"
    )
    divergence = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidDivergence.CS.hlsl"
    )
    jacobi = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidPressureJacobi.CS.hlsl"
    )
    projection = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidProjection.CS.hlsl"
    )

    assert "D3D12_ROOT_PARAMETER rootParameters[5]" in source
    assert source.count("grid.GetObstacle()") >= 3
    assert "Texture3D<uint> gObstacle : register(t2)" in divergence
    assert "gDivergence[dispatchThreadId] = 0.0f" in divergence
    assert "gObstacle.Load(int4(leftCell, 0)) == 0u" in divergence

    assert "Texture3D<uint> gObstacle : register(t2)" in jacobi
    assert "gPressureWrite[dispatchThreadId] = 0.0f" in jacobi
    assert "pressureLeft = centerPressure" in jacobi
    assert "gObstacle.Load(int4(leftCell, 0)) == 0u" in jacobi

    assert "Texture3D<uint> gObstacle : register(t2)" in projection
    assert "gVelocityWrite[dispatchThreadId] = 0.0f" in projection
    assert "blockedLeft" in projection
    assert "blockedRight" in projection
    assert "blockedBottom" in projection
    assert "blockedTop" in projection
    assert "blockedBack" in projection
    assert "blockedFront" in projection
    assert "projectedVelocity.x = 0.0f" in projection
    assert "projectedVelocity.y = 0.0f" in projection
    assert "projectedVelocity.z = 0.0f" in projection


def test_force_and_emitter_stages_consume_obstacle_mask():
    force_source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/GpuVolumetricFluidForcePass.cpp"
    )
    emitter_source = read(
        "Engine/Graphics/Renderer/GpuFluid/Volumetric/Pass/"
        "GpuVolumetricFluidEmitterInjectionPass.cpp"
    )

    assert "D3D12_ROOT_PARAMETER rootParameters[6]" in force_source
    assert force_source.count("grid.GetObstacle()") >= 3
    for shader_name in [
        "GpuVolumetricFluidVorticityCurl.CS.hlsl",
        "GpuVolumetricFluidVorticityConfinement.CS.hlsl",
        "GpuVolumetricFluidBuoyancy.CS.hlsl",
    ]:
        shader = read(f"Resources/Shaders/GpuFluid/Volumetric/{shader_name}")
        assert "Texture3D<uint> gObstacle : register(t3)" in shader
        assert "gObstacle.Load" in shader

    assert "D3D12_ROOT_PARAMETER rootParameters[10]" in emitter_source
    assert "grid.GetObstacle()" in emitter_source
    emitter_shader = read(
        "Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidEmitterInjection.CS.hlsl"
    )
    assert "Texture3D<uint> gObstacle : register(t4)" in emitter_shader
    assert "gVelocityWrite[dispatchThreadId] = 0.0f" in emitter_shader
    assert "gDensityWrite[dispatchThreadId] = 0.0f" in emitter_shader
    assert "gTemperatureWrite[dispatchThreadId] = 0.0f" in emitter_shader


def test_manifest_build_and_docs_register_phase17_7_and_advance_to_raymarch():
    manifest = read("Engine/Graphics/Shader/Manifest/GpuVolumetricFluidShaderManifest.h")
    props = read("Directory.Build.props")
    docs = read("Docs/Phase17GpuVolumetricFluid.md")

    assert "ObstacleRaster" in manifest
    assert "GpuVolumetricFluidObstacleRaster.CS.hlsl" in manifest
    for name in [
        "GpuVolumetricFluidObstacleTypes.h",
        "GpuVolumetricFluidObstacleRasterPass.cpp",
        "GpuVolumetricFluidObstacleRasterPass.h",
        "GpuVolumetricFluidColliderObstacleAdapter.cpp",
        "GpuVolumetricFluidColliderObstacleAdapter.h",
        "GpuVolumetricFluidObstacleRaster.CS.hlsl",
    ]:
        assert name in props
    assert "- [x] 17.7 Volumetric Collider / Obstacle raster" in docs
    assert "## 17.7 Volumetric Collider / Obstacle raster" in docs
    assert "## Next implementation target — 17.8" in docs
