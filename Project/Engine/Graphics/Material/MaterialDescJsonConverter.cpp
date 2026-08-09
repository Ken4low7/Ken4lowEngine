#include "MaterialDescJsonConverter.h"

#include "JsonReadUtil.h"

namespace Ken4lowEngine
{
	namespace
	{
		nlohmann::json AsObjectOrEmpty(const nlohmann::json& value)
		{
			return value.is_object() ? value : nlohmann::json::object();
		}

		nlohmann::json ToJsonVector4(const Vector4& value)
		{
			return nlohmann::json::array({ value.x, value.y, value.z, value.w });
		}
	}

	MaterialDescSource MaterialDescJsonConverter::FromJson(const nlohmann::json& json)
	{
		const nlohmann::json root = AsObjectOrEmpty(json);
		const nlohmann::json legacy = AsObjectOrEmpty(JsonReadUtil::ReadObjectOr(root, Keys::Legacy, nlohmann::json::object()));
		const nlohmann::json pbr = AsObjectOrEmpty(JsonReadUtil::ReadObjectOr(root, Keys::Pbr, nlohmann::json::object()));

		MaterialDescSource source{};
		source.sourceKind = SourceKindFromString(JsonReadUtil::ReadStringOr(root, Keys::SourceKind, "json"));
		source.materialId = JsonReadUtil::ReadStringOr(root, Keys::MaterialId, source.materialId);
		source.materialName = JsonReadUtil::ReadStringOr(root, Keys::MaterialName, source.materialName);
		source.sourcePath = JsonReadUtil::ReadStringOr(root, Keys::SourcePath, source.sourcePath);
		source.preferPbrWorkflow = JsonReadUtil::ReadBoolOr(root, Keys::PreferPbrWorkflow, source.preferPbrWorkflow);

		// 欠損キーはSource既定値へフォールバックする。JsonReadUtilへ寄せてもMaterialDescのキー規約と既存Json互換は変えない。
		source.legacyColor = JsonReadUtil::ReadVector4Or(legacy, Keys::LegacyColor, source.legacyColor);
		source.legacyShininess = JsonReadUtil::ReadFloatOr(legacy, Keys::LegacyShininess, source.legacyShininess);
		source.legacyReflectionRate = JsonReadUtil::ReadFloatOr(legacy, Keys::LegacyReflectionRate, source.legacyReflectionRate);
		source.legacyRoughness = JsonReadUtil::ReadFloatOr(legacy, Keys::LegacyRoughness, source.legacyRoughness);
		source.usePointSampling = JsonReadUtil::ReadBoolOr(legacy, Keys::LegacyUsePointSampling, source.usePointSampling);

		source.baseColorFactor = JsonReadUtil::ReadVector4Or(pbr, Keys::PbrBaseColor, source.baseColorFactor);
		source.metallicFactor = JsonReadUtil::ReadFloatOr(pbr, Keys::PbrMetallic, source.metallicFactor);
		source.roughnessFactor = JsonReadUtil::ReadFloatOr(pbr, Keys::PbrRoughness, source.roughnessFactor);
		source.normalScale = JsonReadUtil::ReadFloatOr(pbr, Keys::PbrNormalScale, source.normalScale);
		source.occlusionStrength = JsonReadUtil::ReadFloatOr(pbr, Keys::PbrOcclusionStrength, source.occlusionStrength);
		source.emissiveFactor = JsonReadUtil::ReadVector4Or(pbr, Keys::PbrEmissiveColor, source.emissiveFactor);
		const float emissiveStrength = JsonReadUtil::ReadFloatOr(pbr, Keys::PbrEmissiveStrength, 1.0f);
		source.emissiveFactor.x *= emissiveStrength;
		source.emissiveFactor.y *= emissiveStrength;
		source.emissiveFactor.z *= emissiveStrength;

		// Legacy/PBRは同じbaseColorTextureを使うことがあるため、PBR側が空ならLegacy側の指定をSourceへ残す。
		source.baseColorTexturePath = JsonReadUtil::ReadStringOr(pbr, Keys::PbrBaseColorTexture, JsonReadUtil::ReadStringOr(legacy, Keys::LegacyBaseColorTexture, source.baseColorTexturePath));
		source.normalTexturePath = JsonReadUtil::ReadStringOr(pbr, Keys::PbrNormalTexture, source.normalTexturePath);
		source.metallicRoughnessTexturePath = JsonReadUtil::ReadStringOr(pbr, Keys::PbrMetallicRoughnessTexture, source.metallicRoughnessTexturePath);
		source.occlusionTexturePath = JsonReadUtil::ReadStringOr(pbr, Keys::PbrOcclusionTexture, source.occlusionTexturePath);
		source.emissiveTexturePath = JsonReadUtil::ReadStringOr(pbr, Keys::PbrEmissiveTexture, source.emissiveTexturePath);

		const auto slotsIt = root.find(Keys::TextureSlots);
		if (slotsIt != root.end() && slotsIt->is_array())
		{
			for (const auto& slotJson : *slotsIt)
			{
				if (!slotJson.is_object())
				{
					continue;
				}

				MaterialSourceTextureSlot slot{};
				slot.semantic = JsonReadUtil::ReadStringOr(slotJson, Keys::TextureSlotSemantic, "");
				slot.texturePath = JsonReadUtil::ReadStringOr(slotJson, Keys::TextureSlotPath, "");
				if (!slot.semantic.empty() || !slot.texturePath.empty())
				{
					// TextureManagerへは接続せず、Json上のslot情報をSourceへ移すだけにする。
					source.textureSlots.push_back(slot);
				}
			}
		}

		return MaterialDescLoader::NormalizeSource(source);
	}

	nlohmann::json MaterialDescJsonConverter::ToJson(const MaterialDescSource& source)
	{
		const MaterialDescSource normalized = MaterialDescLoader::NormalizeSource(source);

		nlohmann::json json = nlohmann::json::object();
		json[Keys::MaterialId] = normalized.materialId;
		json[Keys::MaterialName] = normalized.materialName;
		json[Keys::SourcePath] = normalized.sourcePath;
		json[Keys::SourceKind] = ToString(normalized.sourceKind);
		json[Keys::PreferPbrWorkflow] = normalized.preferPbrWorkflow;

		json[Keys::Legacy] = {
			{ Keys::LegacyColor, ToJsonVector4(normalized.legacyColor) },
			{ Keys::LegacyShininess, normalized.legacyShininess },
			{ Keys::LegacyReflectionRate, normalized.legacyReflectionRate },
			{ Keys::LegacyRoughness, normalized.legacyRoughness },
			{ Keys::LegacyBaseColorTexture, normalized.baseColorTexturePath },
			{ Keys::LegacyUsePointSampling, normalized.usePointSampling },
		};

		json[Keys::Pbr] = {
			{ Keys::PbrBaseColor, ToJsonVector4(normalized.baseColorFactor) },
			{ Keys::PbrMetallic, normalized.metallicFactor },
			{ Keys::PbrRoughness, normalized.roughnessFactor },
			{ Keys::PbrNormalScale, normalized.normalScale },
			{ Keys::PbrOcclusionStrength, normalized.occlusionStrength },
			{ Keys::PbrEmissiveColor, ToJsonVector4(normalized.emissiveFactor) },
			{ Keys::PbrEmissiveStrength, 1.0f },
			{ Keys::PbrBaseColorTexture, normalized.baseColorTexturePath },
			{ Keys::PbrNormalTexture, normalized.normalTexturePath },
			{ Keys::PbrMetallicRoughnessTexture, normalized.metallicRoughnessTexturePath },
			{ Keys::PbrOcclusionTexture, normalized.occlusionTexturePath },
			{ Keys::PbrEmissiveTexture, normalized.emissiveTexturePath },
		};

		json[Keys::TextureSlots] = nlohmann::json::array();
		for (const auto& slot : normalized.textureSlots)
		{
			if (slot.semantic.empty() && slot.texturePath.empty())
			{
				continue;
			}
			json[Keys::TextureSlots].push_back({
				{ Keys::TextureSlotSemantic, slot.semantic },
				{ Keys::TextureSlotPath, slot.texturePath },
			});
		}

		// ここではJsonオブジェクトを返すだけで、ファイル保存やMaterialRepository登録は呼び出し側の明示的な責務にする。
		return json;
	}

	const char* MaterialDescJsonConverter::ToString(MaterialSourceKind kind)
	{
		switch (kind)
		{
		case MaterialSourceKind::Json:
			return "json";
		case MaterialSourceKind::Gltf:
			return "gltf";
		case MaterialSourceKind::MaterialEditor:
			return "materialEditor";
		case MaterialSourceKind::Manual:
		default:
			return "manual";
		}
	}

	MaterialSourceKind MaterialDescJsonConverter::SourceKindFromString(const std::string& text)
	{
		if (text == "json")
		{
			return MaterialSourceKind::Json;
		}
		if (text == "gltf")
		{
			return MaterialSourceKind::Gltf;
		}
		if (text == "materialEditor")
		{
			return MaterialSourceKind::MaterialEditor;
		}
		return MaterialSourceKind::Manual;
	}
}
