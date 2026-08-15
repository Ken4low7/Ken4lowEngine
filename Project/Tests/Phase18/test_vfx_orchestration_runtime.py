import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_runtime_owns_handles_scheduler_and_hot_reload_snapshot():
    header = read("Engine/Vfx/Runtime/VfxCueRuntime.h")
    cpp = read("Engine/Vfx/Runtime/VfxCueRuntime.cpp")

    for api in ("LoadCue", "ReloadCue", "Play", "Stop", "SetWorldPosition", "SetFloatParameter", "RunStressBurst"):
        assert api in header
    assert "VfxCueHandle" in header
    assert "instance.program = cueIt->second.program" in cpp
    assert "StartDueTracks" in cpp
    assert "StopExpiredTracks" in cpp
    assert "nextInstructionIndex" in cpp
    assert "loopGuard" in cpp
    assert "maxTrackStartsPerFrame" in cpp
    assert "budgetDelayedTrackStarts" in cpp


def test_all_six_track_types_have_adapters_and_are_not_merged_into_one_subsystem():
    header = read("Engine/Vfx/Runtime/Adapters/VfxTrackAdapters.h")
    cpp = read("Engine/Vfx/Runtime/Adapters/VfxTrackAdapters.cpp")

    for adapter in (
        "VfxParticleTrackAdapter",
        "VfxFluidTrackAdapter",
        "VfxLightTrackAdapter",
        "VfxPostEffectTrackAdapter",
        "VfxCameraShakeTrackAdapter",
    ):
        assert adapter in header

    assert "GpuParticleEffectRuntime::GetInstance()" in cpp
    assert "FluidEmitterComponent" in cpp
    assert "FluidEmitterTargetDomain::Volumetric3D" in cpp
    assert "GpuVolumetricFluidManager::GetInstance()->SetRuntimeEnabled(true)" in cpp
    assert "LightComponent::LightType::Point" in cpp
    assert "PostEffectManager::GetInstance()->EnableEffect" not in cpp  # ref-count helper owns direct calls
    assert "manager->EnableEffect(entry.effectName)" in cpp
    assert "manager->DisableEffect(entry.effectName)" in cpp
    assert "VfxCameraShakeTrackAdapter::Apply" in cpp
    assert "RestorePreviousOffset" in cpp


def test_fluid_and_light_use_transient_actor_contract_and_world_abandon_guard():
    cpp = read("Engine/Vfx/Runtime/Adapters/VfxTrackAdapters.cpp")
    runtime = read("Engine/Vfx/Runtime/VfxCueRuntime.cpp")

    assert 'actor.AddTag("__VFX_RUNTIME")' in cpp
    assert "world->MakeActorHandle(&actor)" in cpp
    assert "world->DestroyActor(actor)" in cpp or "entry.world->DestroyActor(actor)" in cpp
    assert "AbandonWorld" in cpp
    assert "adapters_.AbandonWorld(activeWorld_)" in runtime
    assert "activeWorld_ = world" in runtime


def test_user_parameters_bind_to_intensity_radius_and_loop_particle_float():
    types = read("Engine/Vfx/Data/VfxCueTypes.h")
    serializer = read("Engine/Vfx/Asset/VfxCueSerializer.cpp")
    compiler = read("Engine/Vfx/Runtime/VfxCueCompiler.cpp")
    runtime = read("Engine/Vfx/Runtime/VfxCueRuntime.cpp")

    assert "VfxCueUserParameterDesc" in types
    assert "VfxCueTrackBindingDesc" in types
    assert "IntensityScale" in types
    assert "RadiusScale" in types
    assert "ParticleFloat" in types
    assert '"userParameters"' in serializer
    assert '"bindings"' in serializer
    assert "Duplicate VFX user parameter" in compiler
    assert "ParticleFloat binding requires" in compiler
    assert "resolved.intensityScale" in runtime
    assert "resolved.radiusScale" in runtime
    assert "particleFloatOverrides" in runtime


def test_application_loop_restores_shake_updates_runtime_and_finalizes_before_scene():
    app = read("Engine/Core/Application/GameApplication.cpp")

    begin_index = app.index("VfxCueRuntime::GetInstance()->BeginFrame()")
    camera_index = app.index("defaultCamera_->Update()")
    scene_update_index = app.index("sceneManager_->Update()")
    vfx_update_index = app.index("VfxCueRuntime::GetInstance()->Update")
    finalize_vfx_index = app.index("VfxCueRuntime::GetInstance()->Finalize()")
    finalize_scene_index = app.index("sceneManager->Finalize()")

    assert begin_index < camera_index
    assert scene_update_index < vfx_update_index
    assert finalize_vfx_index < finalize_scene_index
    assert "GetSceneActorWorld()" in app


def test_timeline_editor_has_asset_edit_preview_hot_reload_and_stress_controls():
    editor = read("Engine/Vfx/Editor/VfxTimelineEditor.cpp")
    header = read("Engine/Vfx/Editor/VfxTimelineEditor.h")

    assert "VFX Timeline" in editor
    assert "LoadFromDisk" in header
    assert "SaveToDisk" in header
    assert "Register In-Memory" in editor
    assert "Hot Reload" in editor
    assert "Preview" in editor
    assert "Stop Preview" in editor
    assert "Add Track" in editor
    assert "Timeline Zoom" in editor
    assert "User Parameters" in editor
    assert "Add Intensity Binding" in editor
    assert "Runtime / Budget / Stress" in editor
    assert "Stress Burst Current Cue" in editor


def test_runtime_budget_and_diagnostics_cover_cross_system_load():
    types = read("Engine/Vfx/Runtime/VfxRuntimeTypes.h")
    runtime = read("Engine/Vfx/Runtime/VfxCueRuntime.cpp")

    for field in (
        "maxActiveInstances",
        "maxTrackStartsPerFrame",
        "maxActiveTracks",
        "maxTransientLights",
        "maxFluidTracks",
        "maxCameraShakes",
    ):
        assert field in types

    for stat in (
        "adapterFailures",
        "budgetRejectedInstances",
        "budgetDelayedTrackStarts",
        "hotReloadCount",
        "stressPlayCount",
        "peakActiveInstanceCount",
        "peakActiveTrackCount",
    ):
        assert stat in types
    assert "RunStressBurst" in runtime


def test_phase18_sample_uses_intensity_bindings_across_subsystems():
    cue = json.loads((ROOT / "Resources/Vfx/Phase18/Explosion.vfx.json").read_text(encoding="utf-8"))
    assert cue["userParameters"][0]["name"] == "Intensity"
    bound_types = {
        track["type"]
        for track in cue["tracks"]
        if any(binding["target"] == "IntensityScale" for binding in track.get("bindings", []))
    }
    assert {"VolumetricFluid", "Light", "PostEffect", "CameraShake"}.issubset(bound_types)


def test_build_registers_full_phase18_runtime_editor():
    build = read("Directory.Build.targets")
    for path in (
        "Engine\\Vfx\\Runtime\\VfxCueRuntime.cpp",
        "Engine\\Vfx\\Runtime\\Adapters\\VfxTrackAdapters.cpp",
        "Engine\\Vfx\\Editor\\VfxTimelineEditor.cpp",
        "Engine\\Vfx\\Runtime\\VfxRuntimeTypes.h",
        "Engine\\Vfx\\Runtime\\VfxCueRuntime.h",
        "Engine\\Vfx\\Editor\\VfxTimelineEditor.h",
    ):
        assert path in build
