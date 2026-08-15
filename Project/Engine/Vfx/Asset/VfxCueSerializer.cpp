#include "VfxCueSerializer.h"

#include "JsonFileIO.h"
#include "JsonReadUtil.h"

#include <json.hpp>

namespace Ken4lowEngine
{
using json = nlohmann::json;

namespace
{
	json ToJson(const Vector3& value)
	{
		return { value.x, value.y, value.z };
	}

	template<class T>
	void ReadOptional(const json& source, const char* key, T& value)
	{
		JsonReadUtil::TryRead(source, key, value);
	}

	void ReadVector3(const json& source, const char* key, Vector3& value)
	{
		value = JsonReadUtil::ReadVector3Or(source, key, value);
	}

	bool ReadParticlePayload(const json& source, VfxCueTrackDesc& track)
	{
		if (!source.contains("particle") || !source.at("particle").is_object()) return false;
		const json& payloadJson = source.at("particle");
		auto* payload = std::get_if<VfxParticleTrackPayload>(&track.payload);
		if (payload == nullptr) return false;
		ReadOptional(payloadJson, "effectAssetPath", payload->effectAssetPath);
		ReadOptional(payloadJson, "effectName", payload->effectName);
		ReadOptional(payloadJson, "loop", payload->loop);
		return true;
	}

	bool ReadFluidPayload(const json& source, VfxCueTrackDesc& track)
	{
		if (!source.contains("fluid") || !source.at("fluid").is_object()) return false;
		const json& payloadJson = source.at("fluid");
		auto* payload = std::get_if<VfxFluidTrackPayload>(&track.payload);
		if (payload == nullptr) return false;
		ReadVector3(payloadJson, "localVelocity", payload->localVelocity);
		ReadOptional(payloadJson, "radius", payload->radius);
		ReadOptional(payloadJson, "velocityStrength", payload->velocityStrength);
		ReadOptional(payloadJson, "densityRate", payload->densityRate);
		ReadOptional(payloadJson, "temperatureRate", payload->temperatureRate);
		ReadOptional(payloadJson, "falloffExponent", payload->falloffExponent);
		return true;
	}

	bool ReadLightPayload(const json& source, VfxCueTrackDesc& track)
	{
		if (!source.contains("light") || !source.at("light").is_object()) return false;
		const json& payloadJson = source.at("light");
		auto* payload = std::get_if<VfxLightTrackPayload>(&track.payload);
		if (payload == nullptr) return false;
		ReadVector3(payloadJson, "color", payload->color);
		ReadOptional(payloadJson, "intensity", payload->intensity);
		ReadOptional(payloadJson, "range", payload->range);
		return true;
	}

	bool ReadPostEffectPayload(const json& source, VfxCueTrackDesc& track)
	{
		if (!source.contains("postEffect") || !source.at("postEffect").is_object()) return false;
		const json& payloadJson = source.at("postEffect");
		auto* payload = std::get_if<VfxPostEffectTrackPayload>(&track.payload);
		if (payload == nullptr) return false;
		ReadOptional(payloadJson, "effectName", payload->effectName);
		ReadOptional(payloadJson, "weight", payload->weight);
		return true;
	}

	bool ReadCameraShakePayload(const json& source, VfxCueTrackDesc& track)
	{
		if (!source.contains("cameraShake") || !source.at("cameraShake").is_object()) return false;
		const json& payloadJson = source.at("cameraShake");
		auto* payload = std::get_if<VfxCameraShakeTrackPayload>(&track.payload);
		if (payload == nullptr) return false;
		ReadVector3(payloadJson, "translationAmplitude", payload->translationAmplitude);
		ReadVector3(payloadJson, "rotationAmplitudeDegrees", payload->rotationAmplitudeDegrees);
		ReadOptional(payloadJson, "frequency", payload->frequency);
		ReadOptional(payloadJson, "fovAmplitudeDegrees", payload->fovAmplitudeDegrees);
		return true;
	}

	bool ReadPayload(const json& source, VfxCueTrackDesc& track)
	{
		switch (track.type)
		{
		case VfxCueTrackType::Particle:
			return ReadParticlePayload(source, track);
		case VfxCueTrackType::Fluid2D:
		case VfxCueTrackType::VolumetricFluid:
			return ReadFluidPayload(source, track);
		case VfxCueTrackType::Light:
			return ReadLightPayload(source, track);
		case VfxCueTrackType::PostEffect:
			return ReadPostEffectPayload(source, track);
		case VfxCueTrackType::CameraShake:
			return ReadCameraShakePayload(source, track);
		default:
			return false;
		}
	}

	void WritePayload(json& target, const VfxCueTrackDesc& track)
	{
		switch (track.type)
		{
		case VfxCueTrackType::Particle:
		{
			if (const auto* payload = std::get_if<VfxParticleTrackPayload>(&track.payload))
			{
				target["particle"] = {
					{ "effectAssetPath", payload->effectAssetPath },
					{ "effectName", payload->effectName },
					{ "loop", payload->loop }
				};
			}
			break;
		}
		case VfxCueTrackType::Fluid2D:
		case VfxCueTrackType::VolumetricFluid:
		{
			if (const auto* payload = std::get_if<VfxFluidTrackPayload>(&track.payload))
			{
				target["fluid"] = {
					{ "localVelocity", ToJson(payload->localVelocity) },
					{ "radius", payload->radius },
					{ "velocityStrength", payload->velocityStrength },
					{ "densityRate", payload->densityRate },
					{ "temperatureRate", payload->temperatureRate },
					{ "falloffExponent", payload->falloffExponent }
				};
			}
			break;
		}
		case VfxCueTrackType::Light:
		{
			if (const auto* payload = std::get_if<VfxLightTrackPayload>(&track.payload))
			{
				target["light"] = {
					{ "color", ToJson(payload->color) },
					{ "intensity", payload->intensity },
					{ "range", payload->range }
				};
			}
			break;
		}
		case VfxCueTrackType::PostEffect:
		{
			if (const auto* payload = std::get_if<VfxPostEffectTrackPayload>(&track.payload))
			{
				target["postEffect"] = {
					{ "effectName", payload->effectName },
					{ "weight", payload->weight }
				};
			}
			break;
		}
		case VfxCueTrackType::CameraShake:
		{
			if (const auto* payload = std::get_if<VfxCameraShakeTrackPayload>(&track.payload))
			{
				target["cameraShake"] = {
					{ "translationAmplitude", ToJson(payload->translationAmplitude) },
					{ "rotationAmplitudeDegrees", ToJson(payload->rotationAmplitudeDegrees) },
					{ "frequency", payload->frequency },
					{ "fovAmplitudeDegrees", payload->fovAmplitudeDegrees }
				};
			}
			break;
		}
		default:
			break;
		}
	}
}

std::string ToString(VfxCueTrackType type)
{
	switch (type)
	{
	case VfxCueTrackType::Particle: return "Particle";
	case VfxCueTrackType::Fluid2D: return "Fluid2D";
	case VfxCueTrackType::VolumetricFluid: return "VolumetricFluid";
	case VfxCueTrackType::Light: return "Light";
	case VfxCueTrackType::PostEffect: return "PostEffect";
	case VfxCueTrackType::CameraShake: return "CameraShake";
	default: return "Particle";
	}
}

bool TryParseVfxCueTrackType(const std::string& text, VfxCueTrackType& outType)
{
	if (text == "Particle") outType = VfxCueTrackType::Particle;
	else if (text == "Fluid2D") outType = VfxCueTrackType::Fluid2D;
	else if (text == "VolumetricFluid") outType = VfxCueTrackType::VolumetricFluid;
	else if (text == "Light") outType = VfxCueTrackType::Light;
	else if (text == "PostEffect") outType = VfxCueTrackType::PostEffect;
	else if (text == "CameraShake") outType = VfxCueTrackType::CameraShake;
	else return false;
	return true;
}

bool VfxCueSerializer::Load(VfxCueDesc& desc, const std::string& filePath)
{
	try
	{
		json root;
		if (!JsonFileIO::LoadJsonFile(filePath, root) || !root.is_object()) return false;

		VfxCueDesc loaded{};
		ReadOptional(root, "schemaVersion", loaded.schemaVersion);
		ReadOptional(root, "cueName", loaded.cueName);
		ReadOptional(root, "loop", loaded.loop);
		ReadOptional(root, "duration", loaded.duration);

		if (loaded.schemaVersion != VfxCueDesc::kCurrentSchemaVersion) return false;
		if (!root.contains("tracks") || !root.at("tracks").is_array()) return false;
		if (root.at("tracks").size() > VfxCueDesc::kMaxTracks) return false;

		loaded.tracks.clear();
		loaded.tracks.reserve(root.at("tracks").size());
		for (const json& source : root.at("tracks"))
		{
			if (!source.is_object()) return false;
			std::string typeText;
			ReadOptional(source, "type", typeText);
			VfxCueTrackType type{};
			if (!TryParseVfxCueTrackType(typeText, type)) return false;

			VfxCueTrackDesc track = CreateDefaultVfxCueTrack(type);
			ReadOptional(source, "name", track.name);
			ReadOptional(source, "enabled", track.enabled);
			ReadOptional(source, "startTime", track.startTime);
			ReadOptional(source, "duration", track.duration);
			ReadVector3(source, "localOffset", track.localOffset);
			if (!ReadPayload(source, track)) return false;
			loaded.tracks.push_back(std::move(track));
		}

		// LoadはI/O/schema責務だけを持ち、意味検証と時刻正規化はVfxCueCompilerへ一元化する。
		desc = std::move(loaded);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

bool VfxCueSerializer::Save(const VfxCueDesc& desc, const std::string& filePath)
{
	try
	{
		json root = {
			{ "schemaVersion", desc.schemaVersion },
			{ "cueName", desc.cueName },
			{ "loop", desc.loop },
			{ "duration", desc.duration },
			{ "tracks", json::array() }
		};

		for (const VfxCueTrackDesc& track : desc.tracks)
		{
			json target = {
				{ "name", track.name },
				{ "type", ToString(track.type) },
				{ "enabled", track.enabled },
				{ "startTime", track.startTime },
				{ "duration", track.duration },
				{ "localOffset", ToJson(track.localOffset) }
			};
			WritePayload(target, track);
			root["tracks"].push_back(std::move(target));
		}

		return JsonFileIO::SaveJsonFile(filePath, root, 4);
	}
	catch (const std::exception&)
	{
		return false;
	}
}

std::optional<VfxCueDesc> VfxCueSerializer::LoadFromFile(const std::string& filePath)
{
	VfxCueDesc desc{};
	if (!Load(desc, filePath)) return std::nullopt;
	return desc;
}

} // namespace Ken4lowEngine
