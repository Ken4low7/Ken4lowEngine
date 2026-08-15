# Phase 17 — 3D Volumetric Fluid

Phase 17 extends the Phase 16 2D Eulerian solver into a true 3D volume while keeping the 2D solver available for cheaper planar smoke, fog, and gameplay effects.

## Design direction

- Keep Phase 16 `GpuFluid*` available and introduce the 3D path as `GpuVolumetricFluid*`.
- Use native D3D12 `Texture3D` resources.
- Reuse the existing graphics SRV heap and compute SRV/UAV heap.
- Keep CPU/HLSL data layouts explicitly mirrored.
- Start at 64x64x64 because volumetric cost grows cubically.
- Preserve ping-pong ownership and explicit resource-state transitions from Phase 16.
- Keep world/domain mapping independent from Scene Components.
- Keep Phase17 compute shaders in a dedicated manifest.
- Treat the `R8_UINT` obstacle Texture3D as a solver boundary contract, not only a visualization field.

## Roadmap

- [x] 17.1 3D base data / domain API
- [x] 17.2 Texture3D Grid / Resource / Reset foundation
- [x] 17.3 3D Velocity Advection
- [x] 17.4 3D Divergence / Pressure / Projection
- [x] 17.5 3D Density / Temperature / Vorticity / Buoyancy
- [x] 17.6 3D Emitter injection
- [x] 17.7 Volumetric Collider / Obstacle raster
- [ ] 17.8 Volume Raymarch Rendering
- [ ] 17.9 Depth-aware composition / lighting
- [ ] 17.10 Editor / Diagnostics / Stress Test

## 17.1 3D base data / domain API

`GpuVolumetricFluidGridDesc` defaults to 64x64x64 with `cellSize = 0.25f`. Each axis is capped at 256 voxels because doubling all three dimensions multiplies voxel count by eight.

`GpuVolumetricFluidDomainMapping` defines an oriented volume using `origin`, `axisU`, `axisV`, and `axisW`. `WorldToGrid`, `GridToWorld`, and `WorldVelocityToFluid` are shared by emitters, obstacles, and rendering.

`GpuVolumetricFluidSimulationConstants` is 80 bytes and mirrored by `GpuVolumetricFluidCommon.hlsli`.

## 17.2 Texture3D Grid / Resource / Reset foundation

Shared descriptor managers expose native Texture3D helpers:

- `SRVManager::CreateSRVForTexture3D`
- `UAVManager::CreateSRVForTexture3DOnThisHeap`
- `UAVManager::CreateUAVForTexture3D`

The field layout is:

| Field | Format | Resources | Logical bytes / voxel |
|---|---|---:|---:|
| Velocity | `R16G16B16A16_FLOAT` | 2 | 16 |
| Pressure | `R16_FLOAT` | 2 | 4 |
| Divergence | `R16_FLOAT` | 1 | 2 |
| Density | `R16_FLOAT` | 2 | 4 |
| Temperature | `R16_FLOAT` | 2 | 4 |
| Vorticity | `R16G16B16A16_FLOAT` | 1 | 8 |
| Obstacle | `R8_UINT` | 1 | 1 |

Total logical storage is 39 bytes per voxel. A 64^3 volume is about 9.75 MiB and 128^3 is about 78 MiB.

`GpuVolumetricFluidResetPass` clears all ping-pong generations plus divergence, vorticity, and obstacle, then resets read indices to zero.

## 17.3 3D Velocity Advection

`GpuVolumetricFluidVelocityAdvectionPass` transports velocity through itself with semi-Lagrangian backtracing:

`x_back = x - dt * velocity(x)`

The shader uses `Texture3D<float4>`, linear clamp filtering, and `numthreads(8, 8, 4)`.

After 17.7 the pass also samples the obstacle Texture3D. Solid destination voxels are forced to zero. If the backtrace source lands inside a solid voxel, the sample position falls back to the current voxel rather than pulling velocity through the wall.

The pass still inserts a UAV barrier and returns the write resource to compute-readable state before swapping velocity generations.

## 17.4 3D Divergence / Pressure / Projection

`GpuVolumetricFluidPressureProjectionPass` records:

`Clear Pressure -> Divergence -> Jacobi x N -> Projection`

With the default 32 pressure iterations, one projection records 34 compute dispatches excluding the UAV pressure clear.

### 3D divergence

The six-neighbor centered difference is:

`div(u) = 0.5 / h * ((uR.x-uL.x) + (uT.y-uB.y) + (uF.z-uBack.z))`

Domain-outside neighbors and solid neighbors are both treated as zero velocity. Solid destination voxels write zero divergence.

### Pressure Jacobi

The 3D Poisson iteration uses six neighbors:

`p_new = (pL + pR + pB + pT + pBack + pFront - div * h^2) / 6`

Missing domain neighbors and solid neighbors use the center pressure. This gives the outer volume and internal colliders the same simple Neumann pressure-boundary behavior. Solid destination voxels write zero pressure.

### Projection

Projection subtracts the XYZ pressure gradient. For each axis, either a domain edge or a solid neighbor marks that face as blocked. The projected velocity then zeros only the blocked axis normal component, preserving tangential flow along collider surfaces.

## 17.5 3D Density / Temperature / Vorticity / Buoyancy

### Scalar advection

`GpuVolumetricFluidScalarAdvectionPass` transports Density and Temperature with the same semi-Lagrangian Texture3D backtrace used by velocity. `DispatchAll` shares one simulation constant upload between both scalar dispatches.

After 17.7, solid destination voxels write zero scalar and a backtrace source inside a solid falls back to the current voxel.

### 3D curl vector

`GpuVolumetricFluidVorticityCurl.CS.hlsl` stores the full curl vector in the xyz channels of the RGBA16F vorticity field:

`curl(u) = (dw/dy - dv/dz, du/dz - dw/dx, dv/dx - du/dy)`

Solid and domain-outside velocity neighbors are treated as zero. Solid destination voxels write zero vorticity.

### Vorticity confinement

`GpuVolumetricFluidVorticityConfinement.CS.hlsl` computes:

`N = normalize(grad(|omega|))`

`F_vort = vorticityStrength * cross(N, omega)`

`velocity += F_vort * dt`

For the `|omega|` gradient, solid and outside neighbors use the center magnitude. This avoids turning the collider boundary itself into an artificial curl-magnitude gradient. Blocked faces also zero their normal velocity component after force application.

### Buoyancy

Buoyancy follows the Phase16 sign convention:

`F_y = buoyancy * (temperature - ambientTemperature) - smokeWeight * density`

`velocity.y += F_y * dt`

Solid destination voxels write zero velocity, and solid/domain boundary faces preserve the no-normal-flow contract.

### Force pass ordering

`GpuVolumetricFluidForcePass::DispatchAll` records:

`Curl -> Vorticity Confinement -> Buoyancy`

Force application can introduce divergence, so the eventual runtime manager must execute another Pressure Projection after the force stage.

## 17.6 3D Emitter injection

`GpuVolumetricFluidEmitterInjectionPass` adds world-space spherical sources to Velocity, Density, and Temperature in one full-volume dispatch.

`GpuVolumetricFluidEmitterSource` stores world position, world velocity, radius, velocity strength, density rate, temperature rate, falloff exponent, and enabled state. `BuildGpuVolumetricFluidEmitterGpuData` converts world center and velocity through `GpuVolumetricFluidDomainMapping`.

`GpuVolumetricFluidEmitterGpuData` is 64 bytes and mirrored by HLSL. A spherical source completely outside any of the six volume sides is rejected before upload. One dispatch accepts at most 256 sources to bound the per-voxel source loop.

Velocity, Density, and Temperature read their current ping-pong generation and write the opposite generation in one `numthreads(8, 8, 4)` dispatch. All three UAV barriers complete before any field swaps.

After 17.7 the injection shader also consumes the obstacle mask. Solid voxels write zero Velocity, Density, and Temperature instead of accumulating source data. This prevents hidden smoke inside a collider from appearing when that collider moves away.

The existing `FluidEmitterComponent` remains shared by 2D and 3D fluid through `BuildEmitterSource()` and `BuildVolumetricEmitterSource()`.

## 17.7 Volumetric Collider / Obstacle raster

### Obstacle source contract

`GpuVolumetricFluidObstacleSource` supports:

- Sphere
- AABB represented as an axis-aligned Box
- OBB represented as an oriented Box

The GPU StructuredBuffer element is `GpuVolumetricFluidObstacleGpuData`, fixed at 96 bytes. Sphere data uses center/radius. Box data uses center, half extents, and normalized world-space X/Y/Z axes.

Capsule and Segment remain intentionally unsupported until the Physics collider layer exposes the primitive data required for stable rasterization.

### Physics adapter

`GpuVolumetricFluidColliderObstacleAdapter` scans active `ColliderComponent` instances that are enabled for Physics collision and converts Sphere/AABB/OBB primitives into renderer-independent obstacle sources.

It is separate from the Phase16 adapter so the 2D and 3D data contracts can evolve independently while both use the same Physics collider primitives.

### CPU culling and safety bound

Before upload, invalid sources and obstacles whose conservative bounding sphere is completely outside the volume are removed. For OBBs the half-extents vector length is used as a conservative bounding-sphere radius, avoiding false-negative domain culling.

One raster dispatch accepts at most 256 obstacles. `lastObstacleCount` reports uploaded obstacles and `lastCulledObstacleCount` reports invalid, out-of-volume, or over-limit sources.

### Texture3D raster

`GpuVolumetricFluidObstacleRasterPass` writes the existing `R8_UINT` obstacle Texture3D with one `numthreads(8, 8, 4)` dispatch.

`GpuVolumetricFluidObstacleRasterConstants` is 64 bytes and supplies:

- world-space domain origin
- normalized U/V/W axes
- cell size
- obstacle count

Each voxel center is converted to world space:

`P = origin + U*x*cellSize + V*y*cellSize + W*z*cellSize`

The shader then tests the world point against all uploaded Sphere/Box shapes. This keeps rotated OBBs and rotated fluid domains in the same world-space containment contract.

When obstacles are present, every voxel is explicitly rewritten to 0 or 1 each dispatch. When there are no active obstacles, the mask is cleared with `ClearUnorderedAccessViewUint`. Moving or deleted colliders therefore cannot leave stale solid voxels.

### Solver-wide obstacle contract

17.7 connects the mask to every simulation stage currently implemented:

- Velocity Advection: solid destination zero, backtrace cannot pull through solid.
- Scalar Advection: solid destination zero, backtrace cannot pull through solid.
- Divergence: solid/outside neighbors contribute zero velocity.
- Pressure Jacobi: solid/outside neighbors use center pressure.
- Projection: solid destination zero and blocked-face normal velocity zero.
- Curl: solid/outside neighbors contribute zero velocity.
- Vorticity Confinement: solid/outside magnitude neighbors use center magnitude and blocked normals are zeroed.
- Buoyancy: solid destination zero and blocked normals are zeroed.
- Emitter Injection: solid voxels clear Velocity/Density/Temperature and receive no source injection.

### Intended fixed-step order

Once the runtime manager is added, the intended solver sequence is:

`Obstacle -> Emitter -> Velocity Advection -> Projection -> Scalar Advection -> Curl/Vorticity/Buoyancy -> Projection`

The obstacle mask must be rasterized first so all later stages in that fixed step see current Physics transforms.

## Build integration

`Project/Directory.Build.props` registers all current Phase17 data/pass/adapter/manifest/HLSL files only for the main `Ken4lowEngine` project.

## Validation

`Project/Tests/Phase17` statically checks the current 3D foundation, including:

- Texture3D descriptor and grid contracts
- deterministic reset
- 3D velocity/scalar advection
- six-neighbor pressure solve
- vector Curl / confinement / buoyancy
- 3D emitter upload and injection
- 96-byte obstacle GPU layout and 64-byte raster constants
- Sphere/AABB/OBB Physics adapter mapping
- conservative obstacle domain culling and 256-obstacle cap
- Texture3D world-space obstacle raster
- zero-mask clear when all colliders disappear
- obstacle SRV binding in all implemented solver stages
- internal Neumann pressure boundary and zero normal velocity
- shader manifest and build registration

A real Windows / Visual Studio / DXC / GPU build is still required after repository integration.

## Next implementation target — 17.8

Implement **Volume Raymarch Rendering**.

The next stage should add:

- a world-space oriented volume box render contract
- camera-ray / oriented-box intersection
- Density/Temperature Texture3D sampling along the ray
- front-to-back transmittance accumulation
- configurable absorption, emission, step count, and early exit
- a transparent Forward Queue packet or dedicated volume bucket without a vertex buffer
- current active render-view camera resolution at draw execution so reflection views remain correct
- optional Obstacle debug visualization without coupling the renderer to Physics
