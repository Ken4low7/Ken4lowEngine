#pragma once

#include "GameplayAttributeSet.h"
#include "GameplayTag.h"
#include "ActorHandle.h"
#include "Vector3.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{

enum class GameplayAbilityTargetPolicy : uint8_t
{
	None = 0,
	Self,
	OptionalActor,
	RequiredActor,
};

struct GameplayAbilityCostDesc
{
	std::string attribute;
	float amount = 0.0f;
};

struct GameplayAbilitySelfModifierDesc
{
	GameplayModifierSpec modifier{};
	bool removeOnAbilityEnd = true;
};

struct GameplayAbilityDesc
{
	static constexpr uint32_t kSchemaVersion = 1u;
	static constexpr uint32_t kMaxTagCount = 32u;
	static constexpr uint32_t kMaxSelfModifierCount = 16u;

	uint32_t schemaVersion = kSchemaVersion;
	std::string abilityName;
	std::string abilityTag;
	GameplayAbilityTargetPolicy targetPolicy = GameplayAbilityTargetPolicy::None;
	float cooldownSeconds = 0.0f;
	float durationSeconds = 0.0f;
	GameplayAbilityCostDesc cost{};
	std::vector<std::string> requiredTags;
	std::vector<std::string> blockedTags;
	std::vector<std::string> grantedTags;
	std::vector<GameplayAbilitySelfModifierDesc> selfModifiers;
	std::string activationEventTag;
	std::string completionEventTag;
	std::string cancelEventTag;
	std::string vfxCueName;
	std::string vfxCueAssetPath;
	std::string vfxIntensityParameter = "Intensity";
	float vfxIntensity = 1.0f;
	bool stopVfxOnAbilityEnd = false;
};

struct GameplayAbilityContext
{
	ActorHandle target{};
	Vector3 worldPosition{};
	Vector3 direction{};
	float strength = 1.0f;
	bool hasWorldPosition = false;
	bool hasDirection = false;
};

struct GameplayAbilityHandle
{
	uint64_t value = 0;
	[[nodiscard]] bool IsValid() const { return value != 0; }
	explicit operator bool() const { return IsValid(); }
	bool operator==(const GameplayAbilityHandle&) const = default;
};

struct GameplayAbilityProgram
{
	GameplayAbilityDesc desc{};
	GameplayTag abilityTag{};
	std::vector<GameplayTag> requiredTags;
	std::vector<GameplayTag> blockedTags;
	std::vector<GameplayTag> grantedTags;
};

struct GameplayAbilityCompileResult
{
	bool success = false;
	GameplayAbilityProgram program{};
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

} // namespace Ken4lowEngine
