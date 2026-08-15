import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_vfx_cue_has_explicit_track_types_and_typed_payloads():
    types = read("Engine/Vfx/Data/VfxCueTypes.h")

    for name in (
        "Particle",
        "Fluid2D",
        "VolumetricFluid",
        "Light",
        "PostEffect",
        "CameraShake",
    ):
        assert name in types

    for payload in (
        "VfxParticleTrackPayload",
        "VfxFluidTrackPayload",
        "VfxLightTrackPayload",
        "VfxPostEffectTrackPayload",
        "VfxCameraShakeTrackPayload",
    ):
        assert payload in types

    assert "std::variant" in types
    assert "kCurrentSchemaVersion = 1" in types
    assert "kMaxTracks = 256" in types


def test_particle_track_references_phase13_effect_instead_of_copying_emitters():
    types = read("Engine/Vfx/Data/VfxCueTypes.h")

    particle_begin = types.index("struct VfxParticleTrackPayload")
    particle_end = types.index("struct VfxFluidTrackPayload")
    particle = types[particle_begin:particle_end]

    assert "effectAssetPath" in particle
    assert "effectName" in particle
    assert "GpuParticleEmitterDesc" not in particle


def test_serializer_uses_strict_versioned_track_schema():
    serializer = read("Engine/Vfx/Asset/VfxCueSerializer.cpp")

    assert '"schemaVersion"' in serializer
    assert "VfxCueDesc::kCurrentSchemaVersion" in serializer
    assert "TryParseVfxCueTrackType" in serializer
    assert "else return false" in serializer
    assert '"particle"' in serializer
    assert '"fluid"' in serializer
    assert '"light"' in serializer
    assert '"postEffect"' in serializer
    assert '"cameraShake"' in serializer


def test_compiler_validates_payloads_and_sorts_runtime_program():
    compiler = read("Engine/Vfx/Runtime/VfxCueCompiler.cpp")

    assert "ValidateTrack" in compiler
    assert "std::get_if<VfxParticleTrackPayload>" in compiler
    assert "std::get_if<VfxFluidTrackPayload>" in compiler
    assert "std::get_if<VfxLightTrackPayload>" in compiler
    assert "std::get_if<VfxPostEffectTrackPayload>" in compiler
    assert "std::get_if<VfxCameraShakeTrackPayload>" in compiler
    assert "if (!track.enabled)" in compiler
    assert "std::stable_sort" in compiler
    assert "lhs.sourceTrackIndex < rhs.sourceTrackIndex" in compiler
    assert "compiled.duration = (std::max)(compiled.duration, instruction.endTime)" in compiler


def test_compiled_program_is_runtime_facing_and_authoring_order_independent():
    program = read("Engine/Vfx/Runtime/VfxCueProgram.h")

    assert "struct VfxCueInstruction" in program
    assert "sourceTrackIndex" in program
    assert "startTime" in program
    assert "endTime" in program
    assert "VfxCueTrackPayload payload" in program
    assert "struct VfxCueProgram" in program
    assert "std::vector<VfxCueInstruction> instructions" in program


def test_phase18_explosion_sample_composes_multiple_subsystems():
    sample_path = ROOT / "Resources/Vfx/Phase18/Explosion.vfx.json"
    cue = json.loads(sample_path.read_text(encoding="utf-8"))

    assert cue["schemaVersion"] == 1
    assert cue["cueName"] == "Phase18Explosion"
    track_types = {track["type"] for track in cue["tracks"]}
    assert {"Particle", "VolumetricFluid", "Light", "PostEffect", "CameraShake"}.issubset(track_types)

    particle = next(track for track in cue["tracks"] if track["type"] == "Particle")
    assert particle["particle"]["effectAssetPath"] == "Resources/Effects/Phase13/Explosion.effect.json"
    assert particle["particle"]["effectName"] == "Phase13Explosion"


def test_build_and_docs_register_completed_phase18():
    build = read("Directory.Build.targets")
    docs = read("Docs/Phase18UnifiedVfxOrchestration.md")

    assert "Engine\\Vfx\\Asset\\VfxCueSerializer.cpp" in build
    assert "Engine\\Vfx\\Runtime\\VfxCueCompiler.cpp" in build
    assert "Engine\\Vfx\\Runtime\\VfxCueRuntime.cpp" in build
    assert "Engine\\Vfx\\Runtime\\Adapters\\VfxTrackAdapters.cpp" in build
    assert "Engine\\Vfx\\Editor\\VfxTimelineEditor.cpp" in build
    assert "Resources\\Vfx\\Phase18\\Explosion.vfx.json" in build

    for phase in range(1, 11):
        assert f"[x] 18.{phase}" in docs
    assert "Phase 18 status: repository integration complete through 18.10" in docs
    assert "Known Boundaries" in docs
    assert "Completion Boundary" in docs
