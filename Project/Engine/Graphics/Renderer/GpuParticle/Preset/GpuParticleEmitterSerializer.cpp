#include "GpuParticleEmitterSerializer.h"

#include <fstream>
#include <filesystem>
#include <algorithm>

#include <json.hpp>

namespace Ken4lowEngine
{
	using json = nlohmann::json;
	namespace fs = std::filesystem;

	namespace
	{
		template<typename T>
		bool ReadEnumString(const json& j, const char* key, T& outValue, bool(*parser)(const std::string&, T&))
		{
			if (!j.contains(key) || !j.at(key).is_string())
			{
				return false;
			}
			return parser(j.at(key).get<std::string>(), outValue);
		}
	}

	const char* GpuParticleEmitterSerializer::ToString(GpuParticleKind value)
	{
		switch (value)
		{
		case GpuParticleKind::Sprite: return "Sprite";
		case GpuParticleKind::Mesh:   return "Mesh";
		case GpuParticleKind::Ribbon: return "Ribbon";
		case GpuParticleKind::Beam:   return "Beam";
		default:                      return "Sprite";
		}
	}

	const char* GpuParticleEmitterSerializer::ToString(GpuParticleType value)
	{
		switch (value)
		{
		case GpuParticleType::Default:   return "Default";
		case GpuParticleType::Blood:     return "Blood";
		case GpuParticleType::Dust:      return "Dust";
		case GpuParticleType::Debris:    return "Debris";
		case GpuParticleType::Smoke:     return "Smoke";
		case GpuParticleType::Ambient:   return "Ambient";
		case GpuParticleType::Spark:     return "Spark";
		case GpuParticleType::Shockwave: return "Shockwave";
		case GpuParticleType::Heal:      return "Heal";
		case GpuParticleType::Trail:     return "Trail";
		case GpuParticleType::DeathBurstCore: return "DeathBurstCore";
		case GpuParticleType::Count:     return "Count";
		default:                         return "Default";
		}
	}

	const char* GpuParticleEmitterSerializer::ToString(GpuRibbonType value)
	{
		switch (value)
		{
		case GpuRibbonType::Trail: return "Trail";
		default:                   return "Trail";
		}
	}

	const char* GpuParticleEmitterSerializer::ToString(BillboardMode value)
	{
		switch (value)
		{
		case BillboardMode::None:   return "None";
		case BillboardMode::Camera: return "Camera";
		case BillboardMode::YAxis:  return "YAxis";
		case BillboardMode::Ribbon: return "Ribbon";
		default:                    return "None";
		}
	}

	bool GpuParticleEmitterSerializer::TryParseGpuParticleKind(const std::string& text, GpuParticleKind& outValue)
	{
		if (text == "Sprite") { outValue = GpuParticleKind::Sprite; return true; }
		if (text == "Mesh") { outValue = GpuParticleKind::Mesh;   return true; }
		if (text == "Ribbon") { outValue = GpuParticleKind::Ribbon; return true; }
		if (text == "Beam") { outValue = GpuParticleKind::Beam;   return true; }
		return false;
	}

	bool GpuParticleEmitterSerializer::TryParseGpuParticleType(const std::string& text, GpuParticleType& outValue)
	{
#define GPU_PARTICLE_PARSE(name) if (text == #name) { outValue = GpuParticleType::name; return true; }

		GPU_PARTICLE_PARSE(Default)
			GPU_PARTICLE_PARSE(Blood)
			GPU_PARTICLE_PARSE(Dust)
			GPU_PARTICLE_PARSE(Debris)
			GPU_PARTICLE_PARSE(Smoke)
			GPU_PARTICLE_PARSE(Ambient)
			GPU_PARTICLE_PARSE(Spark)
			GPU_PARTICLE_PARSE(Shockwave)
			GPU_PARTICLE_PARSE(Heal)
			GPU_PARTICLE_PARSE(Trail)
			GPU_PARTICLE_PARSE(DeathBurstCore)

#undef GPU_PARTICLE_PARSE
			return false;
	}

	bool GpuParticleEmitterSerializer::TryParseGpuRibbonType(const std::string& text, GpuRibbonType& outValue)
	{
		if (text == "Trail") { outValue = GpuRibbonType::Trail; return true; }
		return false;
	}

	bool GpuParticleEmitterSerializer::TryParseBillboardMode(const std::string& text, BillboardMode& outValue)
	{
		if (text == "None") { outValue = BillboardMode::None;   return true; }
		if (text == "Camera") { outValue = BillboardMode::Camera; return true; }
		if (text == "YAxis") { outValue = BillboardMode::YAxis;  return true; }
		if (text == "Ribbon") { outValue = BillboardMode::Ribbon; return true; }
		return false;
	}

	std::optional<GpuParticleEmitterAsset> GpuParticleEmitterSerializer::LoadFromFile(const std::string& filePath)
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open())
		{
			return std::nullopt;
		}

		json j;
		ifs >> j;

		GpuParticleEmitterAsset asset{};

		if (j.contains("name") && j["name"].is_string())
		{
			asset.name = j["name"].get<std::string>();
		}

		if (j.contains("textureFilePath") && j["textureFilePath"].is_string())
		{
			asset.textureFilePath = j["textureFilePath"].get<std::string>();
		}

		if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3)
		{
			asset.position.x = j["position"][0].get<float>();
			asset.position.y = j["position"][1].get<float>();
			asset.position.z = j["position"][2].get<float>();
		}

		if (j.contains("radius"))        asset.radius = j["radius"].get<float>();
		if (j.contains("loopCount"))     asset.loopCount = j["loopCount"].get<uint32_t>();
		if (j.contains("loopFrequency")) asset.loopFrequency = j["loopFrequency"].get<float>();
		if (j.contains("drawType"))      asset.drawType = j["drawType"].get<uint32_t>();

		ReadEnumString(j, "kind", asset.kind, &TryParseGpuParticleKind);
		ReadEnumString(j, "spriteType", asset.spriteType, &TryParseGpuParticleType);
		ReadEnumString(j, "ribbonType", asset.ribbonType, &TryParseGpuRibbonType);
		ReadEnumString(j, "billboardFlags", asset.billboardFlags, &TryParseBillboardMode);

		if (asset.name.empty())
		{
			asset.name = fs::path(filePath).stem().string();
		}

		return asset;
	}

	bool GpuParticleEmitterSerializer::SaveToFile(const GpuParticleEmitterAsset& asset, const std::string& filePath)
	{
		const fs::path path(filePath);
		if (path.has_parent_path())
		{
			fs::create_directories(path.parent_path());
		}

		json j;
		j["name"] = asset.name;
		j["textureFilePath"] = asset.textureFilePath;
		j["position"] = { asset.position.x, asset.position.y, asset.position.z };
		j["radius"] = asset.radius;
		j["loopCount"] = asset.loopCount;
		j["loopFrequency"] = asset.loopFrequency;
		j["drawType"] = asset.drawType;
		j["kind"] = ToString(asset.kind);
		j["spriteType"] = ToString(asset.spriteType);
		j["ribbonType"] = ToString(asset.ribbonType);
		j["billboardFlags"] = ToString(asset.billboardFlags);

		std::ofstream ofs(filePath);
		if (!ofs.is_open())
		{
			return false;
		}

		ofs << j.dump(4);
		return true;
	}

	std::vector<std::string> GpuParticleEmitterSerializer::FindJsonFiles(const std::string& directoryPath)
	{
		std::vector<std::string> files;

		const fs::path dir(directoryPath);
		if (!fs::exists(dir) || !fs::is_directory(dir))
		{
			return files;
		}

		for (const auto& entry : fs::directory_iterator(dir))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			const fs::path path = entry.path();
			if (path.extension() == ".json")
			{
				files.push_back(path.string());
			}
		}

		std::sort(files.begin(), files.end());
		return files;
	}

} // namespace Ken4lowEngine