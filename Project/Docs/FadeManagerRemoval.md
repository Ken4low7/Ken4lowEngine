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

## Separate LevelLoader assertion

The `LevelLoader.cpp` assertion shown during this cleanup is not a FadeManager texture failure. It means a Level JSON path could not be opened. Removing FadeManager eliminates its texture loads, but a missing Level JSON reference must be diagnosed separately from the resolved `fullPath`.
