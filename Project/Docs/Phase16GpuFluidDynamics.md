# Phase 16 — GPU Fluid Dynamics

Phase 16 introduces a 2D Eulerian GPU fluid solver as the base for Phase 17 volumetric fluids, Phase 18 FLIP liquids, and Phase 19 fluid/rigidbody interaction.

## Design direction

- Keep simulation descriptors independent from DirectX 12 resources.
- Store simulation fields as typed `Texture2D` resources with SRV/UAV descriptors.
- Use ping-pong resources for fields that are repeatedly read and written by compute passes.
- Keep resource-state tracking inside the fluid resource layer.
- Keep CPU/HLSL constant-buffer layouts explicitly mirrored and size-checked.
- Start with FP16 fields to keep bandwidth and memory pressure low enough for stress testing.
- Keep Fluid compute SRVs and UAVs on the same shader-visible descriptor heap when one dispatch needs both.

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
  - graphics/general SRV plus compute SRV/UAV allocation
  - velocity / pressure / density / temperature ping-pong fields
  - divergence / obstacle single fields
  - resource-state transition helper
  - UAV barrier helper
  - approximate field-memory diagnostics
- [x] 16.3 Velocity Advection
  - dedicated `GpuFluidVelocityAdvectionPass`
  - shader-manifest registration through the existing `ShaderCompiler`
  - linear-clamp static sampler
  - semi-Lagrangian backtrace in cell/world units
  - 8x8 compute dispatch
  - SRV read / UAV write state transitions
  - UAV barrier and ping-pong swap
- [ ] 16.4 Divergence / Pressure / Projection
- [ ] 16.5 Density / Temperature
- [ ] 16.6 Vorticity / Buoyancy
- [ ] 16.7 FluidEmitterComponent
- [ ] 16.8 Collider / Obstacle
- [ ] 16.9 Forward Rendering
- [ ] 16.10 Editor / Diagnostics / Stress Test

## 16.3 binding contract

| Root parameter | Shader register | Resource |
|---:|---|---|
| 0 | `b0` | `GpuFluidSimulationConstants` |
| 1 | `t0` | Velocity read texture |
| 2 | `u0` | Velocity write texture |
| static sampler | `s0` | Linear clamp |

Velocity is treated as world-units per second. Backtrace distance is converted from world distance to cell distance with `invCellSize`, then from cells to normalized texture UV with `invGridWidth` / `invGridHeight`.

The output texture receives an explicit UAV barrier before it is transitioned to `NON_PIXEL_SHADER_RESOURCE` and promoted to the new ping-pong read side.

## Build registration

`Project/Directory.Build.props` adds the Phase 16 C++ files only when `MSBuildProjectName == Ken4lowEngine`. This keeps the growing GPU Fluid module out of the already large legacy `.vcxproj` item list while still making the source files part of the C++ build.

## Next implementation target — 16.4

Add divergence calculation, Jacobi pressure iteration, and velocity projection. Pressure remains ping-pong because each Jacobi iteration reads the previous pressure field and writes the next one; divergence remains a single read-only input during the pressure solve.
