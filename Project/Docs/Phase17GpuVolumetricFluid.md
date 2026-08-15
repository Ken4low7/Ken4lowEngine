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
- Render the volume through the existing Transparent Forward Queue so sorting and active render-view camera state stay consistent with the rest of the engine.
- Make Scene Depth sampling a Forward-stage contract instead of letting one volume renderer transition a shared depth target independently.
- Keep the 3D runtime disabled by default so existing Phase16 scenes do not suddenly run two fluid solvers from the same shared emitter components.

## Roadmap

- [x] 17.1 3D base data / domain API
- [x] 17.2 Texture3D Grid / Resource / Reset foundation
- [x] 17.3 3D Velocity Advection
- [x] 17.4 3D Divergence / Pressure / Projection
- [x] 17.5 3D Density / Temperature / Vorticity / Buoyancy
- [x] 17.6 3D Emitter injection
- [x] 17.7 Volumetric Collider / Obstacle raster
- [x] 17.8 Volume Raymarch Rendering
- [x] 17.9 Depth-aware composition / lighting
- [x] 17.10 Editor / Diagnostics / Stress Test

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

After 17.7 the pass samples the obstacle Texture3D. Solid destination voxels are forced to zero and backtraces do not pull velocity through a solid voxel.

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

Force application can introduce divergence, so the runtime executes another Pressure Projection after the force stage.

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

The fixed-step sequence is:

`Obstacle -> Emitter -> Velocity Advection -> Projection -> Scalar Advection -> Curl/Vorticity/Buoyancy -> Projection`

## 17.8 Volume Raymarch Rendering

`GpuVolumetricFluidRaymarchRenderer` draws a vertexless oriented unit cube with `DrawInstanced(36, 1, 0, 0)`. The vertex shader maps that cube into the U/V/W domain axes:

`P = origin + U * width * x + V * height * y + W * depth * z`

The pixel shader performs a camera-ray / oriented-box slab intersection and integrates Density/Temperature front-to-back over `[tNear,tFar]`.

Density uses Beer-Lambert-style extinction:

`alphaStep = 1 - exp(-density * absorption * stepLength)`

and the accumulated transmittance is:

`C += T * alphaStep * sampleColor`

`T *= (1 - alphaStep)`

The loop exits early when `T <= earlyExitTransmittance`. The proxy uses `CullMode = NONE`; the shader keeps only the entry surface for an outside camera and the exit surface for a camera inside the volume so the same ray is not integrated twice.

`GpuVolumetricFluidForwardRenderBridge` submits one Transparent Queue packet per volume and sorts from the complete 3D domain center. Camera state is resolved when the Queue callback executes rather than when the packet is submitted.

17.8 originally used a 256-byte render constant block. Phase17.9 extends that contract to 384 bytes to add inverse ViewProjection, Scene Depth metadata, and lighting parameters.

## 17.9 Depth-aware composition / lighting

### Shader-readable Forward depth contract

`RenderDepthContext` owns the shared Depth-read transition boundary used by transparent rendering.

For the main `D24_UNORM_S8_UINT` depth target, the resource itself is created as `R24G8_TYPELESS`. Two DSV/SRV interpretations are then used over the same resource:

- writable `D24_UNORM_S8_UINT` DSV for opaque/masked/legacy 3D drawing
- read-only `D24_UNORM_S8_UINT` DSV for transparent drawing
- `R24_UNORM_X8_TYPELESS` SRV for pixel-shader sampling

At the Transparent bucket boundary the active resource transitions:

`DEPTH_WRITE -> DEPTH_READ | PIXEL_SHADER_RESOURCE`

and the same color target is rebound with the read-only DSV. After Additive rendering it returns to `DEPTH_WRITE`.

This transition belongs to the Forward Queue instead of `GpuVolumetricFluidRaymarchRenderer`, so all transparent items see one consistent Depth state and one renderer cannot independently invalidate a shared attachment.

`RenderDepthContextStats` records prepare/restore/failure/attachment counts and Phase17.10 exposes those values in the Editor.

### Scene Depth ray termination

The volume renderer binds Scene Depth at `t3`. The 384-byte `GpuVolumetricFluidRenderConstants` contains:

- ViewProjection and inverse ViewProjection
- active camera position
- active viewport size and Depth clear value
- oriented volume axes/extents and grid XYZ
- raymarch quality settings
- directional-light and ambient-scattering parameters
- Density/Temperature/Obstacle colors and scales

For each proxy pixel, the shader loads Scene Depth and reconstructs the actual opaque world position with the active inverse ViewProjection:

`world = mul(float4(ndc.xy, sceneDepth, 1), inverseViewProjection)`

The reconstructed world point is projected onto the same camera ray to obtain `sceneRayDistance`.

The integration end becomes:

`marchEnd = min(volumeExit, sceneRayDistance)`

when an opaque surface is present. If the Depth sample equals the active clear value, the ray keeps the normal volume exit instead.

This reconstruction does not hard-code a separate standard-depth or reversed-depth linearization formula. The active projection/inverse projection defines the conversion and the active clear value defines the unwritten-depth case.

Because Scene Depth is now the ray-stop authority, the volume proxy PSO disables fixed-function Depth testing. This is required for the camera-inside case: an exit proxy face may lie behind opaque geometry even though the visible front segment of smoke must still be integrated up to that opaque surface.

### Directional scattering

At draw execution the renderer reads `LightManager` and selects the strongest enabled Directional Light. The CPU passes direction, light color, intensity, and global ambient color.

The pixel shader evaluates a Henyey-Greenstein phase approximation using configurable anisotropy:

`phase(cosTheta, g) = (1-g^2) / (4*pi*(1+g^2-2*g*cosTheta)^(3/2))`

Direct light is combined with ambient scattering before multiplying the smoke color. Temperature emission remains additive so hot smoke/fire can still glow independently from incoming light.

### One-tap volumetric self-shadow approximation

A second full light-ray march would multiply the cost of every camera sample. Phase17.9 instead samples Density once more at a configurable distance toward the Directional Light.

The current and offset densities estimate mean optical thickness:

`T_light ~= exp(-meanDensity * absorption * shadowDistance)`

`selfShadowStrength` blends between fully unshadowed direct light and that approximation. This adds at most one extra Density lookup per occupied camera-ray sample while still giving thick smoke a usable sense of internal light attenuation.

### Render-view safety

Main View depth is fully registered in `RenderDepthContext`.

Reflection/other `CameraManager::RenderViewOverride` paths currently have their own private Depth attachments but do not yet register those attachments with `RenderDepthContext`. When an override is active and no Depth override has been registered, `PrepareForShaderRead()` explicitly refuses to fall back to the Main Depth. The depth-aware volumetric draw is skipped instead of rebinding the wrong RTV/DSV and corrupting the reflection capture.

This is a deliberate safe fallback. Registering Reflection Probe / Planar Reflection depth attachments can later use the existing `PushOverride` / `PopOverride` API without changing the raymarch renderer or shader contract.

## 17.10 Editor / Diagnostics / Stress Test

### Runtime owner

`GpuVolumetricFluidManager` owns the Texture3D grid, every Phase17 compute pass, the raymarch renderer, source lists, fixed-step accumulator, reset/reconfigure state, and runtime statistics.

The manager records the complete simulation order in one place:

`Obstacle -> Emitter -> Velocity -> Projection -> Scalar -> Vorticity/Buoyancy -> Projection`

`ActorWorld::Draw()` updates the manager after actor/physics state is current and submits the volume to the existing Forward Queue before Transparent execution.

The manager checks the current frame-resource index and fence value. If Reflection Probe or another capture redraws the same `ActorWorld` inside the same engine frame, the solver update is skipped and only the already-produced Texture3D state is reused.

The 3D runtime defaults to disabled. This keeps existing Phase16 scenes unchanged even though `FluidEmitterComponent` is intentionally shared by the 2D and 3D systems. Enabling the 3D runtime performs a deterministic reset before simulation resumes.

### Scene collection

`FluidEmitterComponent::BuildVolumetricEmitterSource()` feeds the 3D source list and `GpuVolumetricFluidColliderObstacleAdapter` gathers supported physics colliders.

Runtime statistics expose both Scene and synthetic source counts plus the pass-level accepted/culled counts, making completely out-of-domain emitters and obstacles visible from the Editor instead of silently disappearing.

### Safe grid reconfigure

Grid Width, Height, Depth, cell size, and pressure iterations are editable from the diagnostics panel.

When the grid has already been allocated, reconfiguration is deferred until `UpdateFromWorld()`. The manager waits only at this rare resource-recreation boundary, destroys and recreates the Texture3D fields, resets every field, and preserves the world-space center of the oriented U/V/W domain across XYZ resolution changes.

When the runtime has not been initialized yet, the requested configuration is stored directly and used for the first allocation so an unnecessary create/destroy cycle is avoided.

### Debug visualization

The render mode now supports:

- `Smoke`
- `DensityDebug`
- `TemperatureDebug`
- `ObstacleDebug`

Density Debug shows the Density scalar without lighting. Temperature Debug maps signed temperature through the cold/hot colors. Obstacle Debug directly loads the `R8_UINT` solid mask. Smoke mode keeps the Phase17.9 Scene Depth, directional scattering, thermal emission, and one-tap self-shadow path.

### Editor controls and diagnostics

`GpuVolumetricFluidDiagnosticsPanel` is opened with **F8**. The Phase16 2D panel remains on **F12**.

The 3D panel exposes:

- runtime enable / pause / step / reset
- Forward Raymarch enable
- Smoke / Density / Temperature / Obstacle visualization
- opacity, scalar scales, absorption, emission, ray step, max steps, early-exit threshold
- directional scattering, ambient scattering, anisotropy, self-shadow strength/distance
- U/V/W domain origin and axes
- fixed timestep, max substeps, dissipation, vorticity, buoyancy, smoke weight
- grid Width / Height / Depth / cell size / pressure iterations
- logical Texture3D memory
- Scene and synthetic source counts
- emitter/obstacle accepted and culled counts
- pressure iteration count
- reset / reconfigure / failed reconfigure counts
- duplicate same-frame simulation skip count
- lifetime pass dispatch counts
- raymarch draw and Forward packet counts
- `RenderDepthContext` prepare/restore/failure/attachment counters
- shared SRV descriptor heap usage/high-water/exhaustion counters
- shared `FrameUploadArena` usage/high-water/overflow counters

### Stress presets

Two 3D presets are provided:

| Preset | Grid | Cell | Pressure | Synthetic Emitters | Synthetic Obstacles | Logical field memory |
|---|---:|---:|---:|---:|---:|---:|
| Baseline 64^3 | 64x64x64 | 0.25 | 32 | 8 | 8 | about 9.75 MiB |
| Heavy 128^3 | 128x128x128 | 0.125 | 48 | 24 | 24 | about 78 MiB |

Both preserve the same approximate 16x16x16 world-space domain size. Synthetic sources are distributed through a 3D lattice instead of a 2D row so the Z dimension is exercised as part of the stress load.

## Build integration

`Project/Directory.Build.props` registers all current Phase17 data/pass/adapter/renderer/manager/depth-context/editor/manifest/HLSL files only for the main `Ken4lowEngine` project.

## Validation

`Project/Tests/Phase17` statically checks the current 3D foundation, including:

- Texture3D descriptor and grid contracts
- deterministic reset and compute solver stages
- complete manager-owned fixed-step order
- same-engine-frame duplicate update suppression
- shared 2D/3D emitter and 3D collider collection
- XYZ center-preserving grid reconfigure
- 64^3 / 128^3 stress presets
- pass dispatch, pressure, source-culling, memory, Forward and Depth-context diagnostics
- 384-byte CPU/HLSL raymarch render contract
- vertexless 36-vertex oriented cube proxy
- active render-view camera resolution at draw execution
- camera-ray / oriented-box slab intersection
- Scene Depth `t3` binding and inverse-ViewProjection world reconstruction
- ray termination at the nearest opaque surface
- typeless D24/R24 Depth attachment and read-only DSV state
- Transparent/Additive Forward Depth transition ordering
- Directional-Light phase scattering and one-tap Density self-shadow approximation
- Density / Temperature / Obstacle debug rendering
- Reflection override safe-failure guard when no matching Depth override is registered
- shader manifest and build registration
- Editor F8 integration

A real Windows / Visual Studio / DXC / GPU build is still required after repository integration.

## Phase 17 completion

Phase17 is complete at the repository-integration level.

The engine now has a full 3D Eulerian fluid path from Texture3D allocation and deterministic reset through advection, pressure projection, scalar transport, forces, emitter injection, collider obstacles, depth-aware lit raymarch rendering, runtime ownership, Editor controls, diagnostics, and stress presets.

Known limitations intentionally left for later work:

- one global 3D volumetric domain/runtime; multiple independent volumes are not yet managed as separate instances
- Reflection Probe / Planar Reflection depth attachments are not yet registered with `RenderDepthContext`, so volumetric drawing safely skips those override captures instead of using the wrong Main Depth
- lighting uses the strongest enabled Directional Light rather than the complete scene-light list
- volumetric self-shadow is a one-tap approximation, not a full light-space transmittance volume or secondary ray march
- diagnostics are CPU/pass/context counters; there is no per-pixel GPU readback for exact ray steps, depth-clipped samples, or early-exit counts
- 128^3 already represents roughly 78 MiB of logical field storage, so higher resolutions should be treated as explicit high-cost experiments rather than normal defaults
- the 3D runtime is intentionally default-OFF to preserve existing Phase16 content until a project explicitly opts into volumetric simulation

The next useful architectural step is no longer another mandatory Phase17 solver stage. Future work can choose between multi-volume runtime ownership, Reflection-depth registration, GPU timing/readback diagnostics, or using the completed particle/fluid foundation in a full game scene.
