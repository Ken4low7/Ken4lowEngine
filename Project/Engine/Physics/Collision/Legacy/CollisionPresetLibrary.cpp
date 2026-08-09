#include "CollisionPresetLibrary.h"

#include "JsonAssetEntry.h"
#include "JsonDataManager.h"

#include <json.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace
{
	using json = nlohmann::json;

	const std::array<EObjectChannel, 13>& PresetObjectChannels()
	{
		static const std::array<EObjectChannel, 13> channels = {
			EObjectChannel::Default,
			EObjectChannel::Player,
			EObjectChannel::Weapon,
			EObjectChannel::Enemy,
			EObjectChannel::PlayerProjectile,
			EObjectChannel::EnemyProjectile,
			EObjectChannel::Item,
			EObjectChannel::Dummy,
			EObjectChannel::Boss,
			EObjectChannel::BossProjectile,
			EObjectChannel::WorldStatic,
			EObjectChannel::TargetLock,
			EObjectChannel::Crystal,
		};
		return channels;
	}

	const char* ToPresetString(EObjectChannel channel)
	{
		switch (channel)
		{
		case EObjectChannel::Default: return "Default";
		case EObjectChannel::Player: return "Player";
		case EObjectChannel::Weapon: return "Weapon";
		case EObjectChannel::Enemy: return "Enemy";
		case EObjectChannel::PlayerProjectile: return "PlayerProjectile";
		case EObjectChannel::EnemyProjectile: return "EnemyProjectile";
		case EObjectChannel::Item: return "Item";
		case EObjectChannel::Dummy: return "Dummy";
		case EObjectChannel::Boss: return "Boss";
		case EObjectChannel::BossProjectile: return "BossProjectile";
		case EObjectChannel::WorldStatic: return "WorldStatic";
		case EObjectChannel::TargetLock: return "TargetLock";
		case EObjectChannel::Crystal: return "Crystal";
		default: return "Default";
		}
	}

	const char* ToPresetString(ECollisionResponse response)
	{
		switch (response)
		{
		case ECollisionResponse::Ignore: return "Ignore";
		case ECollisionResponse::Overlap: return "Overlap";
		case ECollisionResponse::Block: return "Block";
		default: return "Ignore";
		}
	}

	bool ParseObjectChannelString(const std::string& text, EObjectChannel& outChannel)
	{
		for (EObjectChannel channel : PresetObjectChannels())
		{
			if (text == ToPresetString(channel))
			{
				outChannel = channel;
				return true;
			}
		}
		return false;
	}

	bool ParseObjectChannelJson(const json& value, EObjectChannel& outChannel)
	{
		if (value.is_string())
		{
			return ParseObjectChannelString(value.get<std::string>(), outChannel);
		}
		if (value.is_number_unsigned() || value.is_number_integer())
		{
			const int64_t raw = value.get<int64_t>();
			if (raw >= 0 && raw < static_cast<int64_t>(EObjectChannel::Count))
			{
				const EObjectChannel candidate = ToObjectChannel(static_cast<uint32_t>(raw));
				for (EObjectChannel channel : PresetObjectChannels())
				{
					if (candidate == channel)
					{
						outChannel = candidate;
						return true;
					}
				}
			}
		}
		return false;
	}

	bool ParseCollisionResponseJson(const json& value, ECollisionResponse& outResponse)
	{
		if (value.is_string())
		{
			const std::string text = value.get<std::string>();
			if (text == "Ignore") { outResponse = ECollisionResponse::Ignore; return true; }
			if (text == "Overlap") { outResponse = ECollisionResponse::Overlap; return true; }
			if (text == "Block") { outResponse = ECollisionResponse::Block; return true; }
			return false;
		}
		if (value.is_number_unsigned() || value.is_number_integer())
		{
			const int64_t raw = value.get<int64_t>();
			if (raw >= 0 && raw <= static_cast<int64_t>(ECollisionResponse::Block))
			{
				outResponse = static_cast<ECollisionResponse>(raw);
				return true;
			}
		}
		return false;
	}

	CollisionPreset FindDefaultPresetOrFallback(std::string_view name)
	{
		for (const CollisionPreset& preset : GetDefaultCollisionPresets())
		{
			if (preset.name == name)
			{
				return preset;
			}
		}
		return MakeCollisionPreset(name.empty() ? "Default" : name, EObjectChannel::Default, true, true);
	}

	bool ParsePresetJson(const json& presetJson, CollisionPreset& outPreset)
	{
		if (!presetJson.is_object())
		{
			return false;
		}

		const std::string rawName = presetJson.contains("name") && presetJson["name"].is_string()
			? presetJson["name"].get<std::string>()
			: "Default";
		const std::string name = rawName.empty() ? "Default" : rawName;
		CollisionPreset parsed = FindDefaultPresetOrFallback(name);
		parsed.name = name;

		EObjectChannel objectChannel{};
		if (presetJson.contains("objectChannel") && ParseObjectChannelJson(presetJson["objectChannel"], objectChannel))
		{
			parsed.objectChannel = objectChannel;
		}

		if (presetJson.contains("queryEnabled") && presetJson["queryEnabled"].is_boolean())
		{
			parsed.queryEnabled = presetJson["queryEnabled"].get<bool>();
		}
		if (presetJson.contains("physicsEnabled") && presetJson["physicsEnabled"].is_boolean())
		{
			parsed.physicsEnabled = presetJson["physicsEnabled"].get<bool>();
		}

		const json responses = presetJson.value("responses", json::object());
		if (responses.is_object())
		{
			for (auto it = responses.begin(); it != responses.end(); ++it)
			{
				EObjectChannel channel{};
				ECollisionResponse response{};
				if (!ParseObjectChannelString(it.key(), channel)) continue;
				if (!ParseCollisionResponseJson(it.value(), response)) continue;
				SetPresetResponse(parsed, channel, response);
			}
		}

		outPreset = parsed;
		return true;
	}

	json PresetToJson(const CollisionPreset& preset)
	{
		json responses = json::object();
		for (EObjectChannel channel : PresetObjectChannels())
		{
			responses[ToPresetString(channel)] = ToPresetString(preset.GetResponse(channel));
		}

		return {
			{ "name", preset.name },
			{ "objectChannel", ToPresetString(preset.objectChannel) },
			{ "queryEnabled", preset.queryEnabled },
			{ "physicsEnabled", preset.physicsEnabled },
			{ "responses", responses },
		};
	}
}

void CollisionPresetLibrary::ResetToDefaults()
{
	// Jsonが無い/壊れている場合はコード上の既定Presetだけで安全に続行する。
	presets_ = GetDefaultCollisionPresets();
	loadedFromJson_ = false;
	lastStatus_ = "Using built-in CollisionPreset defaults.";
}

bool CollisionPresetLibrary::LoadFromJsonFile(std::string_view filePath)
{
	ResetToDefaults();

	Ken4lowEngine::JsonAssetEntry entry{};
	if (!Ken4lowEngine::JsonDataManager::SafeLoad(std::string(filePath), entry))
	{
		lastStatus_ = "CollisionPreset json not found or failed. Fallback to built-in defaults.";
		return false;
	}

	try
	{
		const json presetsJson = entry.data.value("presets", json::array());
		if (!presetsJson.is_array())
		{
			lastStatus_ = "CollisionPreset json has invalid presets field. Fallback to built-in defaults.";
			return false;
		}

		std::vector<CollisionPreset> parsedPresets;
		for (const json& presetJson : presetsJson)
		{
			CollisionPreset preset{};
			if (ParsePresetJson(presetJson, preset))
			{
				parsedPresets.push_back(preset);
			}
		}

		if (parsedPresets.empty())
		{
			lastStatus_ = "CollisionPreset json contained no valid presets. Fallback to built-in defaults.";
			return false;
		}

		presets_ = std::move(parsedPresets);
		loadedFromJson_ = true;
		lastStatus_ = "CollisionPreset json loaded: " + std::string(filePath);
		return true;
	}
	catch (const std::exception& e)
	{
		ResetToDefaults();
		lastStatus_ = "CollisionPreset json invalid. Fallback to built-in defaults. error=" + std::string(e.what());
		return false;
	}
}

bool CollisionPresetLibrary::SaveToJsonFile(std::string_view filePath) const
{
	json presetsJson = json::array();
	for (const CollisionPreset& preset : presets_)
	{
		presetsJson.push_back(PresetToJson(preset));
	}

	Ken4lowEngine::JsonAssetEntry entry{};
	entry.version = 1;
	entry.id = "collision_presets";
	entry.displayName = "Collision Presets";
	entry.type = "CollisionPresetLibrary";
	entry.path = std::string(filePath);
	entry.data = {
		{ "presets", presetsJson },
	};

	// 保存は明示ボタンからだけ行い、既存Collider設定を自動でJson管理へ移行しない。
	return Ken4lowEngine::JsonDataManager::SafeSave(entry);
}

const CollisionPreset* CollisionPresetLibrary::FindPreset(std::string_view name) const
{
	for (const CollisionPreset& preset : presets_)
	{
		if (preset.name == name)
		{
			return &preset;
		}
	}
	return nullptr;
}

const CollisionPreset* CollisionPresetLibrary::GetPreset(size_t index) const
{
	if (index >= presets_.size())
	{
		return nullptr;
	}
	return &presets_[index];
}
