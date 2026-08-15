# Phase 16 — GPU Fluid Dynamics

Phase 16 introduces a 2D Eulerian GPU fluid solver as the base for Phase 17 volumetric fluids, Phase 18 FLIP liquids, and Phase 19 fluid/rigidbody interaction.

## Design direction

- Keep simulation descriptors independent from DirectX 12 resources.
- Store simulation fields as typed `Texture2D` resources with SRV/UAV descriptors.
- Use ping-pong resources for fields that are repeatedly read and written by compute passes.
- Keep resource-state tracking inside the fluid resource layer.
- Keep CPU/HLSL constant-buffer layouts explicitly mirrored and size-checked.
- Convert Scene/Physics objects into plain renderer source data instead of passing Actor/Component objects into compute passes.
- Reuse the engine Forward Render Queue for transparent fluid presentation instead of creating a separate scene-ordering path.

## Phase 16 field layout

| Field | Format | Resources | Purpose |
|---|---|---:|---|
| Velocity | `R16G16_FLOAT` | 2 | Advection / projection / force velocity |
| Pressure | `R16_FLOAT` | 2 | Jacobi pressure solve |
| Divergence | `R16_FLOAT` | 1 | Velocity divergence |
| Density | `R16_FLOAT` | 2 | Smoke / visual density |
| Temperature | `R16_FLOAT` | 2 | Buoyancy source / debug visualization |
| Vorticity | `R16_FLOAT` | 1 | 2D curl intermediate for confinement |
| Obstacle | `R8_UINT` | 1 | Solid-cell mask / debug visualization |

At 256x256 the logical field storage is about 1.56 MiB before allocation/alignment overhead.

## Progress

- [x] 16.1 Fluid base data/API
- [x] 16.2 GPU Grid / Resource foundation
- [x] 16.3 Velocity Advection
- [x] 16.4 Divergence / Pressure / Projection
- [x] 16.5 Density / Temperature
- [x] 16.6 Vorticity / Buoyancy
- [x] 16.7 FluidEmitterComponent
- [x] 16.8 Collider / Obstacle
- [x] 16.9 Forward Rendering
  - `GpuFluidRenderDesc` visual settings separated from simulation settings
  - 192-byte CPU/HLSL render constant contract
  - world-space domain quad generated from `SV_VertexID`
  - density smoke visualization with alpha blending
  - signed temperature hot/cold debug visualization
  - obstacle mask debug visualization
  - graphics SRV path through `SRVManager`
  - depth test enabled with depth writes disabled
  - `ForwardRenderQueue` transparent packet bridge
  - active camera / render-view override support
- [ ] 16.10 Editor / Diagnostics / Stress Test

## 16.3 Velocity advection

`GpuFluidVelocityAdvectionPass` uses semi-Lagrangian backtrace and velocity ping-pong. Solid destination cells are written as zero, and a backtrace landing in a solid cell is clamped back to the current fluid cell instead of sampling through the wall.

## 16.4 Pressure projection

`GpuFluidPressureProjectionPass` makes velocity approximately divergence-free.

1. Clear pressure ping-pong textures.
2. Compute centered-difference divergence.
3. Solve `p = (pL + pR + pB + pT - div * h^2) / 4` with Jacobi iterations.
4. Subtract `grad(p)` from velocity.
5. Remove normal velocity on domain and obstacle boundaries.
6. Swap velocity.

Obstacle cells write zero divergence/pressure/velocity. For a fluid cell adjacent to a solid cell, Jacobi and projection use the center pressure for that neighbor, giving a simple Neumann pressure boundary. Projection then explicitly removes the velocity component normal to a neighboring solid cell.

With the default `pressureIterations = 40`, one pressure-projection call records 42 compute dispatches.

## 16.5 Scalar advection

`GpuFluidScalarAdvectionPass` transports density and temperature with one shared shader/PSO. `DispatchAll()` records two compute dispatches and shares one simulation constant-buffer allocation. Solid destination cells become zero and backtraces that enter a solid cell are stopped at the current fluid cell.

## 16.6 Force flow

`GpuFluidForcePass` applies vorticity confinement and buoyancy.

`omega = d(vy)/dx - d(vx)/dy`

`N = normalize(grad(abs(omega)))`

`u' = u + dt * vorticityStrength * (N_y, -N_x) * omega`

Buoyancy uses:

`F_b = buoyancy * (temperature - ambientTemperature) - smokeWeight * density`

`DispatchAll()` records three compute dispatches: curl, vorticity confinement, and buoyancy. Curl treats solid-neighbor velocities as zero, confinement does not generate curl gradients through solid cells, and all force passes write zero velocity inside solids.

## 16.7 Fluid emitter flow

`FluidEmitterComponent -> GpuFluidEmitterSource -> GpuFluidEmitterInjectionPass`

`GpuFluidDomainMapping` defines the 2D simulation plane using world-space `origin`, `axisU`, and `axisV`. Active emitters are compacted into one upload allocation and processed in one full-grid compute dispatch. The obstacle mask prevents velocity, density, and temperature from being injected into solid cells.

## 16.8 Collider / Obstacle flow

### Physics adapter

`GpuFluidColliderObstacleAdapter` reads existing `ColliderComponent` objects without changing their physics behavior. It accepts only colliders for which `Collider::IsCollisionEnabledForPhysics()` is true, so triggers and disabled/query-only colliders do not become fluid walls.

Supported primitive mapping:

- Sphere -> world-space sphere
- AABB -> world-space axis-aligned box
- OBB -> world-space oriented box with the collider's three normalized basis axes
- Capsule / Segment -> deferred; existing `ColliderComponent` does not currently build these primitives in its transform sync path

The adapter can build one source or collect all valid collider sources from an `ActorWorld`.

### World-space rasterization

Obstacle primitives stay in world space. `GpuFluidObstacleRasterPass` converts each fluid cell center back into a world point using the same domain mapping used by emitters:

`world = origin + axisU * ((x + 0.5) * cellSize) + axisV * ((y + 0.5) * cellSize)`

The compute shader tests that world point against each uploaded obstacle and writes `1` for solid or `0` for fluid into the existing `R8_UINT` obstacle texture. Because every cell is rewritten each dispatch, moving or removed colliders do not leave stale obstacle cells. If there are no active obstacles, the mask is cleared with `ClearUnorderedAccessViewUint` and no compute dispatch is recorded.

Raster binding contract:

| Root parameter | Shader register | Resource |
|---:|---|---|
| 0 | `b0` | `GpuFluidSimulationConstants` |
| 1 | `b1` | domain origin/axes, cell size, obstacle count |
| 2 | `t0` | root `StructuredBuffer<GpuFluidObstacleGpuData>` |
| 3 | `u0` | `R8_UINT` obstacle mask |

## 16.9 Forward Rendering

`GpuFluidForwardRenderer` presents the simulation as a world-space transparent domain surface. It does not own the simulation and does not enumerate Scene objects. Callers provide the existing grid, domain mapping, and `GpuFluidRenderDesc`.

### Render data contract

`GpuFluidRenderDesc` contains presentation-only settings:

- render mode: Density / Temperature / Obstacle
- smoke color
- hot/cold debug colors
- obstacle debug color
- opacity
- density visualization scale
- temperature visualization scale

`GpuFluidRenderConstants` is explicitly fixed to 192 bytes and mirrors the graphics shaders. It contains the current active view-projection matrix, world-space domain origin, world-space U/V extents, colors/scales, grid dimensions, and render mode.

The world extents are derived from the simulation description:

`extentU = normalize(axisU) * gridWidth * cellSize`

`extentV = normalize(axisV) * gridHeight * cellSize`

so changing grid resolution does not require a separate render mesh resize.

### Vertex path

`GpuFluidForward.VS.hlsl` uses `SV_VertexID` to generate two triangles directly. No vertex/index buffer is allocated for the fluid domain.

`world = origin + extentU * uv.x + extentV * uv.y`

`clip = mul(float4(world, 1), viewProjection)`

### Pixel modes

The pixel shader binds:

| Root parameter | Shader register | Resource |
|---:|---|---|
| 0 | `b0` | `GpuFluidRenderConstants` |
| 1 | `t0` | Density SRV |
| 2 | `t1` | Temperature SRV |
| 3 | `t2` | Obstacle SRV |

Density mode converts positive density to alpha using `densityScale` and uses `smokeColor` as the tint. Temperature mode visualizes signed temperature using hot/cold colors and `temperatureScale`. Obstacle mode uses integer `Load` from the `R8_UINT` mask and draws only solid cells.

The renderer uses the persistent graphics `srvIndex` descriptors from `GpuFluidGridResource` and calls `SRVManager::PreDraw()`. This intentionally does not reuse the compute SRVs in `UAVManager`; switching back to simulation is handled by the next compute pass through its normal state transition and `PreDispatch()` path.

### Forward pipeline contract

- RTV: `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`
- DSV: `DXGI_FORMAT_D24_UNORM_S8_UINT`
- normal alpha blending
- depth test: `LESS_EQUAL`
- depth write: disabled
- culling: disabled
- topology: triangle list
- one draw call: `DrawInstanced(6, 1, 0, 0)`

This matches the existing transparent GPU particle forward target/depth contract.

### Forward queue bridge

`GpuFluidForwardRenderBridge` submits each fluid domain as one `MaterialBlendMode::Transparent` packet into `ForwardRenderQueue`. Packets are stored in a `std::deque` so queue payload addresses remain stable while multiple domains are submitted in one frame.

Sort depth is calculated from the domain center and the current active camera forward vector. The draw callback asks `CameraManager` for the active view-projection matrix at execution time, so temporary render-view overrides such as reflection captures do not bake the main-camera matrix into the packet.

A runtime owner can therefore use:

`Simulation -> GpuFluidForwardRenderBridge::Submit(...) -> ForwardRenderQueue -> GpuFluidForwardRenderer::Draw(...)`

without making the renderer depend on `ActorWorld`.

### Recommended runtime order

Simulation:

`Obstacle Raster -> Emitter Injection -> Velocity Advection -> Projection -> Scalar Advection -> Vorticity/Buoyancy -> Projection`

Rendering:

`Forward Queue collection -> Opaque -> Masked -> Fluid/other Transparent -> Additive`

The simulation textures are transitioned to `PIXEL_SHADER_RESOURCE` only when the fluid packet is actually drawn. A later compute simulation step transitions them back through the existing resource-state tracker.

## Build integration

`Project/Directory.Build.props` registers all Phase 16 C++ and shader files only for the `Ken4lowEngine` project.

## Next implementation target — 16.10

Add the editor/diagnostic layer around the now-visible solver: runtime ownership/orchestration UI, render-mode controls, grid/domain visualization, pass/dispatch counters, memory and upload statistics, obstacle/emitter counts, pause/step/reset controls, and stress-test presets for resolution, pressure iterations, emitter count, and obstacle count.
