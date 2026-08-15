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
- Own final pass ordering in one runtime manager so Editor controls cannot bypass solver invariants.

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
- [x] 16.10 Editor / Diagnostics / Stress Test

**Phase 16 complete.**

## Solver flow

`GpuFluidManager` owns the grid and all Phase 16 compute/graphics passes. One fixed simulation step is recorded in this order:

`Obstacle Raster -> Emitter Injection -> Velocity Advection -> Projection -> Scalar Advection -> Vorticity/Buoyancy -> Projection`

The second projection is intentionally retained after force application so buoyancy and vorticity cannot leave the final velocity field divergent.

The runtime uses `fixedDeltaTime` plus an accumulator and limits catch-up work with `maxSubsteps`. Pause stops accumulator progress, Step records exactly one fixed step, and Reset clears every field and returns all ping-pong read indices to generation zero.

`ActorWorld::Draw()` updates the fluid after Actor updates and Physics correction have already produced the latest emitter/collider transforms. Compute work is therefore recorded before the same frame's transparent bucket and the newest density is visible immediately.

Reflection and other render-view captures may call `ActorWorld::Draw()` more than once in one engine frame. The manager guards simulation advancement with the current Frame Resource index plus its fence generation: the same World advances once, while each render view may still submit and draw the same fluid state independently.

When the active `ActorWorld` changes, the simulation is reset so density/temperature from the previous Scene does not leak into the next Scene or PIE World.

## 16.3 Velocity advection

`GpuFluidVelocityAdvectionPass` uses semi-Lagrangian backtrace and velocity ping-pong. Solid destination cells are written as zero, and a backtrace landing in a solid cell is stopped at the current fluid cell instead of sampling through the wall.

## 16.4 Pressure projection

`GpuFluidPressureProjectionPass` computes centered divergence, solves the pressure Poisson equation with Jacobi iterations, and subtracts the pressure gradient. Obstacle cells write zero divergence/pressure/velocity. Solid neighbors use center pressure as a simple Neumann boundary, and projection explicitly removes wall-normal velocity.

With the default `pressureIterations = 40`, one projection call records 42 compute dispatches. The final runtime step invokes projection twice.

## 16.5 Density / Temperature

`GpuFluidScalarAdvectionPass` transports density and signed temperature using the projected velocity field. Density and temperature share one PSO and simulation constant allocation. Solid destinations are zeroed and scalar backtraces do not pass through obstacle cells.

## 16.6 Vorticity / Buoyancy

`GpuFluidForcePass` calculates 2D curl into the dedicated `R16_FLOAT` vorticity field, applies vorticity confinement, then applies density/temperature buoyancy feedback.

`omega = d(vy)/dx - d(vx)/dy`

`N = normalize(grad(abs(omega)))`

`u' = u + dt * vorticityStrength * (N_y, -N_x) * omega`

`F_b = buoyancy * (temperature - ambientTemperature) - smokeWeight * density`

Force shaders respect the obstacle mask and never write non-zero velocity into solid cells.

## 16.7 Fluid emitters

`FluidEmitterComponent -> GpuFluidEmitterSource -> GpuFluidEmitterInjectionPass`

`GpuFluidDomainMapping` defines the 2D simulation plane through world-space `origin`, `axisU`, and `axisV`. Active emitters are compacted into one upload allocation and processed by one full-grid compute dispatch. Source rates are integrated with the fixed simulation step and obstacle cells reject injection.

## 16.8 Collider / Obstacle

`GpuFluidColliderObstacleAdapter` reads existing `ColliderComponent` objects without changing Physics behavior. It accepts only colliders for which `Collider::IsCollisionEnabledForPhysics()` is true, so triggers and disabled/query-only colliders do not become fluid walls.

Supported primitive mapping:

- Sphere -> world-space sphere
- AABB -> world-space axis-aligned box
- OBB -> world-space oriented box with three basis axes
- Capsule / Segment -> deferred to later fluid/physics expansion

`GpuFluidObstacleRasterPass` maps each fluid cell center back into world space and tests it against uploaded obstacle primitives:

`world = origin + axisU * ((x + 0.5) * cellSize) + axisV * ((y + 0.5) * cellSize)`

Every obstacle dispatch rewrites the full `R8_UINT` mask. If no obstacles are active, `ClearUnorderedAccessViewUint` removes stale cells without recording a compute dispatch.

## 16.9 Forward Rendering

`GpuFluidForwardRenderer` presents the simulation as a world-space transparent surface without owning Scene objects. `GpuFluidRenderDesc` separates presentation settings from solver settings and supports Density, Temperature, and Obstacle modes.

The 192-byte `GpuFluidRenderConstants` contract contains active view-projection, domain extents, colors/scales, grid dimensions, and render mode. The world-space quad is generated from `SV_VertexID`, so no dedicated vertex/index buffer is required.

Forward pipeline contract:

- RTV: `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`
- DSV: `DXGI_FORMAT_D24_UNORM_S8_UINT`
- normal alpha blending
- depth test: `LESS_EQUAL`
- depth write: disabled
- culling: disabled
- topology: triangle list
- draw: `DrawInstanced(6, 1, 0, 0)`

`GpuFluidForwardRenderBridge` submits one transparent packet per fluid domain into the existing `ForwardRenderQueue`. The draw callback resolves the active camera at execution time, so reflection/render-view overrides use the correct matrix.

## 16.10 Runtime / Editor / Diagnostics / Stress Test

### Runtime ownership

`GpuFluidManager` owns:

- `GpuFluidGridResource`
- Velocity Advection
- Pressure Projection
- Scalar Advection
- Vorticity / Buoyancy
- Emitter Injection
- Obstacle Raster
- Reset pass
- Forward renderer

`Framework` explicitly initializes the manager after SRV/UAV/Camera systems are available and finalizes it before descriptor managers are destroyed. Scene transitions therefore do not rebuild the solver unnecessarily, while engine shutdown returns all descriptors deterministically.

### Reset

`GpuFluidResetPass` clears both generations of all ping-pong fields plus divergence, vorticity, and obstacle. Float fields use `ClearUnorderedAccessViewFloat`; obstacle uses `ClearUnorderedAccessViewUint`. After barriers, Velocity / Pressure / Density / Temperature reset their read index to zero.

### GPU-safe grid reconfigure

Grid resolution, cell size, and pressure iteration changes are requested from the Editor but not applied while the Editor UI is drawing. The request is consumed at the beginning of the next fluid update. Only this rare reconfiguration path waits for the currently submitted fence generation before destroying and recreating textures; ordinary simulation frames add no extra GPU wait.

Grid reconfiguration preserves the world-space center of the existing fluid domain. Stress presets can therefore change resolution and cell size without visibly shifting the domain to one side.

### F12 diagnostics panel

`GpuFluidDiagnosticsPanel` is connected to the existing Editor level overlay and toggles with **F12**. Existing shortcuts remain F9 Diagnostics, F10 Render Graph, and F11 Profiler.

The panel exposes:

- Pause / Step / Reset
- Forward rendering enable
- Density / Temperature / Obstacle render mode
- opacity and visualization colors/scales
- domain origin / U/V axes
- fixed delta time / max substeps
- dissipation, vorticity, buoyancy, smoke weight, ambient temperature
- grid width / height / cell size
- pressure iterations
- current logical GPU fluid field memory
- shared `FrameUploadArena` used/capacity/high-water/overflow statistics
- Scene emitter / obstacle counts
- synthetic stress emitter / obstacle counts
- lifetime dispatch counters for each solver stage
- forward draw count
- total simulation steps / reset count / accumulator / simulation time

Upload diagnostics read the existing `FrameUploadArena::GetStats()` snapshot directly; the Fluid layer does not introduce a duplicate upload counter.

### Stress presets

Stress data uses the same `GpuFluidEmitterSource` and `GpuFluidObstacleSource` contracts as production Scene data; there is no test-only shader or alternate solver path.

| Preset | Grid | Cell size | Pressure iterations | Synthetic emitters | Synthetic obstacles |
|---|---:|---:|---:|---:|---:|
| Medium | 256x256 | 0.10 | 40 | 8 | 8 |
| Heavy | 512x512 | 0.075 | 60 | 24 | 24 |
| Extreme | 1024x1024 | 0.05 | 80 | 64 | 64 |

Custom stress counts support 0–256 synthetic emitters and obstacles. Synthetic emitters inject velocity/density/temperature through the normal injection pass, and synthetic sphere obstacles go through the normal world-space obstacle raster path.

## Build integration

`Project/Directory.Build.props` registers all Phase 16 C++, editor, and shader files only for the `Ken4lowEngine` project.

## Validation

`Project/Tests/Phase16` contains static contract tests for each Phase 16 stage, including runtime ordering, frame-generation guarding, reset coverage, GPU-safe reconfiguration, Editor controls, shared upload diagnostics, stress presets, lifecycle integration, shader/resource bindings, and forward rendering.

A real Windows / Visual Studio / DXC build and GPU runtime validation is still required after merging because the repository-only workflow cannot execute the DirectX 12 runtime here.

## Next implementation target — Phase 17

Extend the 2D solver foundation into **3D Volumetric Fluid**:

- 3D velocity / pressure / density / temperature textures
- 3D advection and projection
- volumetric obstacle representation
- 3D emitter mapping
- raymarch or slice-based volume rendering
- depth-aware composition with the existing Forward/PostEffect pipeline

Phase 16 remains the reusable 2D solver, diagnostics, and authoring foundation rather than being replaced by the Phase 17 implementation.
