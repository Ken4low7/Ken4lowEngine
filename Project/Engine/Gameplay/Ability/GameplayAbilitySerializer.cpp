#include "GameplayAbilitySerializer.h"

#include "JsonFileIO.h"
#include "JsonReadUtil.h"

#include <json.hpp>
#include <utility>

namespace Ken4lowEngine
{
namespace
{
	using json = nlohmann::json;

	const char* TargetPolicyToString(GameplayAbilityTargetPolicy policy)
	{
		switch (policy)
		{
		case GameplayAbilityTargetPolicy::None: return "None";
		case GameplayAbilityTargetPolicy::Self: return "Self";
		case GameplayAbilityTargetPolicy::OptionalActor: return "OptionalActor";
		case GameplayAbilityTargetPolicy::RequiredActor: return "RequiredActor";
		default: return "None";
		}
	}

	bool TryParseTargetPolicy(const std::string& value, GameplayAbilityTargetPolicy& outPolicy)
	{
		if (value == "None") outPolicy = GameplayAbilityTargetPolicy::None;
		else if (value == "Self") outPolicy = GameplayAbilityTargetPolicy::Self;
		else if (value == "OptionalActor") outPolicy = GameplayAbilityTargetPolicy::OptionalActor;
		else if (value == "RequiredActor") outPolicy = GameplayAbilityTargetPolicy::RequiredActor;
		else return false;
		return true;
	}

	const char* ModifierOperationToString(GameplayModifierOperation operation)
	{
		switch (operation)
		{
		case GameplayModifierOperation::Add: return "Add";
		case GameplayModifierOperation::Multiply: return "Multiply";
		case GameplayModifierOperation::Override: return "Override";
		default: return "Add";
		}
	}

	bool TryParseModifierOperation(const std::string& value, GameplayModifierOperation& outOperation)
	{
		if (value == "Add") outOperation = GameplayModifierOperation::Add;
		else if (value == "Multiply") outOperation = GameplayModifierOperation::Multiply;
		else if (value == "Override") outOperation = GameplayModifierOperation::Override;
		else return false;
		return true;
	}

	const char* StackingPolicyToString(GameplayModifierStackingPolicy policy)
	{
		switch (policy)
		{
		case GameplayModifierStackingPolicy::Independent: return "Independent";
		case GameplayModifierStackingPolicy::RefreshDuration: return "RefreshDuration";
		case GameplayModifierStackingPolicy::Replace: return "Replace";
		default: return "Independent";
		}
	}

	bool TryParseStackingPolicy(const std::string& value, GameplayModifierStackingPolicy& outPolicy)
	{
		if (value == "Independent") outPolicy = GameplayModifierStackingPolicy::Independent;
		else if (value == "RefreshDuration") outPolicy = GameplayModifierStackingPolicy::RefreshDuration;
		else if (value == "Replace") outPolicy = GameplayModifierStackingPolicy::Replace;
		else return false;
		return true;
	}

	bool ReadStringArray(const json& root, const char* key, std::vector<std::string>& outValues)
	{
		outValues.clear();
		const auto it = root.find(key);
		if (it == root.end()) return true;
		if (!it->is_array()) return false;
		for (const json& value : *it)
		{
			if (!value.is_string()) return false;
			outValues.push_back(value.get<std::string>());
		}
		return true;
	}

	bool ReadSelfModifiers(const json& root, std::vector<GameplayAbilitySelfModifierDesc>& outModifiers)
	{
		outModifiers.clear();
		const auto it = root.find("selfModifiers");
		if (it == root.end()) return true;
		if (!it->is_array()) return false;
		for (const json& source : *it)
		{
			if (!source.is_object()) return false;
			GameplayAbilitySelfModifierDesc desc{};
			desc.modifier.attribute = JsonReadUtil::ReadStringOr(source, "attribute", {});
			std::string operationText = JsonReadUtil::ReadStringOr(source, "operation", "Add");
			if (!TryParseModifierOperation(operationText, desc.modifier.operation)) return false;
			desc.modifier.magnitude = JsonReadUtil::ReadFloatOr(source, "magnitude", desc.modifier.magnitude);
			desc.modifier.durationSeconds = JsonReadUtil::ReadFloatOr(source, "durationSeconds", desc.modifier.durationSeconds);
			desc.modifier.stackKey = JsonReadUtil::ReadStringOr(source, "stackKey", {});
			std::string stackText = JsonReadUtil::ReadStringOr(source, "stackingPolicy", "Independent");
			if (!TryParseStackingPolicy(stackText, desc.modifier.stackingPolicy)) return false;
			desc.removeOnAbilityEnd = JsonReadUtil::ReadBoolOr(source, "removeOnAbilityEnd", desc.removeOnAbilityEnd);
			outModifiers.push_back(std::move(desc));
		}
		return true;
	}
}

bool GameplayAbilitySerializer::Load(GameplayAbilityDesc& outDesc, const std::string& filePath)
{
	nlohmann::json root;
	if (!JsonFileIO::LoadJsonFile(filePath, root) || !root.is_object()) return false;

	GameplayAbilityDesc desc{};
	desc.schemaVersion = static_cast<uint32_t>(JsonReadUtil::ReadIntOr(root, "schemaVersion", 0));
	if (desc.schemaVersion != GameplayAbilityDesc::kSchemaVersion) return false;
	desc.abilityName = JsonReadUtil::ReadStringOr(root, "abilityName", {});
	desc.abilityTag = JsonReadUtil::ReadStringOr(root, "abilityTag", {});
	std::string targetPolicy = JsonReadUtil::ReadStringOr(root, "targetPolicy", "None");
	if (!TryParseTargetPolicy(targetPolicy, desc.targetPolicy)) return false;
	desc.cooldownSeconds = JsonReadUtil::ReadFloatOr(root, "cooldownSeconds", desc.cooldownSeconds);
	desc.durationSeconds = JsonReadUtil::ReadFloatOr(root, "durationSeconds", desc.durationSeconds);

	if (root.contains("cost"))
	{
		if (!root.at("cost").is_object()) return false;
		const json& cost = root.at("cost");
		desc.cost.attribute = JsonReadUtil::ReadStringOr(cost, "attribute", {});
		desc.cost.amount = JsonReadUtil::ReadFloatOr(cost, "amount", 0.0f);
	}

	if (!ReadStringArray(root, "requiredTags", desc.requiredTags) ||
		!ReadStringArray(root, "blockedTags", desc.blockedTags) ||
		!ReadStringArray(root, "grantedTags", desc.grantedTags) ||
		!ReadSelfModifiers(root, desc.selfModifiers))
	{
		return false;
	}

	if (root.contains("events"))
	{
		if (!root.at("events").is_object()) return false;
		const json& events = root.at("events");
		desc.activationEventTag = JsonReadUtil::ReadStringOr(events, "activation", {});
		desc.completionEventTag = JsonReadUtil::ReadStringOr(events, "completion", {});
		desc.cancelEventTag = JsonReadUtil::ReadStringOr(events, "cancel", {});
	}

	if (root.contains("vfx"))
	{
		if (!root.at("vfx").is_object()) return false;
		const json& vfx = root.at("vfx");
		desc.vfxCueName = JsonReadUtil::ReadStringOr(vfx, "cueName", {});
		desc.vfxCueAssetPath = JsonReadUtil::ReadStringOr(vfx, "cueAssetPath", {});
		desc.vfxIntensityParameter = JsonReadUtil::ReadStringOr(vfx, "intensityParameter", desc.vfxIntensityParameter);
		desc.vfxIntensity = JsonReadUtil::ReadFloatOr(vfx, "intensity", desc.vfxIntensity);
		desc.stopVfxOnAbilityEnd = JsonReadUtil::ReadBoolOr(vfx, "stopOnAbilityEnd", desc.stopVfxOnAbilityEnd);
	}

	outDesc = std::move(desc);
	return true;
}

bool GameplayAbilitySerializer::Save(const GameplayAbilityDesc& desc, const std::string& filePath)
{
	nlohmann::json root;
	root["schemaVersion"] = GameplayAbilityDesc::kSchemaVersion;
	root["abilityName"] = desc.abilityName;
	root["abilityTag"] = desc.abilityTag;
	root["targetPolicy"] = TargetPolicyToString(desc.targetPolicy);
	root["cooldownSeconds"] = desc.cooldownSeconds;
	root["durationSeconds"] = desc.durationSeconds;
	root["cost"] = {
		{ "attribute", desc.cost.attribute },
		{ "amount", desc.cost.amount }
	};
	root["requiredTags"] = desc.requiredTags;
	root["blockedTags"] = desc.blockedTags;
	root["grantedTags"] = desc.grantedTags;

	root["selfModifiers"] = nlohmann::json::array();
	for (const GameplayAbilitySelfModifierDesc& modifier : desc.selfModifiers)
	{
		root["selfModifiers"].push_back({
			{ "attribute", modifier.modifier.attribute },
			{ "operation", ModifierOperationToString(modifier.modifier.operation) },
			{ "magnitude", modifier.modifier.magnitude },
			{ "durationSeconds", modifier.modifier.durationSeconds },
			{ "stackKey", modifier.modifier.stackKey },
			{ "stackingPolicy", StackingPolicyToString(modifier.modifier.stackingPolicy) },
			{ "removeOnAbilityEnd", modifier.removeOnAbilityEnd }
		});
	}
	root["events"] = {
		{ "activation", desc.activationEventTag },
		{ "completion", desc.completionEventTag },
		{ "cancel", desc.cancelEventTag }
	};
	root["vfx"] = {
		{ "cueName", desc.vfxCueName },
		{ "cueAssetPath", desc.vfxCueAssetPath },
		{ "intensityParameter", desc.vfxIntensityParameter },
		{ "intensity", desc.vfxIntensity },
		{ "stopOnAbilityEnd", desc.stopVfxOnAbilityEnd }
	};
	return JsonFileIO::SaveJsonFile(filePath, root, 4);
}

} // namespace Ken4lowEngine
