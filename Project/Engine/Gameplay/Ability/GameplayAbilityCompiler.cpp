#include "GameplayAbilityCompiler.h"

#include <cmath>
#include <string>
#include <unordered_set>
#include <utility>

namespace Ken4lowEngine
{
namespace
{
	bool ValidateTagList(
		const std::vector<std::string>& source,
		const char* fieldName,
		std::vector<GameplayTag>& outTags,
		std::vector<std::string>& errors)
	{
		if (source.size() > GameplayAbilityDesc::kMaxTagCount)
		{
			errors.push_back(std::string(fieldName) + " exceeds kMaxTagCount");
			return false;
		}
		std::unordered_set<std::string> unique;
		for (const std::string& text : source)
		{
			GameplayTag tag(text);
			if (!tag.IsValid())
			{
				errors.push_back(std::string(fieldName) + " contains invalid tag: " + text);
				continue;
			}
			if (!unique.insert(text).second)
			{
				errors.push_back(std::string(fieldName) + " contains duplicate tag: " + text);
				continue;
			}
			outTags.push_back(std::move(tag));
		}
		return true;
	}

	bool ValidateOptionalEventTag(const std::string& text, const char* fieldName, std::vector<std::string>& errors)
	{
		if (text.empty()) return true;
		if (GameplayTag::IsValidString(text)) return true;
		errors.push_back(std::string(fieldName) + " is not a valid GameplayTag");
		return false;
	}
}

GameplayAbilityCompileResult GameplayAbilityCompiler::Compile(const GameplayAbilityDesc& desc)
{
	GameplayAbilityCompileResult result{};
	result.program.desc = desc;

	if (desc.schemaVersion != GameplayAbilityDesc::kSchemaVersion)
	{
		result.errors.push_back("schemaVersion is unsupported");
	}
	if (desc.abilityName.empty() || desc.abilityName.size() > 96u)
	{
		result.errors.push_back("abilityName must contain 1-96 characters");
	}
	result.program.abilityTag = GameplayTag(desc.abilityTag);
	if (!result.program.abilityTag.IsValid())
	{
		result.errors.push_back("abilityTag is invalid");
	}
	if (!std::isfinite(desc.cooldownSeconds) || desc.cooldownSeconds < 0.0f)
	{
		result.errors.push_back("cooldownSeconds must be finite and >= 0");
	}
	if (!std::isfinite(desc.durationSeconds) || desc.durationSeconds < 0.0f)
	{
		result.errors.push_back("durationSeconds must be finite and >= 0");
	}
	if (!std::isfinite(desc.cost.amount) || desc.cost.amount < 0.0f)
	{
		result.errors.push_back("cost.amount must be finite and >= 0");
	}
	if (desc.cost.amount > 0.0f && desc.cost.attribute.empty())
	{
		result.errors.push_back("cost.attribute is required when cost.amount > 0");
	}

	ValidateTagList(desc.requiredTags, "requiredTags", result.program.requiredTags, result.errors);
	ValidateTagList(desc.blockedTags, "blockedTags", result.program.blockedTags, result.errors);
	ValidateTagList(desc.grantedTags, "grantedTags", result.program.grantedTags, result.errors);

	if (desc.selfModifiers.size() > GameplayAbilityDesc::kMaxSelfModifierCount)
	{
		result.errors.push_back("selfModifiers exceeds kMaxSelfModifierCount");
	}
	for (const GameplayAbilitySelfModifierDesc& modifier : desc.selfModifiers)
	{
		if (modifier.modifier.attribute.empty()) result.errors.push_back("selfModifier.attribute is required");
		if (!std::isfinite(modifier.modifier.magnitude)) result.errors.push_back("selfModifier.magnitude must be finite");
		if (!std::isfinite(modifier.modifier.durationSeconds) || modifier.modifier.durationSeconds < 0.0f)
		{
			result.errors.push_back("selfModifier.durationSeconds must be finite and >= 0");
		}
		if (modifier.modifier.operation == GameplayModifierOperation::Multiply && modifier.modifier.magnitude < 0.0f)
		{
			result.errors.push_back("Multiply selfModifier magnitude must be >= 0");
		}
	}

	ValidateOptionalEventTag(desc.activationEventTag, "activationEventTag", result.errors);
	ValidateOptionalEventTag(desc.completionEventTag, "completionEventTag", result.errors);
	ValidateOptionalEventTag(desc.cancelEventTag, "cancelEventTag", result.errors);

	if (!std::isfinite(desc.vfxIntensity) || desc.vfxIntensity < 0.0f)
	{
		result.errors.push_back("vfxIntensity must be finite and >= 0");
	}
	if (desc.vfxCueName.empty() && !desc.vfxCueAssetPath.empty())
	{
		result.errors.push_back("vfxCueName is required when vfxCueAssetPath is set");
	}
	if (!desc.vfxCueName.empty() && desc.vfxCueAssetPath.empty())
	{
		result.warnings.push_back("VFX Cue must already be registered because vfxCueAssetPath is empty");
	}
	if (desc.targetPolicy == GameplayAbilityTargetPolicy::Self && desc.durationSeconds == 0.0f && desc.grantedTags.empty() && desc.selfModifiers.empty())
	{
		result.warnings.push_back("Self target is informational for this instant ability");
	}

	result.success = result.errors.empty();
	return result;
}

} // namespace Ken4lowEngine
