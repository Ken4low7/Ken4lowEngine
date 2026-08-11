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

### 8.3 Incremental dependency invalidation — implemented

`BuildIncrementalAssets.py` combines the manifest reverse dependency graph with a saved content snapshot:

- snapshot: `Generated/AssetPipeline/DependencySnapshot.json`
- report: `Generated/AssetPipeline/IncrementalBuildReport.json`
- entry point: `Tools/Scripts/RunBuildIncrementalAssets.bat`

Every tracked dependency and source-root file is fingerprinted by SHA-256 and size. The next incremental run compares that snapshot against the current filesystem, merges any explicit `--changed` paths, then resolves affected assets through `DependencyGraph.Reverse`.

Invalidation is transitive across cooked outputs. If an affected Font asset produces a PNG that is itself a Texture dependency, the Texture asset is also marked affected. This keeps generated-content chains correct without turning every source edit into `Build All Assets`.

New source files that do not exist in the previous manifest are still classified by source root (`Font`, `Texture`, or `Mesh`) so the corresponding cooker is selected. Removed dependencies are detected because the saved snapshot retains the previous file state.

The generated report records:

- detected and explicit changed paths
- affected `AssetId` / `BuildKey` / logical keys
- required cooker categories
- changed source paths that were not yet represented by the manifest
- whether package regeneration is required

When `--execute` is used, only those cooker categories are launched. Each existing cooker still performs its own per-asset metadata/DDC check, so unchanged assets inside the selected category remain cheap skips. After successful cooking the Asset Manifest is regenerated, required packages are refreshed, and a new dependency snapshot is committed to the generated workspace.

Example planning-only commands:

```text
python Tools/Scripts/BuildIncrementalAssets.py --project-dir .
python Tools/Scripts/BuildIncrementalAssets.py --project-dir . --changed Resources/Models/Sources/Stage/stage.gltf
```

Normal Windows execution uses:

```text
Tools\Scripts\RunBuildIncrementalAssets.bat
```

`EditorAssetBuildService` has an `Incremental` build kind backed by this entry point, allowing editor UI surfaces to opt into dependency-driven cooking without duplicating the planner logic.

### 8.4 Package / Chunk — implemented

`BuildAssetPackages.py` turns the validated Asset Manifest into deterministic runtime-only `.kpak` chunk archives under:

`Generated/Packages/Chunks`

Package policy is declared in:

`Config/AssetChunks.json`

The configuration supports:

- a default chunk
- ordered rules matching `AssetId`, `AssetType`, logical-key prefixes, or output-path prefixes
- explicit chunk-to-chunk dependencies
- generic `OwnerBindings` for future World Partition/SubLevel ownership without changing runtime `AssetId`

The current default policy keeps general content in `core`, UI/font content in `ui`, and stage meshes in `world`. `ui` and `world` depend on `core`.

Each chunk produces:

- `<chunk>.kpak` — deterministic ZIP-compatible runtime package using fixed entry timestamps/order
- `<chunk>.manifest.json` — chunk identity, dependencies, assets, file hashes, and byte counts

The package archive contains only cooked outputs under `Content/...` plus `ChunkManifest.json`; source assets and `.buildmeta.json` files are not packaged. Every payload file is recorded with SHA-256 and size so later runtime/package validation can verify integrity.

A global `Generated/Packages/PackageManifest.json` records:

- chunk package SHA-256 and size
- chunk dependency graph
- `AssetId -> ChunkId`
- total cooked/package byte counts
- optional owner-to-chunk bindings

`AssetId` is copied from the Asset Manifest and is never recomputed from package location. Moving an asset between `core`, `ui`, or `world` therefore does not change its runtime identity.

`Build All Assets` now runs:

```text
Textures -> Meshes -> Fonts -> Asset Manifest -> Asset Packages
```

`BuildIncrementalAssets.py` also fingerprints `Config/AssetChunks.json`. A chunk-policy-only edit causes repackaging without unnecessarily running Texture/Mesh/Font converters, while a successful content cook refreshes packages after the new manifest is written.

The Windows entry point is:

```text
Tools\Scripts\RunBuildAssetPackages.bat
```

`EditorAssetBuildService` exposes a `Packages` build kind and includes it in the full build sequence.

### 8.5 Deterministic cook validation — next

Final Phase 8 validation will build the same input twice and compare manifest/build keys, cooked hashes, chunk manifests, and package hashes. The goal is reproducible runtime content from identical source data.

## Boundary with later phases

Phase 8 owns import/cook/cache/dependency/package preparation. It does not optimize GPU resource barriers, RenderGraph scheduling, or parallel world updates; those remain later-system work.
