# Phase 16 — GPU Fluid Dynamics

Phase 16 introduces a 2D Eulerian GPU fluid solver as the base for Phase 17 volumetric fluids, Phase 18 FLIP liquids, and Phase 19 fluid/rigidbody interaction.

## Design direction

- Keep simulation descriptors independent from DirectX 12 resources.
- Store simulation fields as typed `Texture2D` resources with SRV/UAV descriptors.
- Use ping-pong resources for fields that are repeatedly read and written by compute passes.
- Keep resource-state tracking inside the fluid resource layer.
- Keep CPU/HLSL constant-buffer layouts explicitly mirrored and size-checked.
- Start with FP16 fields to keep bandwidth and memory pressure low enough for stress testing.
- Convert Scene components into plain renderer source data instead of passing Actor/Component objects into compute passes.

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
  - `SceneComponent`-based emitter with world position
  - JSON / Inspector properties for radius, velocity, density, temperature, and falloff
  - `GpuFluidEmitterSource` renderer-facing data contract
  - arbitrary 2D world-plane mapping through `GpuFluidDomainMapping`
  - CPU culling for disabled / completely out-of-domain emitters
  - one batched emitter upload and one compute dispatch
  - SRV -> UAV ping-pong without typed UAV load dependency
  - automatic `ComponentFactory` registration
- [ ] 16.8 Collider / Obstacle
- [ ] 16.9 Forward Rendering
- [ ] 16.10 Editor / Diagnostics / Stress Test

## 16.3 Velocity Advection flow

`GpuFluidVelocityAdvectionPass` uses semi-Lagrangian backtrace and velocity ping-pong.

1. Transition velocity read texture to `NON_PIXEL_SHADER_RESOURCE`.
2. Transition velocity write texture to `UNORDERED_ACCESS`.
3. Backtrace each cell using velocity in world-units/sec.
4. Bilinearly sample the source position.
5. Apply velocity dissipation.
6. Insert a UAV barrier and swap velocity.

## 16.4 Pressure projection flow

`GpuFluidPressureProjectionPass` makes velocity approximately divergence-free.

1. Clear both pressure ping-pong textures.
2. Compute centered-difference divergence.
3. Solve `p = (pL + pR + pB + pT - div * h^2) / 4` with Jacobi iterations.
4. Subtract `grad(p)` from velocity.
5. Remove normal velocity on the outer domain boundary.
6. Swap velocity.

With the default `pressureIterations = 40`, one pressure-projection call records 42 compute dispatches.

## 16.5 Scalar advection flow

`GpuFluidScalarAdvectionPass` transports density and temperature with the projected velocity field using one shared shader/PSO.

`DispatchAll()` records two compute dispatches and shares one simulation constant-buffer allocation.

## 16.6 Force flow

`GpuFluidForcePass` applies vorticity confinement and buoyancy.

`omega = d(vy)/dx - d(vx)/dy`

`N = normalize(grad(abs(omega)))`

`u' = u + dt * vorticityStrength * (N_y, -N_x) * omega`

Buoyancy uses:

`F_b = buoyancy * (temperature - ambientTemperature) - smokeWeight * density`

`DispatchAll()` records three compute dispatches: curl, vorticity confinement, and buoyancy.

## 16.7 Fluid emitter flow

`FluidEmitterComponent` inherits from `SceneComponent`, so its world position becomes the source center while its source velocity remains an explicitly editable world-space vector. The Component is serialized through `ComponentPropertyUtility` and self-registers with `ComponentFactory`.

Renderer-facing source data is separated from Scene ownership:

`FluidEmitterComponent -> GpuFluidEmitterSource -> GpuFluidEmitterInjectionPass`

### Domain mapping

`GpuFluidDomainMapping` defines the 2D simulation plane with:

- `origin`: lower-left grid corner in world space
- `axisU`: grid X direction in world space
- `axisV`: grid Y direction in world space

The axes must be non-zero and approximately orthogonal. The default mapping is world XY. World position is converted to cell coordinates on the CPU, while source velocity is projected onto the two domain axes and remains in world-units/sec.

### Batched source upload

Each active source is converted to a 48-byte `GpuFluidEmitterGpuData` element. Disabled emitters and emitters completely outside the domain are removed before upload. All remaining elements are copied into one `FrameUploadArena` allocation and bound as a root `StructuredBuffer` SRV.

Binding contract:

| Root parameter | Shader register | Resource |
|---:|---|---|
| 0 | `b0` | `GpuFluidSimulationConstants` |
| 1 | `b1` | emitter count as four root DWORDs |
| 2 | `t0` | velocity read texture |
| 3 | `t1` | density read texture |
| 4 | `t2` | temperature read texture |
| 5 | `t3` | `StructuredBuffer<GpuFluidEmitterGpuData>` |
| 6 | `u0` | velocity write texture |
| 7 | `u1` | density write texture |
| 8 | `u2` | temperature write texture |

The shader runs once over the full grid, reads the current velocity/density/temperature values, loops over the compact emitter array, accumulates source contributions, and writes all three fields to their opposite ping-pong textures. This avoids requiring typed UAV loads for the FP16 fields and keeps the same SRV-read/UAV-write contract as the other simulation passes.

Source contribution uses radial falloff:

`w = pow(saturate(1 - distance / radius), falloffExponent)`

and integrates per-second rates with the simulation step:

- `velocity += sourceVelocity * velocityStrength * w * dt`
- `density += densityRate * w * dt`
- `temperature += temperatureRate * w * dt`

After one dispatch the three outputs receive UAV barriers, transition to SRV state, and swap together. With one or many active emitters the injection stage records exactly one compute dispatch; with no active emitters it records none and does not swap fields.

Recommended runtime order is:

`Emitter Injection -> Velocity Advection -> Projection -> Scalar Advection -> Vorticity/Buoyancy -> Projection`

The pass intentionally does not own `ActorWorld` enumeration. A runtime owner can collect active `FluidEmitterComponent::BuildEmitterSource()` values and submit them as a vector, keeping Renderer code independent from Scene classes.

## Build integration

`Project/Directory.Build.props` registers all Phase 16 C++ and shader files only for the `Ken4lowEngine` project.

## Next implementation target — 16.8

Use the existing `R8_UINT` obstacle field as a solid-cell mask. Add obstacle raster/update APIs, make divergence/Jacobi/projection/force/source passes respect solid cells, and establish no-through-flow boundary behavior that can later be fed by engine Collider components.
