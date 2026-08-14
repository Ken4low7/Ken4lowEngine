# Phase 15.1 — Culling Diagnostics and Normal Cone Foundation

## Culling Statistics

`Object3DCommon::BeginObject3DPass()` resets the Main 3D pass diagnostics every frame.

The `Culling Statistics` ImGui window now combines the existing frustum counters with surface/rasterizer diagnostics:

- Object and Mesh frustum submitted/drawn/culled counts
- tracked indexed draw calls
- submitted surface instances
- submitted triangle workload for Mesh-backed draws
- Back / Front / Two-Sided draw and triangle counts
- Main-pass PSO bind counts for Static / Alpha / Instanced / Animated paths
- a clearly labeled 50% diagnostic estimate for possible rasterizer back-face rejection on one-sided triangles
- Normal Cone candidate draw/triangle counts

The estimated rejected-triangle number is intentionally not presented as a GPU hardware counter. A D3D12 pipeline-statistics query can be added later if exact post-rasterizer primitive counts become useful.

## Normal Cone Foundation

Each `Mesh` now builds `NormalCone` metadata from indexed triangle face normals during initialization.

The cone stores:

- normalized cone axis
- minimum dot product between the axis and contained triangle normals
- valid triangle count
- degenerate triangle count

A Mesh is considered a future back-face cone-culling candidate when its cone is valid and `minDot > 0`, meaning all contained triangle normals fit inside a cone narrower than 90 degrees.

Runtime Normal Cone rejection remains **OFF** in this step. Whole SubMeshes are often too broad for useful and conservative cone rejection, especially for perspective cameras. The metadata exists now so the next step can split geometry into bounded Meshlets and evaluate a conservative per-Meshlet cone test without changing the existing correct rasterizer path.

## Next Step

1. Define Meshlet CPU metadata: vertex/index ranges, bounding sphere, Normal Cone.
2. Build Meshlets with bounded triangle/vertex budgets.
3. Add a CPU debug-only Meshlet visibility evaluator first.
4. Compare visible/rejected Meshlet counts in the Culling Statistics window without changing rendered output.
5. Move the visibility test to GPU compute only after CPU/reference results are stable.
6. Feed compacted visible Meshlets into the later HZB / ExecuteIndirect path.
