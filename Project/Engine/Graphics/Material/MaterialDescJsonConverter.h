#pragma once

#include "MaterialDescLoader.h"

#include <json.hpp>

namespace Ken4lowEngine
{
	/// <summary>
	/// nlohmann::jsonオブジェクトとMaterialDescSourceを相互変換するCPU側Converterです。<br/>
	/// ファイルI/O、TextureManager、MaterialRepository、描画経路には接続せず、Jsonキー規約とSource変換だけを
	/// 1箇所へ集めることで、将来Material Jsonの保存形式を安全に調整できるようにします。
	/// </summary>
	class MaterialDescJsonConverter
	{
	public:
		/// <summary>
		/// Material Jsonで使うキー名を集約した定数群です。<br/>
		/// キー名を1箇所に集めることで、Json保存形式を変更するときの修正漏れを防ぎます。
		/// </summary>
		struct Keys
		{
			static constexpr const char* MaterialId = "materialId";
			static constexpr const char* MaterialName = "materialName";
			static constexpr const char* SourcePath = "sourcePath";
			static constexpr const char* SourceKind = "sourceKind";
			static constexpr const char* PreferPbrWorkflow = "preferPbrWorkflow";

			static constexpr const char* Legacy = "legacy";
			static constexpr const char* LegacyColor = "color";
			static constexpr const char* LegacyShininess = "shininess";
			static constexpr const char* LegacyReflectionRate = "reflectionRate";
			static constexpr const char* LegacyRoughness = "roughness";
			static constexpr const char* LegacyBaseColorTexture = "baseColorTexture";
			static constexpr const char* LegacyUsePointSampling = "usePointSampling";

			static constexpr const char* Pbr = "pbr";
			static constexpr const char* PbrBaseColor = "baseColor";
			static constexpr const char* PbrMetallic = "metallic";
			static constexpr const char* PbrRoughness = "roughness";
			static constexpr const char* PbrNormalScale = "normalScale";
			static constexpr const char* PbrOcclusionStrength = "occlusionStrength";
			static constexpr const char* PbrEmissiveColor = "emissiveColor";
			static constexpr const char* PbrEmissiveStrength = "emissiveStrength";
			static constexpr const char* PbrBaseColorTexture = "baseColorTexture";
			static constexpr const char* PbrNormalTexture = "normalTexture";
			static constexpr const char* PbrMetallicRoughnessTexture = "metallicRoughnessTexture";
			static constexpr const char* PbrOcclusionTexture = "occlusionTexture";
			static constexpr const char* PbrEmissiveTexture = "emissiveTexture";

			static constexpr const char* TextureSlots = "textureSlots";
			static constexpr const char* TextureSlotSemantic = "semantic";
			static constexpr const char* TextureSlotPath = "texturePath";
		};

		/// <summary>
		/// JsonオブジェクトからMaterialDescSourceを作成します。<br/>
		/// 欠損キーや型違いはSourceの既定値へフォールバックし、ファイルI/Oや描画接続は行いません。
		/// </summary>
		static MaterialDescSource FromJson(const nlohmann::json& json);

		/// <summary>
		/// MaterialDescSourceをJsonオブジェクトへ変換します。<br/>
		/// まだファイル保存は行わず、Material Json保存形式を固めるためのオブジェクト生成だけを担当します。
		/// </summary>
		static nlohmann::json ToJson(const MaterialDescSource& source);

		/// <summary>
		/// MaterialSourceKindをJson保存用文字列へ変換します。
		/// </summary>
		static const char* ToString(MaterialSourceKind kind);

		/// <summary>
		/// Json文字列からMaterialSourceKindへ変換します。<br/>
		/// 未知の値はManualへフォールバックし、古いJsonでも安全に読み込めるようにします。
		/// </summary>
		static MaterialSourceKind SourceKindFromString(const std::string& text);
	};
}
