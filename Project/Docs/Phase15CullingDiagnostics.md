# Phase 15.1 — Culling Diagnostics and Normal Cone Foundation

## Culling Statistics

`Object3DCommon::BeginObject3DPass()` resets the Main 3D pass diagnostics every frame, and `EndObject3DPass()` closes the diagnostic scope immediately after the Scene 3D draw so Debug/Particle/Editor rendering does not contaminate the counters.

The `Culling Statistics` ImGui window combines the existing frustum counters with surface/rasterizer diagnostics:

- Object and Mesh frustum submitted/drawn/culled counts
- tracked indexed draw calls
- submitted surface instances
- submitted triangle workload for Mesh-backed draws
- Back / Front / Two-Sided draw and triangle counts
- Main-pass PSO bind counts for Static / Alpha / Instanced / Animated paths
- a clearly labeled 50% diagnostic estimate for possible rasterizer back-face rejection on one-sided triangles
- Visibility Meshlet instance count
- Normal Cone candidate Meshlet / triangle counts

The estimated rejected-triangle number is intentionally not presented as a GPU hardware counter. A D3D12 pipeline-statistics query can be added later if exact post-rasterizer primitive counts become useful.

## Normal Cone Foundation

Each `Mesh` builds `NormalCone` metadata from indexed triangle face normals during initialization.

The cone stores:

- normalized cone axis
- minimum dot product between the axis and contained triangle normals
- valid triangle count
- degenerate triangle count

A surface cluster is considered a future back-face cone-culling candidate when its cone is valid and `minDot > 0`, meaning all contained triangle normals fit inside a cone narrower than 90 degrees.

## Visibility Meshlet Foundation

Each `Mesh` also builds CPU-only `VisibilityMeshlet` metadata while keeping the existing VB/IB and full Draw call unchanged.

The default reference budget is:

- maximum 64 unique vertices
- maximum 126 triangles

Every Visibility Meshlet retains a contiguous range in the original index buffer and stores:

- `startIndex` / `indexCount`
- triangle count
- unique vertex count
- local bounding sphere
- Normal Cone

This is deliberately metadata-only. Runtime Normal Cone rejection remains **OFF**, so this step cannot remove visible geometry. The live diagnostics report how much Meshlet workload is a possible future Normal Cone candidate before any draw suppression is enabled.

Animated/skinned rendering already reports its Surface PSO binds, while Meshlet triangle/candidate statistics currently describe `Mesh`-backed Static/Alpha/Instanced rendering. Skinned Meshlet generation will be added only after the CPU reference test is stable.

## Next Step

1. Add a CPU debug-only per-Meshlet visibility evaluator using Meshlet bounds, camera position, and Normal Cone.
2. Report evaluated / visible / rejected Meshlet counts without changing rendered output.
3. Add deterministic tests for front-facing, back-facing, wide-cone, mirrored, and Two-Sided cases.
4. Compare CPU reference results against actual rendered geometry on Windows/DX12.
5. Move the visibility test to GPU compute only after the CPU/reference results are stable.
6. Feed compacted visible Meshlets into the later HZB / ExecuteIndirect path.
