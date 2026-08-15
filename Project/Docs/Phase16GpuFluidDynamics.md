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
  - SRV/UAV allocation
  - velocity / pressure / density / temperature ping-pong fields
  - divergence / obstacle single fields
  - resource-state transition helper
  - UAV barrier helper
  - approximate field-memory diagnostics
- [ ] 16.3 Velocity Advection
- [ ] 16.4 Divergence / Pressure / Projection
- [ ] 16.5 Density / Temperature
- [ ] 16.6 Vorticity / Buoyancy
- [ ] 16.7 FluidEmitterComponent
- [ ] 16.8 Collider / Obstacle
- [ ] 16.9 Forward Rendering
- [ ] 16.10 Editor / Diagnostics / Stress Test

## Next implementation target — 16.3

Add the compute-pass/pipeline layer, bind the velocity read SRV and write UAV, dispatch semi-Lagrangian velocity advection, insert the UAV barrier, and swap the velocity ping-pong field. The same pass abstraction should then be reusable by density and temperature advection in 16.5.
