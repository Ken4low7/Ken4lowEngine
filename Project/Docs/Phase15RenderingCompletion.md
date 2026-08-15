# Phase 15 — Rendering Completion

## Status

**Phase 15 status: Complete after 15.8 validation/diagnostics closure.**

Phase 15 completes the production-ready Forward rendering baseline used by the current game engine. It does not require a Deferred renderer, HZB occlusion, or full RenderGraph ownership before game production can begin. Those are future renderer upgrades and are intentionally moved out of Phase 15.

The completed baseline is:

- explicit surface culling and winding rules
- Material-driven `Opaque / Masked / Transparent / Additive` classification
- one shared Forward queue contract across static, instanced, animated, and skeletal rendering
- GPU Particle integration without converting GPU-driven particles into CPU draw items
- stable transparent ordering contracts
- diagnostics that preserve the previous frame after `EndFrame()`
- automated Phase 15 regression coverage

## 15.1 — Rasterizer / Surface Visibility

### Completed

- Static `Object3D` opaque and alpha rendering use back-face culling by default.
- Static rendering routes imported per-SubMesh cull metadata into Main, Shadow, and Editor Object-ID draws.
- `MaterialCullMode` exposes `Back / Front / None` and defaults to `Back`.
- Material JSON persists `cullMode` as `back / front / none`.
- Mirrored / negative-scale transforms resolve winding consistently.
- Instanced rendering falls back to Two-Sided when normal and mirrored instances share one draw and one PSO cannot represent both winding signs.
- Animated/skinned rendering owns Back/Front/None PSO variants and preserves imported SubMesh cull modes through animation LOD data.
- Shadow and Editor Picking paths follow the same effective surface cull contract.
- Assimp import preserves two-sided metadata when available.
- `CullingDiagnostics` tracks Main-pass surface workload without mixing Shadow, Picking, Debug Wireframe, Particle, or Editor overlays.
- Visibility Meshlet and Normal Cone metadata are available as a diagnostic/reference foundation while runtime rejection remains conservative.

### Validation note

Runtime Normal Cone rejection remains intentionally disabled until representative Windows/DX12 visual comparison proves the reference sign/winding contract. This is not a blocker for the Forward renderer used by game production.

## 15.2 — Transparent Forward Migration

### Completed

- `MaterialBlendMode` defines `Opaque / Masked / Transparent / Additive` independently from low-level D3D12 blend presets.
- `ForwardRenderPolicy` maps Material classification to:
  - Forward bucket
  - low-level blend state
  - sort direction
  - depth-write contract
  - alpha-test contract
- `ForwardRenderQueue` owns stable item collection, sorting, callback execution, and frame serials.
- Transparent static models use normal alpha blending, depth-read/no-depth-write, and BackToFront queue ordering.
- Queue ownership prevents queued components from being redrawn by the legacy `Actor::Draw()` compatibility path.

## 15.3 — Additive Forward Migration

### Completed

- Additive Forward PSOs use `SrcAlpha + DestOne` and depth-read/no-depth-write.
- Static `Object3D` selects Opaque/Masked/Transparent/Additive render state from Material classification.
- `ModelComponent` submits all four Forward bucket classes.
- `ActorWorld` executes Additive after Transparent.

The canonical Forward execution order is:

1. Opaque
2. Masked
3. legacy / unmigrated Actor 3D compatibility draw
4. Transparent
5. Additive

## 15.4 — Shared Forward Contract + Instanced Opaque/Masked

### Completed

- `MakeForwardRenderItem()` is the renderer-independent item builder.
- Legacy Object3D alpha enablement bridges into `MaterialBlendMode::Transparent`.
- Instanced renderers expose Material classification to components.
- Instanced Opaque/Masked rendering enters the same Forward queue contract as static models.
- Queue frame serials suppress direct-draw duplication.

## 15.5 — Instanced Transparent/Additive

### Completed

- Instanced Alpha/Additive PSOs use depth-read/no-depth-write.
- Instanced Transparent/Additive batches participate in the same Forward buckets as static models.
- Transparent/Additive instance streams are sorted BackToFront without reordering the persistent editor/source instance list.
- Opaque/Masked retain their faster non-transparent path.
- One CPU Forward item represents one instanced renderer batch; the engine intentionally does not flatten every instance into a CPU draw item.

This preserves `DrawInstanced` batching while still providing correct intra-batch transparent ordering.

## 15.6 — Animated / Skeletal Forward Integration

### Completed

- Animated and skeletal renderers participate in all four Material Forward classifications.
- Transparent/Additive animation PSOs use depth-read/no-depth-write.
- Static, non-CS animated, and skinned draw paths keep their existing skinning/root binding contracts.
- Animated and skeletal components submit one Forward item per component/renderer rather than per bone or per meshlet.
- Queue ownership prevents the normal Actor draw path from executing the same component again.
- Shadow and Editor paths remain separate from the Main Forward queue.

## 15.7 — GPU Particle Forward Integration

### Completed

- GPU Particle rendering is no longer issued as a separate `GameApplication` draw after the scene Forward pass.
- `GpuParticleForwardRenderBridge` submits GPU Particle work as system-level packets.
- Transparent particles enter the Transparent bucket.
- Additive particles enter the Additive bucket.
- The CPU Forward queue never submits one item per particle.
- Existing GPU visible-particle compaction, Alpha depth sorting, and `ExecuteIndirect` rendering remain GPU-driven.
- Sprite and Mesh particle pipelines remain depth-read/no-depth-write.
- GPU Particle packet diagnostics expose Transparent/Additive packet counts per submitted queue frame.

### Compatibility contract

The authored Effect Runtime stores BlendMode in the packed GPU particle `drawType`, so new effect assets preserve their explicit Alpha/Additive/Multiply selection through rendering.

Legacy untagged GPU particle `drawType` values retain the existing Additive fallback for compatibility. New production game effects should use the Effect Runtime authoring path when explicit BlendMode control is required.

## 15.8 — Validation / Diagnostics / Closure

### Completed

- `ForwardRenderQueue` preserves a `ForwardRenderFrameStats` snapshot after `EndFrame()`.
- The snapshot records:
  - frame serial
  - submitted item count per bucket
  - total submitted items
  - executed buckets
  - bucket execution sequence
  - rejected submissions
  - duplicate bucket execution requests
- The queue validates the canonical Opaque -> Masked -> Transparent -> Additive bucket sequence.
- Repeated execution of the same bucket in one frame is rejected before callbacks are invoked again, preventing accidental double alpha/additive composition.
- GPU Particle diagnostics separately report Transparent and Additive system packet counts.
- `forward_render_validation_matrix.json` defines the expected render-path/bucket matrix and a repeatable manual mixed-scene recipe.
- Phase 15 closure tests verify that static, instanced, animated, skeletal, and GPU Particle paths remain connected to the expected Forward contracts.

## Forward Production Contract

| Render path | Opaque | Masked | Transparent | Additive |
| --- | --- | --- | --- | --- |
| Static Model | Yes | Yes | Yes | Yes |
| Instanced Model | Yes | Yes | Yes | Yes |
| Animated Model | Yes | Yes | Yes | Yes |
| Skeletal Mesh | Yes | Yes | Yes | Yes |
| GPU Particle | N/A | N/A | Yes | Yes |

Opaque/Masked write depth and sort FrontToBack. Transparent/Additive do not write depth and sort BackToFront at the CPU object/batch level. GPU Particle Alpha sorting remains GPU-owned inside its system packet.

## Manual Visual Validation Recipe

Use `Project/Tests/Phase15/forward_render_validation_matrix.json` as the reference layout. A representative scene should contain:

- one near Opaque static object
- one Masked static object
- a far and near Transparent pair from different render paths
- one Additive animated/skeletal surface
- one Alpha GPU Particle effect
- one Additive GPU Particle effect

Verify that:

1. Opaque and Masked establish depth before transparent work.
2. Transparent objects do not overwrite depth.
3. Far transparent surfaces composite before near transparent surfaces where they are separate CPU Forward items.
4. Instanced Transparent particles/instances keep internal BackToFront ordering.
5. Alpha GPU Particle ordering remains stable under camera movement.
6. Additive surfaces and particles execute after normal alpha composition.
7. Editor Picking, Shadow rendering, and Selection Outline remain unchanged.

## Validation Gate

Phase 15 code changes should keep:

- automated project validation green
- Debug C++ compilation green
- Release C++ compilation green
- GPU Particle HLSL validation green
- no new broken asset references
- no duplicate Forward bucket execution
- canonical Forward bucket execution order

Windows/DX12 visual testing remains a release/QA validation step for asset-specific winding and blend appearance; it is no longer an architectural blocker for closing Phase 15.

## Future Rendering Work — Not Phase 15 Blockers

The following remain valuable engine upgrades, but they are deliberately outside Phase 15 and can be developed after game production has started:

### Deferred / Hybrid Renderer

Potential target:

1. GBuffer geometry pass for opaque surfaces
2. Deferred lighting into HDR scene color
3. Forward Transparent/Additive reuse the completed Phase 15 queue
4. GPU Particle remains after opaque lighting
5. Post effects consume HDR scene color

The likely long-term architecture is **Deferred Opaque + Forward Transparent**.

### GPU Visibility / HZB

Potential target:

1. build a depth pyramid
2. test object/mesh bounds in compute
3. compact visible draw work
4. issue GPU-driven draws
5. migrate validated Meshlet/Normal Cone rules to GPU visibility where safe

### RenderGraph Resource Ownership

Potential future migrations include:

- ShadowMap
- Depth
- GBuffer targets
- HDR scene color
- Post-effect intermediates
- BackBuffer contracts

These improvements should reuse the existing RenderGraph foundation rather than replace the now-stable Forward renderer.
