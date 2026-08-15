# Phase 17 — 3D Volumetric Fluid

Phase 17 extends the Phase 16 2D Eulerian solver into a true 3D volume while keeping the 2D solver available for cheaper planar smoke, fog, and gameplay effects.

## Design direction

- Keep Phase 16 `GpuFluid*` untouched and introduce the 3D path as `GpuVolumetricFluid*`.
- Use native D3D12 `Texture3D` resources rather than stacking 2D slices in application code.
- Reuse the existing persistent graphics SRV heap and compute SRV/UAV heap.
- Keep CPU/HLSL simulation constants explicitly mirrored.
- Start with a conservative 64x64x64 default volume and explicit dimensional limits because cost grows cubically.
- Preserve ping-pong semantics from Phase 16 so advection/projection passes can be ported without changing ownership rules.
- Clear every field deterministically before any 3D compute pass is allowed to read it.
- Keep world/domain mapping independent from Scene Components so emitters, colliders, and rendering can share one coordinate contract.

## Roadmap

- [x] 17.1 3D base data / domain API
- [x] 17.2 Texture3D Grid / Resource / Reset foundation
- [ ] 17.3 3D Velocity Advection
- [ ] 17.4 3D Divergence / Pressure / Projection
- [ ] 17.5 3D Density / Temperature / Vorticity / Buoyancy
- [ ] 17.6 3D Emitter injection
- [ ] 17.7 Volumetric Collider / Obstacle raster
- [ ] 17.8 Volume Raymarch Rendering
- [ ] 17.9 Depth-aware composition / lighting
- [ ] 17.10 Editor / Diagnostics / Stress Test

## 17.1 3D base data and domain API

`GpuVolumetricFluidGridDesc` defaults to 64x64x64 with a 0.25 world-unit cell size. Each dimension is capped at 256 voxels. Unlike a 2D grid, doubling all three axes multiplies voxel count by eight, so the limit belongs in the data contract rather than only in an Editor widget.

`GpuVolumetricFluidSimulationDesc` begins with:

- fixed step: 1/60 s
- pressure iterations: 32
- max catch-up substeps: 2
- velocity dissipation: 0.995
- density dissipation: 0.999
- temperature dissipation: 0.995
- vorticity strength: 0.15
- buoyancy: 1.0
- smoke weight: 0.05

Pressure iterations are capped at 192 for the initial 3D solver.

### World / volume mapping

`GpuVolumetricFluidDomainMapping` defines one oriented volume using:

- `origin`: minimum UVW corner in world space
- `axisU`
- `axisV`
- `axisW`

The three axes must be non-zero and pairwise orthogonal within a small tolerance. Mapping helpers provide:

- `WorldToGrid`
- `GridToWorld`
- `WorldVelocityToFluid`

This contract will later be shared by 3D emitters, obstacle rasterization, and the raymarch renderer rather than duplicating world-to-volume math in each pass.

### Constant-buffer contract

`GpuVolumetricFluidSimulationConstants` is 80 bytes and is mirrored by `GpuVolumetricFluidCommon.hlsli`.

The layout contains:

- grid width / height / depth
- cell size
- inverse grid dimensions
- inverse cell size
- delta / elapsed time
- three dissipation values
- vorticity strength
- ambient temperature
- buoyancy
- smoke weight

The HLSL common file also centralizes voxel-center UVW conversion, UVW clamping, cell clamping, and in-grid tests for all later compute shaders.

## 17.2 Texture3D Grid / Resource foundation

### Descriptor support

The engine descriptor managers now expose native Texture3D helpers:

`SRVManager`

- `CreateSRVForTexture3D`

`UAVManager`

- `CreateSRVForTexture3DOnThisHeap`
- `CreateUAVForTexture3D`

The 3D UAV is created in both the shader-visible heap and the existing CPU-only clear heap. This keeps `ClearUnorderedAccessViewFloat/Uint` available for volume resources without introducing another descriptor allocator.

### Field layout

| Field | Format | Resources | Logical bytes / voxel | Purpose |
|---|---|---:|---:|---|
| Velocity | `R16G16B16A16_FLOAT` | 2 | 16 | xyz velocity, w reserved |
| Pressure | `R16_FLOAT` | 2 | 4 | Jacobi pressure solve |
| Divergence | `R16_FLOAT` | 1 | 2 | 3D divergence |
| Density | `R16_FLOAT` | 2 | 4 | smoke density |
| Temperature | `R16_FLOAT` | 2 | 4 | buoyancy source |
| Vorticity | `R16G16B16A16_FLOAT` | 1 | 8 | xyz curl, w reserved |
| Obstacle | `R8_UINT` | 1 | 1 | solid voxel mask |

Total logical storage is **39 bytes per voxel** before committed-resource alignment/allocator overhead.

At the default 64^3 grid this is 10,223,616 bytes, approximately 9.75 MiB. A 128^3 volume is approximately 78 MiB, which is why 64^3 is the development default rather than copying Phase 16's 256x256 dimensions into all three axes.

### Resource ownership

`GpuVolumetricFluidGridResource` owns:

- velocity ping-pong
- pressure ping-pong
- density ping-pong
- temperature ping-pong
- divergence
- vorticity
- obstacle

Every texture receives:

- a persistent graphics SRV from `SRVManager`
- a compute SRV from `UAVManager`
- a compute UAV from `UAVManager`
- explicit resource-state tracking

Velocity and vorticity use RGBA16F because D3D12 has no three-channel 16-bit float texture format suitable for the same filtered Texture3D workflow. Only xyz is used by the solver; w remains reserved.

### Deterministic reset

`GpuVolumetricFluidResetPass` is part of the foundation rather than being deferred to a later phase. It clears both generations of every ping-pong field plus all single fields, inserts UAV barriers, transitions them to compute-readable state, and resets ping-pong read indices to zero.

This guarantees that 17.3 Velocity Advection can safely read the grid immediately after initialization.

## Build integration

`Project/Directory.Build.props` registers the Phase 17 C++ foundation and shared HLSL include only for the main `Ken4lowEngine` project.

## Validation

`Project/Tests/Phase17/test_gpu_volumetric_fluid_foundation.py` statically checks:

- 3D descriptor helpers
- grid/domain contracts
- 80-byte CPU/HLSL constant layout
- Texture3D resource creation
- field formats and ping-pong ownership
- 39-byte logical memory estimate
- deterministic reset coverage
- build registration
- roadmap state

A real Windows / Visual Studio / DXC / GPU build is still required after repository integration.

## Next implementation target — 17.3

Implement **3D Velocity Advection** using semi-Lagrangian backtrace and trilinear Texture3D sampling:

`x_back = x - dt * velocity(x)`

The first 3D compute pass should:

- dispatch in XYZ thread groups
- read velocity from the current ping-pong SRV
- write the next velocity UAV
- use `GpuVolumetricFluidClampUvwToCellCenters`
- apply velocity dissipation
- reserve the obstacle-aware branch for 17.7 while preserving domain-boundary behavior
- swap velocity only after UAV barrier and state transition
