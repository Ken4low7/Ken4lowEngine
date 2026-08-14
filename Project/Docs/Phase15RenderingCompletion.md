# Phase 15 — Rendering Completion

## Goal

Phase 15 finishes the rendering paths that are still partly legacy or foundation-only before full game production begins.

The target is a renderer with explicit visibility rules, a completed Forward path, a completed Deferred path, and RenderGraph-owned frame resources where practical.

## 15.1 — Rasterizer / Surface Visibility

### Implemented baseline

- Static `Object3D` opaque and alpha rendering use back-face culling by default.
- Instanced `Object3D` rendering uses back-face culling by default.
- Skinned / animated rendering owns Back/Front/None PSO variants; existing batch call sites still use the Back default until cull-mode grouping is wired.
- Rasterizer presets explicitly define `FrontCounterClockwise = FALSE`, so Ken4lowEngine treats clockwise triangles as front faces consistently.
- `MaterialCullMode` exposes `Back / Front / None` and defaults to `Back`.
- Static opaque/alpha and Instanced Object3D create dedicated PSO variants for all three Material cull modes.
- Material ImGui exposes `Back`, `Front`, and `None (Two Sided)` without putting rasterizer state into the Material constant buffer.
- Material JSON persists `cullMode` as `back / front / none`; old or unknown input falls back to `back`.
- Static mirrored/negative-scale transforms flip `Back <-> Front` from the world-matrix handedness determinant.
- Instanced draws use the same handedness rule; if normal and mirrored instances are mixed in one draw, the renderer conservatively falls back to `None` so geometry never disappears because one PSO cannot represent two winding signs at once.
- Directional/Spot/CSM depth shadows and Point-light linear-depth shadows select the same Material cull mode as their main Object3D path.
- Editor Object-ID picking provides Back/Front/None PSOs and follows the visible-surface cull mode, including mirrored static objects and instanced fallback behavior.
- Assimp import reads `AI_MATKEY_TWOSIDED` (including glTF `doubleSided` when exposed by Assimp) and stores it as `MaterialCullMode::None` on the imported SubMesh.
- `Model` retains imported SubMesh cull modes alongside texture/sampling metadata so later render-queue work can group meshes without re-reading source assets.

Back-face culling is intentionally based on triangle winding in the D3D12 rasterizer. Vertex normals remain a lighting input; they are not used to decide whether an individual triangle is front-facing.

### Remaining 15.1 work

- Split skinned/animated batches by effective `MaterialCullMode` and route each group through the already-created Back/Front/None PSOs.
- Route imported per-SubMesh cull metadata into static/animated render grouping instead of only preserving it in `ModelData` / `Model`.
- Add a debug visualization / counter for material cull modes and estimated culled triangle workload where useful.
- Evaluate meshlet normal-cone culling after the basic material/rasterizer contract is stable.

## 15.2 — Forward Renderer Completion

Planned rendering order:

1. Shadow pass
2. Optional depth pre-pass
3. Forward opaque
4. Forward masked
5. Transparent
6. GPU particles
7. Post effects
8. UI

Required work:

- explicit render queues / material blend modes
- stable opaque/masked/transparent ordering contracts
- shared lighting/shadow bindings
- Forward diagnostics and GPU timings

## 15.3 — Deferred Renderer

Planned GBuffer baseline:

- GBuffer0: base color + AO
- GBuffer1: world/view normal + roughness
- GBuffer2: metallic + emissive/material data
- Depth

Planned flow:

1. Geometry pass writes GBuffer + depth
2. Deferred lighting resolves to HDR scene color
3. Transparent objects remain on the Forward path
4. Particles render after opaque lighting
5. Post effects consume HDR scene color
6. UI renders last

The final architecture is therefore a hybrid Deferred Opaque + Forward Transparent renderer.

## 15.4 — GPU Visibility / HZB

After Forward and Deferred produce stable depth contracts:

1. build depth pyramid / HZB
2. test object or mesh bounds in compute
3. compact visible draw work
4. issue GPU-driven draws with `ExecuteIndirect`
5. evaluate meshlet normal-cone culling

Existing Frustum Culling and CPU Occlusion Culling remain the correctness baseline during migration.

## 15.5 — RenderGraph Ownership

Incrementally migrate owner-managed frame resources into graph-owned resources:

- ShadowMap
- Depth
- GBuffer targets
- HDR scene color
- Post-effect intermediates
- BackBuffer contracts

The existing Phase 9 barrier, transient-resource, descriptor, and visualizer foundations should be reused rather than reimplemented.

## Validation

Every rendering step must keep:

- Debug and Release C++ compilation green
- D3D12 Debug Layer free of new errors/warnings
- existing Forward output as a regression reference until Deferred reaches parity
- editor picking and selection outline consistent with visible geometry
- explicit tests for pipeline state contracts

Real Windows/DX12 visual validation is still required for winding/culling changes because asset winding errors cannot be proven by compile-only CI.
