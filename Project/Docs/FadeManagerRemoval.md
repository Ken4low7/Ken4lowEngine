# FadeManager Removal

## Summary

The legacy `FadeManager` scene-transition implementation has been removed from the runtime path. `GameApplication` no longer includes or constructs it, so `SceneManager` now uses its existing no-transition fast path and swaps the prepared scene immediately.

## Removed runtime content

- `ApplicationLayer/SceneManagement/FadeManager/FadeManager.h`
- `ApplicationLayer/SceneManagement/FadeManager/FadeManager.cpp`
- `Resources/Textures/Sources/Effects/CrackAtlas.png`
- `Resources/Textures/Compiled/Effects/CrackAtlas.dds` and its build metadata
- `Resources/Textures/Sources/Effects/black.png`
- `Resources/Textures/Compiled/Effects/black.dds` and its build metadata

`Stage/rock.dds` is intentionally retained. Its name and location make it a stage asset, so the FadeManager removal does not delete it without stronger ownership evidence.

## Build compatibility

The Visual Studio project still contains old explicit FadeManager source entries. `Directory.Build.targets` removes those retired `ClCompile` / `ClInclude` items after project evaluation so the physical source files can stay deleted without breaking MSBuild. This is a narrow compatibility shim until the generated/manual `.vcxproj` metadata is cleaned directly.

## Scene transition behavior

`SceneManager::ChangeScene` already supports a null `ISceneTransition`: when no transition object is configured it calls `ApplyNextScene()` directly. The generic `ISceneTransition` capability remains available for a future transition implementation, but no FadeManager is injected by the application.

## Validation

`Tests/test_fade_manager_removal.py` prevents the application from reintroducing the FadeManager include/constructor, verifies the physical source and fade-only texture removal, and checks that the no-transition SceneManager path remains available.

## Retired FPS LevelData validation

The assertion reported from `LevelLoader.cpp` was caused by `LevelDataValidation` automatically loading the deleted `Resources/JSON/stages/fps_stage00.json` when `DebugScene` was constructed. The old comparison tool has now been retired: its compatibility hook remains so `DebugScene` does not need an unrelated structural rewrite, but it performs no file I/O and no longer includes `LevelLoader` or `BlenderSceneLoader`.

The current Phase 4 Level pipeline (`LevelDocument`, `LevelSerializer`, `TransactionalLevelLoader`, and the `SceneLevelLoader` facade) remains intact. This cleanup removes only the obsolete FPS validation path, not the engine's current Level loading system.

`Tests/test_legacy_level_validation_retirement.py` prevents the deleted FPS JSON from being reintroduced into DebugScene startup and verifies that the transactional Level loader still exists.
