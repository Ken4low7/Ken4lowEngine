#include "MaterialDescLoader.h"

#include <algorithm>
#include <string>

namespace Ken4lowEngine
{
	MaterialDesc MaterialDescLoader::CreateDefaultDesc()
	{
		return CreateLegacyDesc();
	}

	MaterialDesc MaterialDescLoader::CreateLegacyDesc(
		Vector4 color,
		float shininess,
		float reflection,
		float roughness,
		const std::string& baseColorTexturePath)
	{
		MaterialDesc desc{};
		desc.preferPbrWorkflow = false;
		desc.legacy.color = color;
		desc.legacy.shininess = shininess;
		desc.legacy.reflection = reflection;
		desc.legacy.roughness = roughness;
		desc.legacy.uvTransform = Matrix4x4::MakeIdentity();
		desc.legacy.usePointSampling = false;
		desc.legacy.baseColorTexturePath = baseColorTexturePath;

		// Legacy入力もNormalizeを通し、既存MaterialCBDataへ将来接続する前に危険な範囲外値をCPU側で抑える。
		return NormalizeDesc(desc);
	}

	MaterialDesc MaterialDescLoader::CreatePbrDesc(
		Vector4 baseColorFactor,
		float metallicFactor,
		float roughnessFactor,
		const std::string& baseColorTexturePath,
		const std::string& metallicRoughnessTexturePath,
		const std::string& normalTexturePath,
		const std::string& occlusionTexturePath,
		const std::string& emissiveTexturePath)
	{
		MaterialDesc desc{};
		desc.preferPbrWorkflow = true;
		desc.pbr.baseColorFactor = baseColorFactor;
		desc.pbr.metallicFactor = metallicFactor;
		desc.pbr.roughnessFactor = roughnessFactor;
		desc.pbr.normalScale = 1.0f;
		desc.pbr.occlusionStrength = 1.0f;
		desc.pbr.emissiveFactor = { 0.0f, 0.0f, 0.0f, 1.0f };
		desc.pbr.baseColorTexturePath = baseColorTexturePath;
		desc.pbr.metallicRoughnessTexturePath = metallicRoughnessTexturePath;
		desc.pbr.normalTexturePath = normalTexturePath;
		desc.pbr.occlusionTexturePath = occlusionTexturePath;
		desc.pbr.emissiveTexturePath = emissiveTexturePath;

		// PBR入力はglTF/Jsonから来る値を想定し、Shaderへ渡す前段階のCPU側正規化だけを行う。
		return NormalizeDesc(desc);
	}

	MaterialDesc MaterialDescLoader::CreateFromSource(const MaterialDescSource& source)
	{
		const MaterialDescSource normalizedSource = NormalizeSource(source);

		MaterialDesc desc{};
		desc.preferPbrWorkflow = normalizedSource.preferPbrWorkflow;

		desc.legacy.color = normalizedSource.legacyColor;
		desc.legacy.shininess = normalizedSource.legacyShininess;
		desc.legacy.reflection = normalizedSource.legacyReflectionRate;
		desc.legacy.roughness = normalizedSource.legacyRoughness;
		desc.legacy.uvTransform = Matrix4x4::MakeIdentity();
		desc.legacy.usePointSampling = normalizedSource.usePointSampling;
		desc.legacy.baseColorTexturePath = SelectTexturePath(normalizedSource.baseColorTexturePath, normalizedSource, "baseColor", "albedo");

		desc.pbr.baseColorFactor = normalizedSource.baseColorFactor;
		desc.pbr.metallicFactor = normalizedSource.metallicFactor;
		desc.pbr.roughnessFactor = normalizedSource.roughnessFactor;
		desc.pbr.normalScale = normalizedSource.normalScale;
		desc.pbr.occlusionStrength = normalizedSource.occlusionStrength;
		desc.pbr.emissiveFactor = normalizedSource.emissiveFactor;
		desc.pbr.baseColorTexturePath = SelectTexturePath(normalizedSource.baseColorTexturePath, normalizedSource, "baseColor", "albedo");
		desc.pbr.metallicRoughnessTexturePath = SelectTexturePath(normalizedSource.metallicRoughnessTexturePath, normalizedSource, "metallicRoughness", "metallic_roughness");
		desc.pbr.normalTexturePath = SelectTexturePath(normalizedSource.normalTexturePath, normalizedSource, "normal", "normalMap");
		desc.pbr.occlusionTexturePath = SelectTexturePath(normalizedSource.occlusionTexturePath, normalizedSource, "occlusion", "ao");
		desc.pbr.emissiveTexturePath = SelectTexturePath(normalizedSource.emissiveTexturePath, normalizedSource, "emissive", "emission");

		// Sourceは入力元メタ情報を含むが、MaterialDescは描画互換用の値だけに絞る。Repository登録やTexture slot管理は後段へ分ける。
		return NormalizeDesc(desc);
	}

	MaterialDesc MaterialDescLoader::NormalizeDesc(const MaterialDesc& desc)
	{
		MaterialDesc normalized = desc;

		// 色や係数を一般的な範囲へ丸め、MaterialEditorやJson手入力による極端な値で将来の描画接続が破綻しないようにする。
		normalized.legacy.color = ClampColor(normalized.legacy.color);
		normalized.legacy.shininess = ClampNonNegative(normalized.legacy.shininess);
		normalized.legacy.reflection = Clamp01(normalized.legacy.reflection);
		normalized.legacy.roughness = Clamp01(normalized.legacy.roughness);

		normalized.pbr.baseColorFactor = ClampColor(normalized.pbr.baseColorFactor);
		normalized.pbr.metallicFactor = Clamp01(normalized.pbr.metallicFactor);
		normalized.pbr.roughnessFactor = Clamp01(normalized.pbr.roughnessFactor);
		normalized.pbr.normalScale = ClampNonNegative(normalized.pbr.normalScale);
		normalized.pbr.occlusionStrength = Clamp01(normalized.pbr.occlusionStrength);
		normalized.pbr.emissiveFactor = ClampColor(normalized.pbr.emissiveFactor);

		return normalized;
	}

	MaterialDescSource MaterialDescLoader::NormalizeSource(const MaterialDescSource& source)
	{
		MaterialDescSource normalized = source;

		// Json/glTF/手動入力は範囲外値を含み得るため、MaterialDescへ変換する前のSource段階で安全な範囲へ丸める。
		normalized.legacyColor = ClampColor(normalized.legacyColor);
		normalized.legacyShininess = ClampNonNegative(normalized.legacyShininess);
		normalized.legacyReflectionRate = Clamp01(normalized.legacyReflectionRate);
		normalized.legacyRoughness = Clamp01(normalized.legacyRoughness);

		normalized.baseColorFactor = ClampColor(normalized.baseColorFactor);
		normalized.metallicFactor = Clamp01(normalized.metallicFactor);
		normalized.roughnessFactor = Clamp01(normalized.roughnessFactor);
		normalized.normalScale = ClampNonNegative(normalized.normalScale);
		normalized.occlusionStrength = Clamp01(normalized.occlusionStrength);
		normalized.emissiveFactor = ClampColor(normalized.emissiveFactor);

		return normalized;
	}

	bool MaterialDescLoader::ValidateDesc(const MaterialDesc& desc, std::string* outMessage)
	{
		const MaterialDesc normalized = NormalizeDesc(desc);
		if (normalized.legacy.shininess != desc.legacy.shininess)
		{
			if (outMessage)
			{
				*outMessage = "legacy.shininess must be non-negative.";
			}
			return false;
		}
		if (normalized.legacy.reflection != desc.legacy.reflection || normalized.legacy.roughness != desc.legacy.roughness)
		{
			if (outMessage)
			{
				*outMessage = "legacy reflection/roughness must be in 0..1.";
			}
			return false;
		}
		if (normalized.pbr.metallicFactor != desc.pbr.metallicFactor || normalized.pbr.roughnessFactor != desc.pbr.roughnessFactor)
		{
			if (outMessage)
			{
				*outMessage = "pbr metallic/roughness must be in 0..1.";
			}
			return false;
		}
		if (normalized.pbr.normalScale != desc.pbr.normalScale || normalized.pbr.occlusionStrength != desc.pbr.occlusionStrength)
		{
			if (outMessage)
			{
				*outMessage = "pbr normalScale must be non-negative and occlusionStrength must be in 0..1.";
			}
			return false;
		}

		if (outMessage)
		{
			*outMessage = "MaterialDesc is valid.";
		}
		return true;
	}

	bool MaterialDescLoader::ValidateSource(const MaterialDescSource& source, std::string* outMessage)
	{
		const MaterialDescSource normalized = NormalizeSource(source);
		if (normalized.legacyShininess != source.legacyShininess)
		{
			if (outMessage)
			{
				*outMessage = "source legacyShininess must be non-negative.";
			}
			return false;
		}
		if (normalized.legacyReflectionRate != source.legacyReflectionRate || normalized.legacyRoughness != source.legacyRoughness)
		{
			if (outMessage)
			{
				*outMessage = "source legacy reflection/roughness must be in 0..1.";
			}
			return false;
		}
		if (normalized.metallicFactor != source.metallicFactor || normalized.roughnessFactor != source.roughnessFactor)
		{
			if (outMessage)
			{
				*outMessage = "source pbr metallic/roughness must be in 0..1.";
			}
			return false;
		}
		if (normalized.normalScale != source.normalScale || normalized.occlusionStrength != source.occlusionStrength)
		{
			if (outMessage)
			{
				*outMessage = "source normalScale must be non-negative and occlusionStrength must be in 0..1.";
			}
			return false;
		}

		for (const auto& slot : source.textureSlots)
		{
			if (slot.semantic.empty() && !slot.texturePath.empty())
			{
				if (outMessage)
				{
					*outMessage = "source texture slot semantic is required when texturePath is set.";
				}
				return false;
			}
		}

		if (outMessage)
		{
			*outMessage = "MaterialDescSource is valid.";
		}
		return true;
	}

	MaterialAsset MaterialDescLoader::CreateAsset(const std::string& id, const std::string& name, const MaterialDesc& desc)
	{
		// Repository登録前に正規化済みDescへ変換し、登録後もGPU/HLSLへはまだ接続しない。
		return MaterialAsset(id, name, NormalizeDesc(desc));
	}

	float MaterialDescLoader::Clamp01(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	float MaterialDescLoader::ClampNonNegative(float value)
	{
		return value < 0.0f ? 0.0f : value;
	}

	Vector4 MaterialDescLoader::ClampColor(Vector4 value)
	{
		value.x = Clamp01(value.x);
		value.y = Clamp01(value.y);
		value.z = Clamp01(value.z);
		value.w = Clamp01(value.w);
		return value;
	}

	std::string MaterialDescLoader::SelectTexturePath(const std::string& explicitPath, const MaterialDescSource& source, const char* semanticA, const char* semanticB)
	{
		if (!explicitPath.empty())
		{
			return explicitPath;
		}

		for (const auto& slot : source.textureSlots)
		{
			if (slot.texturePath.empty())
			{
				continue;
			}
			if (slot.semantic == semanticA || (semanticB != nullptr && slot.semantic == semanticB))
			{
				// TextureManagerへは接続せず、SourceのsemanticからMaterialDesc用パスを補完するだけに留める。
				return slot.texturePath;
			}
		}
		return {};
	}
}
