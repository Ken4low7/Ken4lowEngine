# Phase 8 — Content Pipeline

## Goal

Phase 8 turns the existing per-format asset conversion scripts into a deterministic content pipeline that can answer three questions reliably:

1. Which source and dependency files produced a runtime asset?
2. Which assets must be rebuilt when one source file changes?
3. Which cooked outputs belong to a package/chunk later in the pipeline?

## Existing foundation reused

The engine already has useful pieces that should not be rewritten:

- `BuildTextures.ps1` converts source textures and writes `.buildmeta.json`.
- `BuildMeshes.ps1` converts glTF/GLB/OBJ and records external dependencies.
- `BuildFonts.ps1` builds font variants and records all generated outputs.
- `EditorAssetBuildService` runs those converters without blocking the editor UI.
- Phase 3 `AssetRegistry` continues to own runtime load/reference/GC state.

Phase 8 therefore formalizes the build side instead of replacing the runtime asset system.

## Phase 8 steps

### 8.1 Deterministic Asset Manifest — implemented

`BuildAssetManifest.py` scans all `.buildmeta.json` files and emits:

`Generated/AssetPipeline/AssetManifest.json`

Each asset record contains:

- stable `AssetId` derived from type + logical source key
- `BuildKey` derived from deterministic build metadata
- source/dependency records
- cooked output paths
- missing-output diagnostics

The manifest also contains both forward and reverse dependency maps. Reverse lookup is the basis for incremental rebuild invalidation.

`Build All Assets` now runs the manifest step after Texture → Mesh → Font conversion. A missing cooked output makes the manifest step fail instead of silently shipping an incomplete asset set.

### 8.2 Derived Data Cache — next

Add a cache keyed by `BuildKey`:

- local cache root under `Generated/DerivedDataCache`
- restore output when the key already exists
- populate cache after successful conversion
- cache statistics: hit / miss / restored bytes / written bytes

The cache must be optional and disposable; deleting it must never lose source content.

### 8.3 Incremental dependency invalidation

Use `DependencyGraph.Reverse` to identify exactly which assets are affected by a changed source file. This will replace broad rebuild decisions with dependency-driven rebuild requests.

### 8.4 Package / Chunk

After deterministic build products are stable:

- assign cooked assets to chunks
- write chunk manifests
- package runtime-only files
- connect World Partition/SubLevel cells to chunk dependencies

Package generation must not make runtime `AssetId` depend on physical package location.

### 8.5 Deterministic cook validation

Final Phase 8 validation will build the same input twice and compare manifest/build keys and cooked hashes. The goal is reproducible runtime content from identical source data.

## Boundary with later phases

Phase 8 owns import/cook/cache/dependency/package preparation. It does not optimize GPU resource barriers, RenderGraph scheduling, or parallel world updates; those remain later-system work.
