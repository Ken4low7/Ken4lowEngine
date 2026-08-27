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
    assert "std::isalnum" in cpp
    assert "if (!segmentHasCharacter) return false" in cpp


def test_event_router_uses_actor_handles_and_stable_dispatch_order():
    header = read("Engine/Gameplay/Events/GameplayEventRouter.h")
    cpp = read("Engine/Gameplay/Events/GameplayEventRouter.cpp")
    assert "ActorHandle source" in header
    assert "ActorHandle target" in header
    assert "Actor* source" not in header
    assert "SubscribeAll" in header
    assert "exactMatch" in header
    assert "struct PendingDelivery" in cpp
    assert "Callback callback" in cpp
    assert "deliveries.push_back({ id, subscriber.callback })" in cpp
    assert "std::sort(deliveries.begin(), deliveries.end()" in cpp
    assert "delivery.callback(event)" in cpp
    assert "Dispatch中のUnsubscribe/Clearは次回Publishからだけ反映" in cpp
    assert "invalidEventCount" in cpp


def test_attribute_set_separates_base_value_and_final_value():
    header = read("Engine/Gameplay/Attributes/GameplayAttributeSet.h")
    cpp = read("Engine/Gameplay/Attributes/GameplayAttributeSet.cpp")
    for api in ("Define", "SetBaseValue", "AddBaseValue", "GetBaseValue", "GetValue", "ApplyModifier", "Update"):
        assert api in header
    assert "attribute.baseValue" in cpp
    assert "ClampToDefinition" in cpp
    assert "GetDefinitions" in cpp


def test_modifiers_apply_add_multiply_override_and_timed_expiration():
    header = read("Engine/Gameplay/Attributes/GameplayAttributeSet.h")
    cpp = read("Engine/Gameplay/Attributes/GameplayAttributeSet.cpp")
    for operation in ("Add", "Multiply", "Override"):
        assert operation in header
    for stacking in ("Independent", "RefreshDuration", "Replace"):
        assert stacking in header
    add_pos = cpp.index("GameplayModifierOperation::Add")
    multiply_pos = cpp.index("GameplayModifierOperation::Multiply")
    override_pos = cpp.index("GameplayModifierOperation::Override")
    assert add_pos < multiply_pos < override_pos
    assert "remainingSeconds -= deltaTime" in cpp
    assert "modifier.remainingSeconds != -1.0f" in cpp
    assert "modifier.sequence > lastOverride->sequence" in cpp


def test_ability_asset_is_versioned_strict_and_compiled_before_runtime():
    types = read("Engine/Gameplay/Ability/GameplayAbilityTypes.h")
    serializer = read("Engine/Gameplay/Ability/GameplayAbilitySerializer.cpp")
    compiler = read("Engine/Gameplay/Ability/GameplayAbilityCompiler.cpp")
    assert "kSchemaVersion = 1u" in types
    assert "GameplayAbilityProgram" in types
    assert '"targetPolicy"' in serializer
    assert '"selfModifiers"' in serializer
    assert '"events"' in serializer
    assert '"vfx"' in serializer
    assert "TryParseTargetPolicy" in serializer
    assert "schemaVersion is unsupported" in compiler
    assert "contains duplicate tag" in compiler
    assert "vfxCueName is required" in compiler


def test_ability_runtime_has_cooldown_cost_tags_target_and_snapshot_contracts():
    header = read("Engine/Gameplay/Ability/GameplayAbilityComponent.h")
    cpp = read("Engine/Gameplay/Ability/GameplayAbilityComponent.cpp")
    actor_component = read("Engine/Scene/Actor/Core/ActorComponent.h")
    owner_handle_cpp = read("Engine/Scene/Actor/Core/ActorComponentHandle.cpp")
    for api in ("RegisterAbility", "LoadAbility", "ReloadAbility", "TryActivateAbility", "CancelAbility", "RunStressBurst"):
        assert api in header
    assert "active.program = program" in cpp
    assert "Reloadは次回Activationからだけ反映" in cpp
    assert "cooldownRemaining_" in cpp
    assert "Missing required tag" in cpp
    assert "Blocked by tag" in cpp
    assert "Insufficient cost attribute" in cpp
    assert "RequiredActor target is missing" in cpp
    assert "GrantRuntimeTags" in cpp
    assert "ReleaseRuntimeTags" in cpp
    assert "ActorHandle GetOwnerHandle() const" in actor_component
    assert "MakeActorHandle(owner_)" in owner_handle_cpp
    assert "return GetOwnerHandle();" in cpp


def test_phase19_does_not_replace_character_health_ownership():
    gameplay_header = read("Engine/Gameplay/Ability/GameplayAbilityComponent.h")
    health = read("Engine/Scene/Actor/Character/CharacterHealthComponent.h")
    docs = read("Docs/Phase19GameplayAbilitySystem.md")
    assert "CharacterHealthComponent" in health
    assert "ApplyDamage" in health
    assert "currentHealth_" in health
    assert "CharacterHealthComponentのHP責務は奪わず" in gameplay_header
    assert "CharacterHealthComponent` remains the owner" in docs


def test_phase18_vfx_is_bridged_not_duplicated():
    cpp = read("Engine/Gameplay/Ability/GameplayAbilityComponent.cpp")
    sample = json.loads((ROOT / "Resources/Gameplay/Abilities/Phase19Pulse.ability.json").read_text(encoding="utf-8"))
    assert "VfxCueRuntime::GetInstance()" in cpp
    assert "vfx->LoadCue" in cpp
    assert "vfx->Play" in cpp
    assert "vfx->SetFloatParameter" in cpp
    assert sample["vfx"]["cueName"] == "Phase18Explosion"
    assert sample["vfx"]["cueAssetPath"] == "Resources/Vfx/Phase18/Explosion.vfx.json"
    assert sample["vfx"]["intensityParameter"] == "Intensity"


def test_component_self_registers_with_existing_factory_and_json_contract():
    cpp = read("Engine/Gameplay/Ability/GameplayAbilityComponent.cpp")
    assert "ComponentFactory::RegisterComponentType" in cpp
    assert 'typeInfo.className = "GameplayAbilityComponent"' in cpp
    assert "typeInfo.allowMultiple = false" in cpp
    assert "AbilityAssets" in cpp
    assert "GameplayTags" in cpp
    assert "Attributes" in cpp


def test_phase19_editor_workflow_lives_in_actor_details_without_second_scene_editor():
    cpp = read("Engine/Gameplay/Ability/GameplayAbilityComponent.cpp")
    assert 'ImGui::CollapsingHeader("Ability Asset"' in cpp
    assert 'ImGui::Button("Load / Register Ability")' in cpp
    assert 'ImGui::Button("Activate")' in cpp
    assert 'ImGui::Button("Reload")' in cpp
    assert 'ImGui::Button("Stress x16")' in cpp
    assert 'ImGui::CollapsingHeader("Phase19 Diagnostics")' in cpp


def test_sample_prefab_defines_usable_cost_tag_and_modifier_state():
    prefab = json.loads((ROOT / "Resources/ActorPrefabs/Phase19AbilityActor.json").read_text(encoding="utf-8"))
    ability = json.loads((ROOT / "Resources/Gameplay/Abilities/Phase19Pulse.ability.json").read_text(encoding="utf-8"))
    component = next(item for item in prefab["Components"] if item["Class"] == "GameplayAbilityComponent")
    definitions = {item["Name"]: item for item in component["Attributes"]}
    assert component["AbilityAssets"] == ["Resources/Gameplay/Abilities/Phase19Pulse.ability.json"]
    assert "State.CanAct" in component["GameplayTags"]
    assert definitions["Energy"]["BaseValue"] >= ability["cost"]["amount"]
    assert "MoveSpeedScale" in definitions
    assert ability["grantedTags"] == ["State.Casting"]
    assert ability["selfModifiers"][0]["stackingPolicy"] == "RefreshDuration"


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
    for stat in (
        "activationAttempts", "activationSuccesses", "activationRejects", "eventsPublished",
        "vfxPlays", "modifiersApplied", "budgetRejects",
    ):
        assert stat in diagnostics
    assert "RunStressBurst" in component
    assert "TryActivateAbility(abilityName)" in component


def test_phase19_build_module_ci_and_docs_are_registered():
    build = read("Directory.Build.targets")
    modules = read("Build/Modules/EngineModules.json")
    workflow = read("../.github/workflows/Phase19GameplayCI.yml")
    docs = read("Docs/Phase19GameplayAbilitySystem.md")
    for path in (
        "Engine\\Gameplay\\Tags\\GameplayTag.cpp",
        "Engine\\Gameplay\\Events\\GameplayEventRouter.cpp",
        "Engine\\Gameplay\\Attributes\\GameplayAttributeSet.cpp",
        "Engine\\Gameplay\\Ability\\GameplayAbilitySerializer.cpp",
        "Engine\\Gameplay\\Ability\\GameplayAbilityCompiler.cpp",
        "Engine\\Gameplay\\Ability\\GameplayAbilityComponent.cpp",
        "Engine\\Gameplay\\Diagnostics\\GameplayAbilityDiagnostics.cpp",
        "Engine\\Scene\\Actor\\Core\\ActorComponentHandle.cpp",
    ):
        assert path in build
    assert '"Engine/Gameplay"' in modules
    assert "Tests/Phase19/run_phase19_static_tests.py" in workflow
    assert "- [x] 19.10" in docs
