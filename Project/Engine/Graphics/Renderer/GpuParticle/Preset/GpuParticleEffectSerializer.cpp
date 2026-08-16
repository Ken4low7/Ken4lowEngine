#include "GpuParticleEffectSerializer.h"

#include "JsonFileIO.h"
#include "JsonReadUtil.h"

#include <algorithm>

#include <json.hpp>

namespace Ken4lowEngine
{
	using json = nlohmann::json;

	namespace
	{
		json ToJson(const Vector2& value) { return { value.x, value.y }; }
		json ToJson(const Vector3& value) { return { value.x, value.y, value.z }; }
		json ToJson(const Vector4& value) { return { value.x, value.y, value.z, value.w }; }

		template<class T>
		void ReadOptional(const json& source, const char* key, T& value)
		{
			// 欠損・型不正時は既定値を維持し、旧Effect JSONを新しいAuthoring schemaへ段階移行できるようにする。
			JsonReadUtil::TryRead(source, key, value);
		}

		void ReadVector2(const json& source, const char* key, Vector2& value)
		{
			value = JsonReadUtil::ReadVector2Or(source, key, value);
		}

		void ReadVector3(const json& source, const char* key, Vector3& value)
		{
			value = JsonReadUtil::ReadVector3Or(source, key, value);
		}

		void ReadVector4(const json& source, const char* key, Vector4& value)
		{
			value = JsonReadUtil::ReadVector4Or(source, key, value);
		}

		std::string ReadStringOr(const json& source, const char* key, const std::string& fallback)
		{
			return JsonReadUtil::ReadStringOr(source, key, fallback);
		}

		void ReadColorGradientLut(
			const json& source,
			const char* key,
			std::array<Vector4, 4>& values)
		{
			if (!source.contains(key) || !source.at(key).is_array() || source.at(key).size() != values.size())
			{
				return;
			}

			const auto& array = source.at(key);
			for (std::size_t index = 0; index < values.size(); ++index)
			{
				if (!array.at(index).is_array() || array.at(index).size() != 4)
				{
					continue;
				}
				values[index] = {
					array.at(index).at(0).get<float>(),
					array.at(index).at(1).get<float>(),
					array.at(index).at(2).get<float>(),
					array.at(index).at(3).get<float>()
				};
			}
		}

		json ToJson(const std::array<Vector4, 4>& values)
		{
			json result = json::array();
			for (const Vector4& value : values)
			{
				result.push_back(ToJson(value));
			}
			return result;
		}

		const char* ParameterTargetToString(GpuParticleParameterTarget target)
		{
			switch (target)
			{
			case GpuParticleParameterTarget::SpawnRate: return "SpawnRate";
			case GpuParticleParameterTarget::BurstCount: return "BurstCount";
			case GpuParticleParameterTarget::LifeTime: return "LifeTime";
			case GpuParticleParameterTarget::Speed: return "Speed";
			case GpuParticleParameterTarget::Size: return "Size";
			case GpuParticleParameterTarget::Alpha: return "Alpha";
			case GpuParticleParameterTarget::Force: return "Force";
			default: return "Speed";
			}
		}

		GpuParticleParameterTarget ParameterTargetFromString(const std::string& text)
		{
			if (text == "SpawnRate") return GpuParticleParameterTarget::SpawnRate;
			if (text == "BurstCount") return GpuParticleParameterTarget::BurstCount;
			if (text == "LifeTime") return GpuParticleParameterTarget::LifeTime;
			if (text == "Size") return GpuParticleParameterTarget::Size;
			if (text == "Alpha") return GpuParticleParameterTarget::Alpha;
			if (text == "Force") return GpuParticleParameterTarget::Force;
			return GpuParticleParameterTarget::Speed;
		}
	}

	std::string ToString(GpuParticleRenderType type)
	{
		switch (type)
		{
		case GpuParticleRenderType::Mesh: return "Mesh";
		case GpuParticleRenderType::Ribbon: return "Ribbon";
		case GpuParticleRenderType::Trail: return "Trail";
		case GpuParticleRenderType::Sprite:
		default: return "Sprite";
		}
	}

	GpuParticleRenderType GpuParticleRenderTypeFromString(const std::string& text)
	{
		if (text == "Mesh") return GpuParticleRenderType::Mesh;
		if (text == "Ribbon") return GpuParticleRenderType::Ribbon;
		if (text == "Trail") return GpuParticleRenderType::Trail;
		return GpuParticleRenderType::Sprite;
	}

	std::string ToString(GpuParticleBlendMode mode)
	{
		switch (mode)
		{
		case GpuParticleBlendMode::Additive: return "Additive";
		case GpuParticleBlendMode::Multiply: return "Multiply";
		case GpuParticleBlendMode::Alpha:
		default: return "Alpha";
		}
	}

	GpuParticleBlendMode GpuParticleBlendModeFromString(const std::string& text)
	{
		if (text == "Additive") return GpuParticleBlendMode::Additive;
		if (text == "Multiply") return GpuParticleBlendMode::Multiply;
		return GpuParticleBlendMode::Alpha;
	}

	std::string ToString(GpuParticleSpawnShape shape)
	{
		switch (shape)
		{
		case GpuParticleSpawnShape::Sphere: return "Sphere";
		case GpuParticleSpawnShape::Box: return "Box";
		case GpuParticleSpawnShape::Cone: return "Cone";
		case GpuParticleSpawnShape::Circle: return "Circle";
		case GpuParticleSpawnShape::Ring: return "Ring";
		case GpuParticleSpawnShape::Hemisphere: return "Hemisphere";
		case GpuParticleSpawnShape::Point:
		default: return "Point";
		}
	}

	GpuParticleSpawnShape GpuParticleSpawnShapeFromString(const std::string& text)
	{
		if (text == "Sphere") return GpuParticleSpawnShape::Sphere;
		if (text == "Box") return GpuParticleSpawnShape::Box;
		if (text == "Cone") return GpuParticleSpawnShape::Cone;
		if (text == "Circle") return GpuParticleSpawnShape::Circle;
		if (text == "Ring") return GpuParticleSpawnShape::Ring;
		if (text == "Hemisphere") return GpuParticleSpawnShape::Hemisphere;
		return GpuParticleSpawnShape::Point;
	}

	const char* GpuParticleEffectSerializer::ToString(GpuParticleRenderType value)
	{
		switch (value)
		{
		case GpuParticleRenderType::Mesh: return "Mesh";
		case GpuParticleRenderType::Ribbon: return "Ribbon";
		case GpuParticleRenderType::Trail: return "Trail";
		case GpuParticleRenderType::Sprite:
		default: return "Sprite";
		}
	}

	bool GpuParticleEffectSerializer::TryParseRenderType(const std::string& text, GpuParticleRenderType& outValue)
	{
		if (text != "Sprite" && text != "Mesh" && text != "Ribbon" && text != "Trail") return false;
		outValue = GpuParticleRenderTypeFromString(text);
		return true;
	}

	bool GpuParticleEffectSerializer::Load(GpuParticleEffectDesc& desc, const std::string& filePath)
	{
		try
		{
			json root;
			if (!JsonFileIO::LoadJsonFile(filePath, root)) return false;
			if (!root.is_object()) return false;

			GpuParticleEffectDesc effect = CreateDefaultGpuParticleEffectDesc();
			ReadOptional(root, "effectName", effect.effectName);

			if (root.contains("userParameters"))
			{
				if (!root.at("userParameters").is_array()) return false;
				effect.userParameters.clear();
				for (const auto& source : root.at("userParameters"))
				{
					if (!source.is_object()) continue;
					GpuParticleUserParameterDesc parameter{};
					ReadOptional(source, "name", parameter.name);
					ReadOptional(source, "defaultValue", parameter.defaultValue);
					ReadOptional(source, "minValue", parameter.minValue);
					ReadOptional(source, "maxValue", parameter.maxValue);
					if (parameter.minValue > parameter.maxValue)
					{
						std::swap(parameter.minValue, parameter.maxValue);
					}
					parameter.defaultValue = std::clamp(parameter.defaultValue, parameter.minValue, parameter.maxValue);
					effect.userParameters.push_back(std::move(parameter));
				}
			}

			if (root.contains("emitters"))
			{
				if (!root.at("emitters").is_array()) return false;
				effect.emitters.clear();
				for (const auto& source : root.at("emitters"))
				{
					if (!source.is_object()) continue;
					const auto renderType = GpuParticleRenderTypeFromString(ReadStringOr(source, "renderType", "Sprite"));
					GpuParticleEmitterDesc emitter = renderType == GpuParticleRenderType::Mesh
						? CreateDefaultMeshEmitterDesc() : CreateDefaultSpriteEmitterDesc();
					emitter.renderType = renderType;

					ReadOptional(source, "name", emitter.name);
					ReadOptional(source, "texturePath", emitter.texturePath);
					ReadOptional(source, "meshPath", emitter.meshPath);
					ReadOptional(source, "meshSubMeshIndex", emitter.meshSubMeshIndex);
					ReadOptional(source, "maxParticles", emitter.maxParticles);
					ReadOptional(source, "loop", emitter.loop);
					ReadOptional(source, "duration", emitter.duration);
					ReadOptional(source, "spawnRate", emitter.spawnRate);
					ReadOptional(source, "burstCount", emitter.burstCount);
					ReadOptional(source, "lifeTime", emitter.lifeTime);
					ReadOptional(source, "lifeTimeRandom", emitter.lifeTimeRandom);
					ReadVector3(source, "position", emitter.position);
					ReadVector3(source, "positionRandom", emitter.positionRandom);
					emitter.spawnShape = GpuParticleSpawnShapeFromString(ReadStringOr(source, "spawnShape", "Point"));
					ReadOptional(source, "spawnRadius", emitter.spawnRadius);
					ReadVector3(source, "spawnBoxSize", emitter.spawnBoxSize);
					ReadVector3(source, "velocity", emitter.velocity);
					ReadVector3(source, "velocityRandom", emitter.velocityRandom);
					ReadVector3(source, "gravity", emitter.gravity);
					ReadOptional(source, "damping", emitter.damping);
					ReadOptional(source, "speed", emitter.speed);
					ReadOptional(source, "speedRandom", emitter.speedRandom);
					ReadOptional(source, "noiseStrength", emitter.noiseStrength);
					ReadOptional(source, "noiseFrequency", emitter.noiseFrequency);
					ReadVector3(source, "vortexAxis", emitter.vortexAxis);
					ReadOptional(source, "vortexStrength", emitter.vortexStrength);
					ReadVector3(source, "attractorPosition", emitter.attractorPosition);
					ReadOptional(source, "attractorStrength", emitter.attractorStrength);
					ReadOptional(source, "attractorRadius", emitter.attractorRadius);
					ReadVector2(source, "startSize", emitter.startSize);
					ReadVector2(source, "endSize", emitter.endSize);
					ReadOptional(source, "sizeRandom", emitter.sizeRandom);
					ReadOptional(source, "useSizeCurve", emitter.useSizeCurve);
					ReadVector4(source, "sizeCurveLut", emitter.sizeCurveLut);
					ReadVector4(source, "startColor", emitter.startColor);
					ReadVector4(source, "endColor", emitter.endColor);
					ReadVector4(source, "colorRandom", emitter.colorRandom);
					ReadOptional(source, "alphaFade", emitter.alphaFade);
					ReadOptional(source, "useColorGradient", emitter.useColorGradient);
					ReadColorGradientLut(source, "colorGradientLut", emitter.colorGradientLut);
					ReadOptional(source, "startRotation", emitter.startRotation);
					ReadOptional(source, "rotationSpeed", emitter.rotationSpeed);
					ReadOptional(source, "rotationRandom", emitter.rotationRandom);
					ReadOptional(source, "billboard", emitter.billboard);
					emitter.blendMode = GpuParticleBlendModeFromString(ReadStringOr(source, "blendMode", "Additive"));
					ReadOptional(source, "useSpriteSheet", emitter.useSpriteSheet);
					ReadOptional(source, "spriteSheetRows", emitter.spriteSheetRows);
					ReadOptional(source, "spriteSheetColumns", emitter.spriteSheetColumns);
					ReadOptional(source, "spriteSheetFrameRate", emitter.spriteSheetFrameRate);
					ReadVector3(source, "startScale3D", emitter.startScale3D);
					ReadVector3(source, "endScale3D", emitter.endScale3D);
					ReadVector3(source, "startRotation3D", emitter.startRotation3D);
					ReadVector3(source, "rotationRandom3D", emitter.rotationRandom3D);
					ReadVector3(source, "angularVelocity", emitter.angularVelocity);
					ReadVector3(source, "angularVelocityRandom", emitter.angularVelocityRandom);

					if (source.contains("parameterBindings") && source.at("parameterBindings").is_array())
					{
						emitter.parameterBindings.clear();
						for (const auto& bindingSource : source.at("parameterBindings"))
						{
							if (!bindingSource.is_object()) continue;
							GpuParticleParameterBindingDesc binding{};
							ReadOptional(bindingSource, "parameterName", binding.parameterName);
							binding.target = ParameterTargetFromString(ReadStringOr(bindingSource, "target", "Speed"));
							ReadOptional(bindingSource, "scale", binding.scale);
							ReadOptional(bindingSource, "bias", binding.bias);
							if (!binding.parameterName.empty()) emitter.parameterBindings.push_back(std::move(binding));
						}
					}

					emitter.spriteSheetRows = (std::max)(emitter.spriteSheetRows, 1);
					emitter.spriteSheetColumns = (std::max)(emitter.spriteSheetColumns, 1);
					emitter.noiseFrequency = (std::max)(emitter.noiseFrequency, 0.0f);
					emitter.attractorRadius = (std::max)(emitter.attractorRadius, 0.0f);
					effect.emitters.push_back(std::move(emitter));
				}
			}

			desc = std::move(effect);
			return true;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	bool GpuParticleEffectSerializer::Save(const GpuParticleEffectDesc& effect, const std::string& filePath)
	{
		try
		{
			json root;
			root["effectName"] = effect.effectName;
			root["userParameters"] = json::array();
			for (const auto& parameter : effect.userParameters)
			{
				root["userParameters"].push_back({
					{ "name", parameter.name },
					{ "defaultValue", parameter.defaultValue },
					{ "minValue", parameter.minValue },
					{ "maxValue", parameter.maxValue }
				});
			}

			root["emitters"] = json::array();
			for (const auto& e : effect.emitters)
			{
				json emitter = {
					{ "name", e.name }, { "renderType", Ken4lowEngine::ToString(e.renderType) },
					{ "texturePath", e.texturePath }, { "meshPath", e.meshPath }, { "meshSubMeshIndex", e.meshSubMeshIndex },
					{ "maxParticles", e.maxParticles }, { "loop", e.loop }, { "duration", e.duration },
					{ "spawnRate", e.spawnRate }, { "burstCount", e.burstCount },
					{ "lifeTime", e.lifeTime }, { "lifeTimeRandom", e.lifeTimeRandom },
					{ "position", ToJson(e.position) }, { "positionRandom", ToJson(e.positionRandom) },
					{ "spawnShape", Ken4lowEngine::ToString(e.spawnShape) }, { "spawnRadius", e.spawnRadius },
					{ "spawnBoxSize", ToJson(e.spawnBoxSize) },
					{ "velocity", ToJson(e.velocity) }, { "velocityRandom", ToJson(e.velocityRandom) },
					{ "gravity", ToJson(e.gravity) }, { "damping", e.damping },
					{ "speed", e.speed }, { "speedRandom", e.speedRandom },
					{ "noiseStrength", e.noiseStrength }, { "noiseFrequency", e.noiseFrequency },
					{ "vortexAxis", ToJson(e.vortexAxis) }, { "vortexStrength", e.vortexStrength },
					{ "attractorPosition", ToJson(e.attractorPosition) },
					{ "attractorStrength", e.attractorStrength }, { "attractorRadius", e.attractorRadius },
					{ "startSize", ToJson(e.startSize) }, { "endSize", ToJson(e.endSize) }, { "sizeRandom", e.sizeRandom },
					{ "useSizeCurve", e.useSizeCurve }, { "sizeCurveLut", ToJson(e.sizeCurveLut) },
					{ "startColor", ToJson(e.startColor) }, { "endColor", ToJson(e.endColor) },
					{ "colorRandom", ToJson(e.colorRandom) }, { "alphaFade", e.alphaFade },
					{ "useColorGradient", e.useColorGradient }, { "colorGradientLut", ToJson(e.colorGradientLut) },
					{ "startRotation", e.startRotation }, { "rotationSpeed", e.rotationSpeed }, { "rotationRandom", e.rotationRandom },
					{ "billboard", e.billboard }, { "blendMode", Ken4lowEngine::ToString(e.blendMode) },
					{ "useSpriteSheet", e.useSpriteSheet }, { "spriteSheetRows", e.spriteSheetRows },
					{ "spriteSheetColumns", e.spriteSheetColumns }, { "spriteSheetFrameRate", e.spriteSheetFrameRate },
					{ "startScale3D", ToJson(e.startScale3D) }, { "endScale3D", ToJson(e.endScale3D) },
					{ "startRotation3D", ToJson(e.startRotation3D) }, { "rotationRandom3D", ToJson(e.rotationRandom3D) },
					{ "angularVelocity", ToJson(e.angularVelocity) }, { "angularVelocityRandom", ToJson(e.angularVelocityRandom) }
				};

				emitter["parameterBindings"] = json::array();
				for (const auto& binding : e.parameterBindings)
				{
					emitter["parameterBindings"].push_back({
						{ "parameterName", binding.parameterName },
						{ "target", ParameterTargetToString(binding.target) },
						{ "scale", binding.scale },
						{ "bias", binding.bias }
					});
				}
				root["emitters"].push_back(std::move(emitter));
			}

			return JsonFileIO::SaveJsonFile(filePath, root, 4);
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	std::optional<GpuParticleEffectDesc> GpuParticleEffectSerializer::LoadFromFile(const std::string& filePath)
	{
		GpuParticleEffectDesc desc{};
		if (!Load(desc, filePath)) return std::nullopt;
		return desc;
	}

	bool GpuParticleEffectSerializer::SaveToFile(const GpuParticleEffectDesc& effect, const std::string& filePath)
	{
		return Save(effect, filePath);
	}
} // namespace Ken4lowEngine
