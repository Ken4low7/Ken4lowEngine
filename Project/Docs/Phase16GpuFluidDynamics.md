# Phase 16 — GPU Fluid Dynamics

Phase 16 introduces a 2D Eulerian GPU fluid solver as the base for Phase 17 volumetric fluids, Phase 18 FLIP liquids, and Phase 19 fluid/rigidbody interaction.

## Design direction

- Keep simulation descriptors independent from DirectX 12 resources.
- Store simulation fields as typed `Texture2D` resources with SRV/UAV descriptors.
- Use ping-pong resources for fields that are repeatedly read and written by compute passes.
- Keep resource-state tracking inside the fluid resource layer.
- Keep CPU/HLSL constant-buffer layouts explicitly mirrored and size-checked.
- Convert Scene/Physics objects into plain renderer source data instead of passing Actor/Component objects into compute passes.

## Phase 16 field layout

| Field | Format | Resources | Purpose |
|---|---|---:|---|
| Velocity | `R16G16_FLOAT` | 2 | Advection / projection / force velocity |
| Pressure | `R16_FLOAT` | 2 | Jacobi pressure solve |
| Divergence | `R16_FLOAT` | 1 | Velocity divergence |
| Density | `R16_FLOAT` | 2 | Smoke / visual density |
| Temperature | `R16_FLOAT` | 2 | Buoyancy source |
| Vorticity | `R16_FLOAT` | 1 | 2D curl intermediate for confinement |
| Obstacle | `R8_UINT` | 1 | Solid-cell mask |

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
  - `GpuFluidObstacleSource` / 96-byte GPU obstacle contract
  - `GpuFluidColliderObstacleAdapter` for Physics Collider collection
  - Sphere / AABB / OBB support
  - Trigger / disabled / non-physics Collider filtering through `IsCollisionEnabledForPhysics()`
  - one full-grid obstacle raster dispatch into `R8_UINT`
  - empty-obstacle GPU clear path
  - obstacle-aware velocity/scalar advection
  - obstacle-aware divergence / Jacobi / projection
  - obstacle-aware vorticity / buoyancy
  - obstacle-aware emitter injection
  - solid-cell zeroing and no-through-flow boundary behavior
- [ ] 16.9 Forward Rendering
- [ ] 16.10 Editor / Diagnostics / Stress Test

## 16.3 Velocity advection

`GpuFluidVelocityAdvectionPass` uses semi-Lagrangian backtrace and velocity ping-pong. Phase 16.8 adds an obstacle SRV: solid destination cells are written as zero, and a backtrace landing in a solid cell is clamped back to the current fluid cell instead of sampling through the wall.

## 16.4 Pressure projection

`GpuFluidPressureProjectionPass` makes velocity approximately divergence-free.

1. Clear pressure ping-pong textures.
2. Compute centered-difference divergence.
3. Solve `p = (pL + pR + pB + pT - div * h^2) / 4` with Jacobi iterations.
4. Subtract `grad(p)` from velocity.
5. Remove normal velocity on domain and obstacle boundaries.
6. Swap velocity.

Obstacle cells write zero divergence/pressure/velocity. For a fluid cell adjacent to a solid cell, Jacobi and projection use the center pressure for that neighbor, giving a simple Neumann pressure boundary. Projection then explicitly removes the velocity component normal to a neighboring solid cell.

With the default `pressureIterations = 40`, one pressure-projection call still records 42 compute dispatches.

## 16.5 Scalar advection

`GpuFluidScalarAdvectionPass` transports density and temperature with one shared shader/PSO. Solid destination cells become zero and backtraces that enter a solid cell are stopped at the current fluid cell.

## 16.6 Force flow

`GpuFluidForcePass` applies vorticity confinement and buoyancy. Curl treats solid-neighbor velocities as zero, confinement does not generate curl gradients through solid cells, and all force passes write zero velocity inside solids.

## 16.7 Fluid emitter flow

`FluidEmitterComponent -> GpuFluidEmitterSource -> GpuFluidEmitterInjectionPass`

`GpuFluidDomainMapping` defines the 2D simulation plane using world-space `origin`, `axisU`, and `axisV`. Active emitters are compacted into one upload allocation and processed in one full-grid compute dispatch. Phase 16.8 adds the obstacle mask to this pass so velocity, density, and temperature are never injected into solid cells.

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

### Recommended runtime order

`Obstacle Raster -> Emitter Injection -> Velocity Advection -> Projection -> Scalar Advection -> Vorticity/Buoyancy -> Projection`

Obstacle rasterization should happen before any stage that reads the mask. A moving physics collider can therefore update the solid mask every simulation step without rebuilding the fluid resources.

## Build integration

`Project/Directory.Build.props` registers all Phase 16 C++ and shader files only for the `Ken4lowEngine` project.

## Next implementation target — 16.9

Add forward rendering for the 2D fluid fields. Density should become the primary visual channel, with optional temperature/debug visualization, domain transform support, obstacle masking, and a renderer-facing API that does not couple the simulation passes to Scene ownership.
