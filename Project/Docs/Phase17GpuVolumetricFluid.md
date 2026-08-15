# Phase 17 — 3D Volumetric Fluid

Phase 17 extends the Phase 16 2D Eulerian solver into a true 3D volume while keeping the 2D solver available for cheaper planar smoke, fog, and gameplay effects.

## Design direction

- Keep Phase 16 `GpuFluid*` untouched and introduce the 3D path as `GpuVolumetricFluid*`.
- Use native D3D12 `Texture3D` resources.
- Reuse the existing graphics SRV heap and compute SRV/UAV heap.
- Keep CPU/HLSL simulation constants explicitly mirrored.
- Start at 64x64x64 because volumetric cost grows cubically.
- Preserve ping-pong ownership and explicit resource-state transitions from Phase 16.
- Clear all fields deterministically before compute passes read them.
- Keep world/domain mapping independent from Scene Components.
- Keep Phase17 compute shaders in a dedicated manifest so 2D and 3D shader IDs remain independent.

## Roadmap

- [x] 17.1 3D base data / domain API
- [x] 17.2 Texture3D Grid / Resource / Reset foundation
- [x] 17.3 3D Velocity Advection
- [x] 17.4 3D Divergence / Pressure / Projection
- [x] 17.5 3D Density / Temperature / Vorticity / Buoyancy
- [ ] 17.6 3D Emitter injection
- [ ] 17.7 Volumetric Collider / Obstacle raster
- [ ] 17.8 Volume Raymarch Rendering
- [ ] 17.9 Depth-aware composition / lighting
- [ ] 17.10 Editor / Diagnostics / Stress Test

## 17.1 3D base data / domain API

`GpuVolumetricFluidGridDesc` defaults to 64x64x64 with `cellSize = 0.25f`. Each axis is capped at 256 voxels because doubling all three dimensions multiplies voxel count by eight.

`GpuVolumetricFluidDomainMapping` defines an oriented volume using `origin`, `axisU`, `axisV`, and `axisW`. `WorldToGrid`, `GridToWorld`, and `WorldVelocityToFluid` are shared by later emitters, obstacles, and rendering.

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

The shader uses `Texture3D<float4>`, linear clamp filtering, and `numthreads(8, 8, 4)`. Texture3D linear filtering supplies trilinear eight-voxel interpolation.

The pass transitions read/write states, dispatches XYZ, inserts a UAV barrier, transitions the write generation back to compute-readable state, and only then swaps velocity generations.

Obstacle handling remains deferred to 17.7.

## 17.4 3D Divergence / Pressure / Projection

`GpuVolumetricFluidPressureProjectionPass` records:

`Clear Pressure -> Divergence -> Jacobi x N -> Projection`

With the default 32 pressure iterations, one projection records 34 compute dispatches excluding the UAV pressure clear.

### 3D divergence

The six-neighbor centered difference is:

`div(u) = 0.5 / h * ((uR.x-uL.x) + (uT.y-uB.y) + (uF.z-uBack.z))`

Velocity outside the six volume faces is treated as zero.

### Pressure Jacobi

The 3D Poisson iteration uses six neighbors:

`p_new = (pL + pR + pB + pT + pBack + pFront - div * h^2) / 6`

Missing pressure neighbors use center pressure, giving a simple Neumann outer boundary.

### Projection

Projection subtracts the XYZ pressure gradient. Each outer face then zeros only its normal velocity component, allowing tangential wall flow while preventing volume escape.

## 17.5 3D Density / Temperature / Vorticity / Buoyancy

### Scalar advection

`GpuVolumetricFluidScalarAdvectionPass` transports Density and Temperature with the same semi-Lagrangian Texture3D backtrace used by velocity.

One PSO is shared by both scalar fields. A `b1` root-constant block selects the field-specific dissipation value:

- Density -> `densityDissipation`
- Temperature -> `temperatureDissipation`

`DispatchAll` shares one `GpuVolumetricFluidSimulationConstants` upload between both scalar dispatches. Each scalar field inserts a UAV barrier and transitions back to compute-readable state before its ping-pong swap.

### 3D curl vector

`GpuVolumetricFluidVorticityCurl.CS.hlsl` stores the full curl vector in the xyz channels of the RGBA16F vorticity field:

`curl(u) = (dw/dy - dv/dz, du/dz - dw/dx, dv/dx - du/dy)`

The derivative uses the six axis-aligned velocity neighbors and centered differences. Missing velocity neighbors outside the closed volume are treated as zero. The reserved W channel remains zero.

### Vorticity confinement

`GpuVolumetricFluidVorticityConfinement.CS.hlsl` computes the gradient of curl magnitude:

`N = normalize(grad(|omega|))`

and feeds small-scale rotational energy back into velocity:

`F_vort = vorticityStrength * cross(N, omega)`

`velocity += F_vort * dt`

Missing magnitude neighbors at the outer volume faces use the center magnitude, avoiding an artificial `|omega|` gradient through the boundary. After force application, the six outer faces zero only their normal velocity components.

### Buoyancy

Buoyancy follows the Phase16 sign convention:

`F_y = buoyancy * (temperature - ambientTemperature) - smokeWeight * density`

`velocity.y += F_y * dt`

The other velocity components remain unchanged except for closed-volume normal-boundary enforcement.

### Force pass ordering

`GpuVolumetricFluidForcePass::DispatchAll` records:

`Curl -> Vorticity Confinement -> Buoyancy`

Curl writes the dedicated vorticity Texture3D. Confinement and Buoyancy each write the opposite velocity generation, insert a UAV barrier, transition the result back to SRV state, and swap velocity.

Force application can introduce divergence. The future runtime manager must therefore execute another Pressure Projection after the force stage rather than hiding projection inside `GpuVolumetricFluidForcePass`.

### Current intended solver order

Once 17.6/17.7 provide sources and obstacles, one fixed simulation step is intended to become:

`Obstacle -> Emitter -> Velocity Advection -> Projection -> Scalar Advection -> Curl/Vorticity/Buoyancy -> Projection`

## Build integration

`Project/Directory.Build.props` registers all Phase17 resource/pass/manifest/HLSL files only for the main `Ken4lowEngine` project.

## Validation

`Project/Tests/Phase17` statically checks the current 3D foundation, including:

- Texture3D descriptor helpers
- grid/domain and constant-buffer contracts
- deterministic reset
- 3D velocity and scalar advection
- XYZ compute dispatch
- six-neighbor divergence and pressure Jacobi
- XYZ pressure projection and closed outer faces
- vector Curl component equations
- `grad(|omega|)` and `cross(N, omega)` confinement
- Density/Temperature buoyancy feedback
- UAV barriers before scalar/velocity ping-pong swaps
- shader manifest and build registration

A real Windows / Visual Studio / DXC / GPU build is still required after repository integration.

## Next implementation target — 17.6

Implement **3D Emitter injection**.

The next stage should add:

- world-space spherical volumetric emitters
- `GpuVolumetricFluidDomainMapping` based World-to-Grid conversion
- velocity / density / temperature injection into Texture3D fields
- batched StructuredBuffer source data through `FrameUploadArena`
- one full-volume XYZ dispatch for all active emitters
- source culling outside the 3D domain
- deterministic velocity/density/temperature ping-pong swaps
- a Scene Component/adapter path that can later be driven by the runtime manager
