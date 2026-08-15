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
  - dedicated `R16_FLOAT` vorticity field
  - centered-difference 2D curl
  - vorticity confinement from `grad(abs(curl))`
  - buoyancy from temperature minus density weight
  - one shared force root signature
  - velocity ping-pong after each force
  - combined `DispatchAll()` path with one simulation CB
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

`GpuFluidPressureProjectionPass` makes velocity approximately divergence-free.

1. Clear both pressure ping-pong textures to zero.
2. Compute `div(u)` using centered differences.
3. Solve with Jacobi iterations:

   `p = (pL + pR + pB + pT - div * h^2) / 4`

4. Subtract the pressure gradient:

   `u' = u - grad(p)`

5. Force the normal velocity component to zero on the outer simulation boundary.
6. Insert UAV barriers and swap velocity.

With the default `pressureIterations = 40`, one pressure-projection call records 42 compute dispatches.

## 16.5 Scalar advection flow

`GpuFluidScalarAdvectionPass` transports density and temperature with the projected velocity field.

Binding contract:

| Root parameter | Shader register | Resource |
|---:|---|---|
| 0 | `b0` | `GpuFluidSimulationConstants` |
| 1 | `t0` | projected velocity read texture |
| 2 | `t1` | density or temperature read texture |
| 3 | `u0` | density or temperature write texture |
| 4 | `b1` | scalar dissipation root constants |
| static sampler | `s0` | linear clamp |

`DispatchAll()` shares one simulation constant-buffer allocation between density and temperature and records two compute dispatches. Temperature remains signed so later buoyancy can distinguish hot and cold deviations.

## 16.6 Force flow

`GpuFluidForcePass` applies vorticity confinement and buoyancy through the existing velocity ping-pong field.

Binding contract shared by all force shaders:

| Root parameter | Shader register | Resource |
|---:|---|---|
| 0 | `b0` | `GpuFluidSimulationConstants` |
| 1 | `t0` | velocity read texture |
| 2 | `t1` | vorticity or density |
| 3 | `t2` | temperature when required |
| 4 | `u0` | vorticity or velocity write texture |

Vorticity is computed as the 2D curl z component:

`omega = d(vy)/dx - d(vx)/dy`

The confinement direction comes from the normalized gradient of curl magnitude:

`N = normalize(grad(abs(omega)))`

and the velocity correction is:

`u' = u + dt * vorticityStrength * (N_y, -N_x) * omega`

Buoyancy applies a vertical acceleration:

`F_b = buoyancy * (temperature - ambientTemperature) - smokeWeight * density`

`DispatchAll()` records three compute dispatches:

1. velocity -> vorticity curl texture
2. velocity + vorticity -> confined velocity, then velocity swap
3. velocity + density + temperature -> buoyancy velocity, then velocity swap

The force stage intentionally does not perform pressure projection internally. The runtime simulation order should project again after external forces so the final velocity remains approximately divergence-free:

`Velocity Advection -> Projection -> Scalar Advection -> Vorticity/Buoyancy -> Projection`

This separation keeps force generation independent from the pressure solver and makes future emitter, obstacle, and rigidbody forces easier to insert.

## Build integration

`Project/Directory.Build.props` registers the Phase 16 C++ files and shader source files only for the `Ken4lowEngine` project.

## Next implementation target — 16.7

Add `FluidEmitterComponent` and source-injection passes. Emitters should inject velocity, density, and temperature without clearing the simulation fields, support world-to-grid conversion, expose radius/strength/falloff parameters, and be serializable/editable through the existing Actor/Component workflow.
