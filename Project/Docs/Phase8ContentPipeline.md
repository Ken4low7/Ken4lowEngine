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
- `BuildKey` derived from deterministic build inputs
- source/dependency records
- cooked output paths
- missing-output diagnostics

The manifest also contains both forward and reverse dependency maps. Reverse lookup is the basis for incremental rebuild invalidation.

`Build All Assets` now runs the manifest step after Texture → Mesh → Font conversion. A missing cooked output makes the manifest step fail instead of silently shipping an incomplete asset set.

### 8.2 Derived Data Cache — implemented

The primary cookers now share a local content-addressed cache rooted at:

`Generated/DerivedDataCache`

Texture, Mesh, and Font builds calculate a pre-conversion SHA-256 `BuildKey` from source/dependency fingerprints, build version, platform/configuration, and relevant converter settings. The same key is written into `.buildmeta.json` and reused by the Asset Manifest, so the cook pipeline and manifest no longer have separate build identities.

On a required rebuild the cooker performs:

1. calculate `BuildKey`
2. check the DDC entry
3. on hit, restore cooked output and regenerate current build metadata
4. on miss, run the converter and store the successful output in DDC

Single-file Texture/Mesh products use atomic temporary-file replacement when populating the cache. Font variants cache their multi-file texture/metadata outputs as one BuildKey-scoped entry.

Each cooker reports:

- DDC hits
- DDC misses
- restored bytes
- written bytes

`-DisableDdc` bypasses cache lookup/storage, while `-Force` deliberately rebuilds from source and refreshes the matching cache entry. The cache is disposable: source files and runtime outputs remain outside `Generated/DerivedDataCache`, so deleting the cache never removes source content.

CI statically validates the DDC contract on all three cookers and parses the PowerShell scripts on Windows before compiling C++.

### 8.3 Incremental dependency invalidation — next

Use `DependencyGraph.Reverse` to identify exactly which assets are affected by a changed source file. This will replace broad rebuild decisions with dependency-driven rebuild requests.

The first implementation should support both explicit changed paths and a saved dependency snapshot, then resolve affected `AssetId`/`BuildKey` records before deciding which cooker needs to run.

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
