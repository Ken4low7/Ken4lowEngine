# Phase 16 — GPU Fluid Dynamics

Phase 16 introduces a 2D Eulerian GPU fluid solver as the base for Phase 17 volumetric fluids, Phase 18 FLIP liquids, and Phase 19 fluid/rigidbody interaction.

## Design direction

- Keep simulation descriptors independent from DirectX 12 resources.
- Store simulation fields as typed `Texture2D` resources with SRV/UAV descriptors.
- Use ping-pong resources for fields that are repeatedly read and written by compute passes.
- Keep resource-state tracking inside the fluid resource layer.
- Keep CPU/HLSL constant-buffer layouts explicitly mirrored and size-checked.
- Start with FP16 fields to keep bandwidth and memory pressure low enough for stress testing.

## Phase 16 field layout

| Field | Format | Resources | Purpose |
|---|---|---:|---|
| Velocity | `R16G16_FLOAT` | 2 | Advection / projection velocity |
| Pressure | `R16_FLOAT` | 2 | Jacobi pressure solve |
| Divergence | `R16_FLOAT` | 1 | Velocity divergence |
| Density | `R16_FLOAT` | 2 | Smoke / visual density |
| Temperature | `R16_FLOAT` | 2 | Buoyancy source |
| Obstacle | `R8_UINT` | 1 | Solid-cell mask |

At 256x256 the logical field storage is about 1.44 MiB before allocation/alignment overhead.

## Progress

- [x] 16.1 Fluid base data/API
  - `GpuFluidGridDesc`
  - `GpuFluidSimulationDesc`
  - 64-byte CPU/HLSL `GpuFluidSimulationConstants` contract
  - shared field classification
- [x] 16.2 GPU Grid / Resource foundation
  - Texture2D resource wrapper
  - graphics SRV + compute SRV + UAV allocation
  - velocity / pressure / density / temperature ping-pong fields
  - divergence / obstacle single fields
  - resource-state transition helper
  - UAV barrier helper
  - approximate field-memory diagnostics
- [x] 16.3 Velocity Advection
  - semi-Lagrangian backtrace
  - bilinear linear-clamp sampling
  - velocity dissipation
  - 8x8 compute dispatch
  - UAV barrier + velocity ping-pong swap
  - dedicated compute pass and shader manifest entry
- [x] 16.4 Divergence / Pressure / Projection
  - centered-difference velocity divergence
  - pressure ping-pong zero clear per projection step
  - configurable Jacobi iteration count (`pressureIterations`)
  - pressure-gradient subtraction from velocity
  - closed-domain normal velocity boundary
  - shared root signature across divergence / Jacobi / projection
- [x] 16.5 Density / Temperature
  - reusable `GpuFluidScalarAdvectionPass`
  - one shared scalar-advection shader/PSO for density and temperature
  - projected velocity sampled from `t0`
  - scalar read/write ping-pong through `t1` / `u0`
  - per-field dissipation supplied with root constants at `b1`
  - individual dispatch and combined `DispatchAll()` path
- [ ] 16.6 Vorticity / Buoyancy
- [ ] 16.7 FluidEmitterComponent
- [ ] 16.8 Collider / Obstacle
- [ ] 16.9 Forward Rendering
- [ ] 16.10 Editor / Diagnostics / Stress Test

## 16.3 Velocity Advection flow

`GpuFluidVelocityAdvectionPass` uses one simulation constant buffer, one velocity SRV, and one velocity UAV.

1. Transition velocity read texture to `NON_PIXEL_SHADER_RESOURCE`.
2. Transition velocity write texture to `UNORDERED_ACCESS`.
3. Backtrace each cell using the current velocity in world-units/sec.
4. Bilinearly sample the previous velocity field at the source position.
5. Apply `velocityDissipation`.
6. Insert a UAV barrier.
7. Transition the output to SRV state and swap the velocity ping-pong field.

## 16.4 Pressure projection flow

`GpuFluidPressureProjectionPass` makes the advected velocity approximately divergence-free before later force/scalar stages use it.

1. Clear both pressure ping-pong textures to zero using `ClearUnorderedAccessViewFloat`.
2. Compute `div(u)` into the divergence texture using centered differences.
3. Solve the projection scalar with Jacobi iterations:

   `p = (pL + pR + pB + pT - div * h^2) / 4`

4. Subtract the centered pressure gradient:

   `u' = u - grad(p)`

5. Force the normal velocity component to zero on the outer simulation boundary.
6. Insert UAV barriers and swap the velocity ping-pong output.

The pressure field here is a projection scalar rather than a physical pressure quantity. The formulation intentionally absorbs the time-step scaling into the solved correction field so projection remains `u' = u - grad(p)`.

With the default `pressureIterations = 40`, one pressure-projection call records 42 compute dispatches: one divergence pass, forty Jacobi passes, and one projection pass. Pressure clears are GPU clear commands and are not counted as compute dispatches.

Obstacle-aware neighbor sampling is intentionally deferred to 16.8. The current outer-domain pressure sampling clamps to edge cells, which acts as a simple Neumann pressure boundary, while projection explicitly removes outward normal velocity at the simulation border.

## 16.5 Scalar advection flow

`GpuFluidScalarAdvectionPass` transports density and temperature with the projected velocity field without duplicating the velocity-advection implementation into two scalar-specific passes.

Binding contract:

| Root parameter | Shader register | Resource |
|---:|---|---|
| 0 | `b0` | `GpuFluidSimulationConstants` |
| 1 | `t0` | projected velocity read texture |
| 2 | `t1` | density or temperature read texture |
| 3 | `u0` | density or temperature write texture |
| 4 | `b1` | four DWORD root constants; first float is scalar dissipation |
| static sampler | `s0` | linear clamp |

For each scalar field:

1. Read the projected velocity at the current cell.
2. Backtrace in world-units/sec using the same semi-Lagrangian convention as velocity advection.
3. Bilinearly sample the previous scalar field.
4. Apply `densityDissipation` or `temperatureDissipation` selected on the CPU.
5. Insert a UAV barrier, transition the output to SRV state, and swap that scalar ping-pong field.

`DispatchAll()` shares one simulation constant-buffer allocation between density and temperature and records two compute dispatches. The scalar shader does not clamp values to zero so temperature can later represent signed hot/cold deviations for buoyancy; source injection policy remains the responsibility of the emitter stage.

The scalar fields must contain defined initial/source data before advection. A full simulation reset/source-injection path is intentionally owned by the later runtime/emitter integration rather than hidden inside advection, because clearing every scalar dispatch would destroy newly injected density and heat.

## Build integration

`Project/Directory.Build.props` registers the Phase 16 C++ files and shader source files only for the `Ken4lowEngine` project. This keeps the existing large `.vcxproj` untouched while still making new Phase 16 sources part of the normal C++ build graph.

## Next implementation target — 16.6

Add force passes for vorticity confinement and buoyancy. Temperature and density from 16.5 will drive buoyancy, while velocity curl will drive vorticity confinement. Both forces should write through velocity ping-pong so the corrected velocity can be projected again before rendering or the next simulation step.
