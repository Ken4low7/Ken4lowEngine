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
- Keep Phase17 compute/graphics shaders in a dedicated manifest.
- Treat the `R8_UINT` obstacle Texture3D as a solver boundary contract, not only a visualization field.
- Render the volume through the existing Transparent Forward Queue so sorting and render-view overrides remain consistent with the rest of the engine.

## Roadmap

- [x] 17.1 3D base data / domain API
- [x] 17.2 Texture3D Grid / Resource / Reset foundation
- [x] 17.3 3D Velocity Advection
- [x] 17.4 3D Divergence / Pressure / Projection
- [x] 17.5 3D Density / Temperature / Vorticity / Buoyancy
- [x] 17.6 3D Emitter injection
- [x] 17.7 Volumetric Collider / Obstacle raster
- [x] 17.8 Volume Raymarch Rendering
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

The pass inserts a UAV barrier and returns the write resource to compute-readable state before swapping velocity generations.

## 17.4 3D Divergence / Pressure / Projection

`GpuVolumetricFluidPressureProjectionPass` records:

`Clear Pressure -> Divergence -> Jacobi x N -> Projection`

With the default 32 pressure iterations, one projection records 34 compute dispatches excluding the UAV pressure clear.

The divergence and pressure solve use six neighbors. Domain-outside and solid velocity neighbors are zero for divergence, while missing/solid pressure neighbors use center pressure for a simple Neumann boundary. Projection subtracts the XYZ pressure gradient and zeros blocked normal velocity while preserving tangential flow along domain and collider surfaces.

## 17.5 3D Density / Temperature / Vorticity / Buoyancy

`GpuVolumetricFluidScalarAdvectionPass` transports Density and Temperature with the same semi-Lagrangian Texture3D backtrace used by velocity. Solid destination voxels write zero scalar and backtraces do not pull values through obstacles.

`GpuVolumetricFluidVorticityCurl.CS.hlsl` stores the full curl vector:

`curl(u) = (dw/dy - dv/dz, du/dz - dw/dx, dv/dx - du/dy)`

Vorticity confinement computes `N = normalize(grad(|omega|))` and feeds `vorticityStrength * cross(N, omega)` back into velocity. Buoyancy follows the Phase16 sign convention:

`F_y = buoyancy * (temperature - ambientTemperature) - smokeWeight * density`

`GpuVolumetricFluidForcePass::DispatchAll` records:

`Curl -> Vorticity Confinement -> Buoyancy`

Force application can introduce divergence, so the eventual runtime manager must execute another Pressure Projection after the force stage.

## 17.6 3D Emitter injection

`GpuVolumetricFluidEmitterInjectionPass` adds world-space spherical sources to Velocity, Density, and Temperature in one full-volume dispatch.

`GpuVolumetricFluidEmitterGpuData` is 64 bytes and mirrored by HLSL. Sources are transformed through `GpuVolumetricFluidDomainMapping`, completely out-of-volume sources are culled on CPU, and one dispatch accepts at most 256 sources.

Velocity, Density, and Temperature read their current ping-pong generation and write the opposite generation in one `numthreads(8, 8, 4)` dispatch. All three UAV barriers complete before any field swaps.

After 17.7 the injection shader consumes the obstacle mask. Solid voxels clear Velocity, Density, and Temperature instead of accumulating hidden source data.

The existing `FluidEmitterComponent` remains shared by 2D and 3D fluid through `BuildEmitterSource()` and `BuildVolumetricEmitterSource()`.

## 17.7 Volumetric Collider / Obstacle raster

`GpuVolumetricFluidObstacleSource` supports Sphere, AABB-as-Box, and OBB-as-oriented-Box. `GpuVolumetricFluidObstacleGpuData` is 96 bytes and the raster constants are 64 bytes.

`GpuVolumetricFluidColliderObstacleAdapter` scans active Physics `ColliderComponent` instances and converts supported primitives into renderer-independent obstacle sources. Invalid/out-of-volume obstacles are conservatively culled and one dispatch accepts at most 256 obstacles.

`GpuVolumetricFluidObstacleRasterPass` writes the existing `R8_UINT` Texture3D with one `numthreads(8, 8, 4)` dispatch. Every voxel is rewritten each raster dispatch, while zero active obstacles use `ClearUnorderedAccessViewUint`, so moving/deleted colliders cannot leave stale solid voxels.

17.7 connects that mask to Velocity/Scalar Advection, Divergence, Pressure Jacobi, Projection, Curl, Vorticity Confinement, Buoyancy, and Emitter Injection.

The intended fixed-step sequence remains:

`Obstacle -> Emitter -> Velocity Advection -> Projection -> Scalar Advection -> Curl/Vorticity/Buoyancy -> Projection`

## 17.8 Volume Raymarch Rendering

### Render contract

`GpuVolumetricFluidRenderDesc` separates visual quality from simulation settings. It exposes smoke/cold/hot/obstacle colors, opacity, density/temperature scales, absorption, thermal emission strength, raymarch step scale, early-exit threshold, maximum step count, and Smoke/ObstacleDebug modes.

`GpuVolumetricFluidRenderConstants` is 256 bytes and explicitly mirrored by both raymarch shaders. It contains the active view-projection, active camera position, oriented U/V/W volume axes and extents, cell/quality scales, explicit grid XYZ dimensions, and color parameters.

### Vertexless oriented volume proxy

`GpuVolumetricFluidRaymarchRenderer` issues `DrawInstanced(36, 1, 0, 0)`. `GpuVolumetricFluidRaymarch.VS.hlsl` generates the twelve cube triangles from `SV_VertexID`, then maps unit-cube XYZ into world space using:

`P = origin + U * width * x + V * height * y + W * depth * z`

No dedicated vertex/index buffer is needed for fluid volumes.

### Camera ray / oriented box intersection

The pixel shader transforms the camera ray into the domain U/V/W distance basis and performs three slab intersections against `[0,width] x [0,height] x [0,depth]`.

The proxy uses `CullMode = NONE` so it also works while the camera is inside the volume. To avoid raymarching the same pixel once on the entry face and again on the exit face, the shader keeps only the entry surface when the camera is outside and the exit surface when the camera is inside. The other proxy fragment is discarded before integration.

### Front-to-back integration

The full `[tNear,tFar]` interval is covered even when the requested sample spacing would exceed `maxSteps`. Desired spacing begins at `cellSize * stepScale`, then the actual step count is capped by the configured maximum.

Density uses Beer-Lambert-style extinction:

`alphaStep = 1 - exp(-density * absorption * stepLength)`

Color and transmittance accumulate front-to-back:

`C += T * alphaStep * sampleColor`

`T *= (1 - alphaStep)`

Temperature blends cold/hot colors and contributes a configurable simple emission term. The loop exits early when `T <= earlyExitTransmittance`.

The Forward normal blend expects straight-alpha color, so integrated premultiplied color is converted back to straight color before returning the pixel.

### Texture3D bindings and obstacle debug

The renderer transitions and binds Density at `t0`, Temperature at `t1`, and Obstacle at `t2`. Smoke mode linearly samples Density/Temperature. `ObstacleDebug` converts UVW to a voxel coordinate using the explicit grid dimensions in the 256-byte render constant block and loads the existing `R8_UINT` mask directly.

### Transparent Forward Queue integration

`GpuVolumetricFluidForwardRenderBridge` submits one `MaterialBlendMode::Transparent` item per volume. Sort depth uses the full 3D domain center including the W/depth axis.

The queued packet stores renderer/grid/domain/render settings, but the renderer resolves `CameraManager::GetActiveViewProjectionMatrix()` and `GetActiveCameraPosition()` only when the Queue executes the draw callback. Reflection Probe and other `RenderViewOverride` paths therefore keep the correct camera state.

The PSO uses normal alpha blending, depth test enabled, and depth writes disabled. Phase17.8 intentionally does not sample Scene Depth during ray integration; an opaque object located inside the volume can therefore still be overdrawn by samples behind that object. Phase17.9 owns that depth-aware clipping/composition work.

## Build integration

`Project/Directory.Build.props` registers all current Phase17 data/pass/adapter/renderer/manifest/HLSL files only for the main `Ken4lowEngine` project.

## Validation

`Project/Tests/Phase17` statically checks the current 3D foundation, including:

- Texture3D descriptor and grid contracts
- deterministic reset and compute solver stages
- 3D emitter and obstacle integration
- 256-byte CPU/HLSL raymarch render contract
- vertexless 36-vertex oriented cube proxy
- active render-view camera resolution at draw execution
- camera-ray / oriented-box slab intersection
- camera-inside and camera-outside proxy-surface selection
- Density/Temperature Texture3D sampling
- Beer-Lambert extinction and front-to-back transmittance
- maximum step bound and early exit
- existing obstacle-mask debug rendering
- Transparent Forward Queue submission and 3D center sorting
- shader manifest and build registration

A real Windows / Visual Studio / DXC / GPU build is still required after repository integration.

## Next implementation target — 17.9

Implement **Depth-aware composition / lighting**.

The next stage should add:

- Scene Depth SRV access for the active Forward render target
- conversion of scene depth into a world/view ray stop distance
- raymarch termination at the nearest opaque scene surface instead of always at volume exit
- robust handling for reversed/standard depth conventions used by the engine
- simple directional-light or scene-light extinction/scattering support
- optional shadow/transmittance approximation without a second full expensive volume solve
- reflection-view-safe depth binding and camera constants
- diagnostics for depth-clipped samples, step counts, and early exits that Phase17.10 can expose
