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

`Build All Assets` seals the manifest only after Font -> Texture -> Mesh cooking has completed. A missing cooked output makes the manifest step fail instead of silently shipping an incomplete asset set.

### 8.2 Derived Data Cache — implemented

The primary cookers share a local content-addressed cache rooted at:

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
Fonts -> Textures -> Meshes -> Asset Manifest -> Asset Packages
```

Font is intentionally first because generated font atlas PNG files are Texture source assets. `RunBuildAssets.bat` and `EditorAssetBuildService` use the same dependency order.

`BuildIncrementalAssets.py` also fingerprints `Config/AssetChunks.json`. A chunk-policy-only edit causes repackaging without unnecessarily running Texture/Mesh/Font converters, while a successful content cook refreshes packages after the new manifest is written.

The Windows entry point is:

```text
Tools\Scripts\RunBuildAssetPackages.bat
```

`EditorAssetBuildService` exposes a `Packages` build kind and includes it in the full build sequence.

### 8.5 Deterministic cook validation — implemented

`ValidateDeterministicCook.py` performs a real two-pass full cook and compares the runtime identity/content produced by both passes.

Each pass deliberately bypasses DDC and forces the primary converters in dependency order:

```text
Fonts -Force -DisableDdc
  -> Textures -Force -DisableDdc
  -> Meshes -Force -DisableDdc
  -> Asset Manifest
  -> Asset Packages
```

By bypassing DDC, a pass cannot accidentally prove only that cached bytes are stable; the converters must reproduce the same output from the same source inputs.

After each pass the validator captures:

- complete `AssetManifest.json` SHA-256
- every `AssetId`
- every `BuildKey`
- every cooked output size and SHA-256
- `PackageManifest.json` SHA-256
- every chunk manifest SHA-256
- every `.kpak` size and SHA-256
- one canonical whole-pipeline signature SHA-256

The two signatures are recursively compared. Any changed BuildKey, cooked byte, manifest byte, chunk manifest, or package byte fails validation with exit code `2` and records the exact difference path.

Generated diagnostics are:

```text
Generated/AssetPipeline/DeterminismPass1.json
Generated/AssetPipeline/DeterminismPass2.json
Generated/AssetPipeline/DeterminismReport.json
```

Normal Windows execution uses:

```text
Tools\Scripts\RunValidateDeterministicCook.bat
```

The validation is intentionally separate from normal `Build All Assets` because it executes two full forced cooks. `EditorAssetBuildService` exposes a `Determinism` build kind for editor-side tooling without making every ordinary build pay the two-pass cost.

Phase 8 automated tests validate signature stability, cooked-byte mismatch detection, BuildKey mismatch detection, deterministic package bytes, manifest dependency behavior, DDC contracts, and incremental invalidation logic. The final project-level acceptance run still requires a Windows machine with the real converter executables so both forced cook passes can be executed.

## Phase 8 completion criteria

The Phase 8 implementation is complete when all automated tests/compilation pass. The branch is ready to merge after one local `RunValidateDeterministicCook.bat` run reports `DETERMINISTIC COOK PASSED` with zero differences.

## Boundary with later phases

Phase 8 owns import/cook/cache/dependency/package preparation. It does not optimize GPU resource barriers, RenderGraph scheduling, or parallel world updates; those remain later-system work.
