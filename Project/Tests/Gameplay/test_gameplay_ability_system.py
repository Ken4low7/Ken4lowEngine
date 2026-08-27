import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_gameplay_tags_are_hierarchical_validated_and_deterministic():
    header = read("Engine/Gameplay/Tags/GameplayTag.h")
    cpp = read("Engine/Gameplay/Tags/GameplayTag.cpp")
    assert "IsValidString" in header
    assert "Matches(const GameplayTag& query)" in header
    assert "HasMatching" in header
    assert "std::lower_bound" in cpp
    assert "name_[query.name_.size()] == '.'" in cpp


def test_event_router_uses_actor_handles_and_stable_dispatch_order():
    header = read("Engine/Gameplay/Events/GameplayEventRouter.h")
    cpp = read("Engine/Gameplay/Events/GameplayEventRouter.cpp")
    assert "ActorHandle source" in header
    assert "ActorHandle target" in header
    assert "SubscribeAll" in header
    assert "struct PendingDelivery" in cpp
    assert "std::sort(deliveries.begin(), deliveries.end()" in cpp
    assert "delivery.callback(event)" in cpp


def test_attribute_set_separates_base_and_final_values_with_timed_modifiers():
    header = read("Engine/Gameplay/Attributes/GameplayAttributeSet.h")
    cpp = read("Engine/Gameplay/Attributes/GameplayAttributeSet.cpp")
    for api in ("Define", "SetBaseValue", "AddBaseValue", "GetBaseValue", "GetValue", "ApplyModifier", "Update"):
        assert api in header
    for operation in ("Add", "Multiply", "Override"):
        assert operation in header
    assert "remainingSeconds -= deltaTime" in cpp
    assert "GameplayModifierOperation::Multiply" in cpp


def test_ability_assets_are_versioned_compiled_and_validated_before_runtime():
    types = read("Engine/Gameplay/Ability/GameplayAbilityTypes.h")
    serializer = read("Engine/Gameplay/Ability/GameplayAbilitySerializer.cpp")
    compiler = read("Engine/Gameplay/Ability/GameplayAbilityCompiler.cpp")
    assert "kSchemaVersion = 1u" in types
    assert "GameplayAbilityProgram" in types
    assert '"targetPolicy"' in serializer
    assert '"selfModifiers"' in serializer
    assert '"events"' in serializer
    assert '"vfx"' in serializer
    assert "schemaVersion is unsupported" in compiler
    assert "contains duplicate tag" in compiler
    assert "vfxCueName is required" in compiler


def test_ability_runtime_handles_cooldown_cost_tags_target_and_program_snapshots():
    header = read("Engine/Gameplay/Ability/GameplayAbilityComponent.h")
    cpp = read("Engine/Gameplay/Ability/GameplayAbilityComponent.cpp")
    for api in ("RegisterAbility", "LoadAbility", "ReloadAbility", "TryActivateAbility", "CancelAbility", "RunStressBurst"):
        assert api in header
    assert "active.program = program" in cpp
    assert "cooldownRemaining_" in cpp
    assert "Missing required tag" in cpp
    assert "Blocked by tag" in cpp
    assert "Insufficient cost attribute" in cpp
    assert "GrantRuntimeTags" in cpp
    assert "ReleaseRuntimeTags" in cpp


def test_vfx_runtime_is_reused_by_gameplay_abilities():
    cpp = read("Engine/Gameplay/Ability/GameplayAbilityComponent.cpp")
    ability = json.loads((ROOT / "Resources/Gameplay/Abilities/Pulse.ability.json").read_text(encoding="utf-8"))
    cue = json.loads((ROOT / "Resources/Vfx/Cues/Explosion.vfx.json").read_text(encoding="utf-8"))
    effect = json.loads((ROOT / "Resources/Effects/Explosion.effect.json").read_text(encoding="utf-8"))

    assert "VfxCueRuntime::GetInstance()" in cpp
    assert "vfx->LoadCue" in cpp
    assert "vfx->Play" in cpp
    assert ability["vfx"]["cueName"] == "Explosion"
    assert ability["vfx"]["cueAssetPath"] == "Resources/Vfx/Cues/Explosion.vfx.json"
    assert cue["cueName"] == "Explosion"
    assert cue["tracks"][0]["particle"]["effectAssetPath"] == "Resources/Effects/Explosion.effect.json"
    assert cue["tracks"][0]["particle"]["effectName"] == "Explosion"
    assert effect["effectName"] == "Explosion"


def test_component_registers_with_factory_and_serializes_runtime_configuration():
    cpp = read("Engine/Gameplay/Ability/GameplayAbilityComponent.cpp")
    assert "ComponentFactory::RegisterComponentType" in cpp
    assert 'typeInfo.className = "GameplayAbilityComponent"' in cpp
    assert "typeInfo.allowMultiple = false" in cpp
    assert "AbilityAssets" in cpp
    assert "GameplayTags" in cpp
    assert "Attributes" in cpp


def test_sample_prefab_points_to_current_ability_asset():
    prefab = json.loads((ROOT / "Resources/ActorPrefabs/AbilitySampleActor.json").read_text(encoding="utf-8"))
    ability = json.loads((ROOT / "Resources/Gameplay/Abilities/Pulse.ability.json").read_text(encoding="utf-8"))
    component = next(item for item in prefab["Components"] if item["Class"] == "GameplayAbilityComponent")
    definitions = {item["Name"]: item for item in component["Attributes"]}

    assert component["AbilityAssets"] == ["Resources/Gameplay/Abilities/Pulse.ability.json"]
    assert "State.CanAct" in component["GameplayTags"]
    assert definitions["Energy"]["BaseValue"] >= ability["cost"]["amount"]
    assert "MoveSpeedScale" in definitions
    assert ability["grantedTags"] == ["State.Casting"]
    assert prefab["Name"] == "Ability Sample Actor"


def test_diagnostics_and_budgets_bound_cross_gameplay_load():
    diagnostics = read("Engine/Gameplay/Diagnostics/GameplayAbilityDiagnostics.h")
    component = read("Engine/Gameplay/Ability/GameplayAbilityComponent.cpp")
    for budget in (
        "maxRegisteredAbilitiesPerComponent",
        "maxActiveAbilitiesPerComponent",
        "maxModifiersPerComponent",
        "maxActivationsPerFramePerComponent",
    ):
        assert budget in diagnostics
    assert "RunStressBurst" in component
    assert "TryActivateAbility(abilityName)" in component


def test_build_registration_uses_current_subsystems_and_assets():
    build = read("Directory.Build.targets")
    modules = read("Build/Modules/EngineModules.json")
    for path in (
        "Engine\\Gameplay\\Tags\\GameplayTag.cpp",
        "Engine\\Gameplay\\Events\\GameplayEventRouter.cpp",
        "Engine\\Gameplay\\Attributes\\GameplayAttributeSet.cpp",
        "Engine\\Gameplay\\Ability\\GameplayAbilitySerializer.cpp",
        "Engine\\Gameplay\\Ability\\GameplayAbilityCompiler.cpp",
        "Engine\\Gameplay\\Ability\\GameplayAbilityComponent.cpp",
        "Engine\\Gameplay\\Diagnostics\\GameplayAbilityDiagnostics.cpp",
    ):
        assert path in build
    assert '"Engine/Gameplay"' in modules
    assert "Resources\\Gameplay\\Abilities\\Pulse.ability.json" in build
    assert "Resources\\ActorPrefabs\\AbilitySampleActor.json" in build
    assert "Phase19" not in build
