# Phase 17 — 3D Volumetric Fluid

Phase 17 extends the Phase 16 2D Eulerian solver into a true 3D volume while keeping the 2D solver available for cheaper planar smoke, fog, and gameplay effects.

## Design direction

- Keep Phase 16 `GpuFluid*` untouched and introduce the 3D path as `GpuVolumetricFluid*`.
- Use native D3D12 `Texture3D` resources rather than application-managed 2D slice stacks.
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
- [ ] 17.5 3D Density / Temperature / Vorticity / Buoyancy
- [ ] 17.6 3D Emitter injection
- [ ] 17.7 Volumetric Collider / Obstacle raster
- [ ] 17.8 Volume Raymarch Rendering
- [ ] 17.9 Depth-aware composition / lighting
- [ ] 17.10 Editor / Diagnostics / Stress Test

## 17.1 3D base data / domain API

`GpuVolumetricFluidGridDesc` defaults to 64x64x64 with `cellSize = 0.25f`. Each axis is capped at 256 voxels because doubling all three dimensions multiplies voxel count by eight.

`GpuVolumetricFluidSimulationDesc` starts with:

- fixed step: 1/60 s
- pressure iterations: 32
- max substeps: 2
- velocity dissipation: 0.995
- density dissipation: 0.999
- temperature dissipation: 0.995
- vorticity strength: 0.15
- buoyancy: 1.0
- smoke weight: 0.05

Pressure iterations are capped at 192.

### Domain mapping

`GpuVolumetricFluidDomainMapping` defines an oriented volume with `origin`, `axisU`, `axisV`, and `axisW`.

The shared mapping API provides:

- `WorldToGrid`
- `GridToWorld`
- `WorldVelocityToFluid`

The axes must be non-zero and pairwise orthogonal. Emitters, obstacles, and volume rendering will all reuse this mapping contract.

### Simulation constants

`GpuVolumetricFluidSimulationConstants` is 80 bytes and is mirrored by `GpuVolumetricFluidCommon.hlsli`.

The common HLSL also provides voxel-center UVW conversion, UVW clamping, cell clamping, and in-grid tests.

## 17.2 Texture3D Grid / Resource / Reset foundation

The descriptor managers expose native Texture3D helpers:

`SRVManager`

- `CreateSRVForTexture3D`

`UAVManager`

- `CreateSRVForTexture3DOnThisHeap`
- `CreateUAVForTexture3D`

The UAV is created in both the shader-visible heap and the CPU-only clear heap so `ClearUnorderedAccessViewFloat/Uint` works for 3D fields.

### Field layout

| Field | Format | Resources | Logical bytes / voxel | Purpose |
|---|---|---:|---:|---|
| Velocity | `R16G16B16A16_FLOAT` | 2 | 16 | xyz velocity, w reserved |
| Pressure | `R16_FLOAT` | 2 | 4 | pressure solve |
| Divergence | `R16_FLOAT` | 1 | 2 | 3D divergence |
| Density | `R16_FLOAT` | 2 | 4 | smoke density |
| Temperature | `R16_FLOAT` | 2 | 4 | buoyancy source |
| Vorticity | `R16G16B16A16_FLOAT` | 1 | 8 | xyz curl, w reserved |
| Obstacle | `R8_UINT` | 1 | 1 | solid voxel mask |

Total logical storage is 39 bytes per voxel before resource-allocation overhead. A 64^3 volume is about 9.75 MiB and 128^3 is about 78 MiB.

`GpuVolumetricFluidResetPass` clears both generations of every ping-pong field plus divergence, vorticity, and obstacle, then resets all ping-pong read indices to zero.

## 17.3 3D Velocity Advection

`GpuVolumetricFluidVelocityAdvectionPass` transports velocity through itself with semi-Lagrangian backtracing:

`x_back = x - dt * velocity(x)`

The shader uses `Texture3D<float4>`, linear clamp filtering, and `numthreads(8, 8, 4)`. Texture3D linear filtering supplies the eight-voxel interpolation required for trilinear sampling.

The pass performs:

1. velocity read transition to `NON_PIXEL_SHADER_RESOURCE`,
2. velocity write transition to `UNORDERED_ACCESS`,
3. XYZ compute dispatch,
4. UAV barrier,
5. write transition back to compute-readable state,
6. velocity ping-pong swap.

Obstacle handling remains deferred to 17.7 so the basic 3D solver path can be validated independently.

## 17.4 3D Divergence / Pressure / Projection

`GpuVolumetricFluidPressureProjectionPass` makes the advected velocity field approximately divergence-free.

One projection call records this sequence:

`Clear Pressure -> Divergence -> Jacobi x N -> Projection`

The pressure clear is a UAV clear and is not counted as a compute dispatch. With the default 32 pressure iterations, one projection records 34 compute dispatches.

### Root contract

The three compute stages share one root signature:

| Root parameter | Register | Use |
|---:|---|---|
| 0 | `b0` | `GpuVolumetricFluidSimulationConstants` |
| 1 | `t0` | velocity or divergence |
| 2 | `t1` | pressure read when required |
| 3 | `u0` | divergence, pressure write, or velocity write |

The root contract intentionally leaves obstacle input out until 17.7.

### 3D divergence

The divergence shader samples the six axis-aligned velocity neighbors:

- left / right
- bottom / top
- back / front

The centered-difference approximation is:

`div(u) = 0.5 / h * ((uR.x-uL.x) + (uT.y-uB.y) + (uF.z-uBack.z))`

Neighbors outside the volume are treated as zero velocity, representing a closed domain with no flux beyond the six outer faces.

### 6-neighbor pressure Jacobi

The pressure Poisson equation uses six neighbors instead of the four-neighbor 2D stencil:

`p_new = (pL + pR + pB + pT + pBack + pFront - div * h^2) / 6`

Both pressure ping-pong generations are cleared before the solve. Each Jacobi iteration transitions the current read generation to SRV, the write generation to UAV, dispatches the full XYZ grid, inserts a UAV barrier, returns the write generation to SRV state, and only then swaps generations.

At the outer domain faces, missing pressure neighbors use the center pressure. This is a simple Neumann pressure boundary and avoids generating artificial pressure gradients outside the volume.

### 3D projection

Projection subtracts the centered 3D pressure gradient:

`u_projected = u - grad(p)`

The gradient contains independent X, Y, and Z components. After subtraction, each outer volume face zeros only its normal velocity component:

- X faces -> `velocity.x = 0`
- Y faces -> `velocity.y = 0`
- Z faces -> `velocity.z = 0`

Tangential velocity remains available, so fluid may move along the walls while it cannot leave the volume.

The projected velocity is written as `float4(xyz, 0)` so the reserved velocity W channel stays deterministic.

### Dispatch and diagnostics

All three stages use `numthreads(8, 8, 4)`. CPU group counts are ceil-divided independently for width, height, and depth.

`GpuVolumetricFluidPressureProjectionPass` exposes:

- total compute dispatch count
- last pressure iteration count

These counters will feed the Phase17.10 diagnostics panel.

## Build integration

`Project/Directory.Build.props` registers the Phase17 resource/pass/manifest/HLSL files only for the main `Ken4lowEngine` project.

## Validation

`Project/Tests/Phase17` statically checks the current 3D foundation, including:

- Texture3D descriptor helpers
- grid/domain contracts
- 80-byte CPU/HLSL constants
- Texture3D field formats and ownership
- deterministic reset
- 3D velocity advection
- XYZ dispatch
- pressure reset coverage
- six-neighbor divergence
- six-neighbor pressure Jacobi
- Neumann outer pressure boundary
- XYZ pressure-gradient subtraction
- zero normal velocity on all six domain faces
- UAV barrier before pressure/velocity ping-pong swaps
- shader manifest and build registration

A real Windows / Visual Studio / DXC / GPU build is still required after repository integration.

## Next implementation target — 17.5

Implement **3D Density / Temperature / Vorticity / Buoyancy**.

The next stage should add:

- Texture3D scalar advection for density and temperature
- 3D curl vector calculation
- 3D vorticity confinement
- buoyancy feedback into the Y velocity component
- velocity projection after force application at the eventual runtime-manager integration point
- the same XYZ dispatch, barrier, and ping-pong contracts used by 17.3/17.4
