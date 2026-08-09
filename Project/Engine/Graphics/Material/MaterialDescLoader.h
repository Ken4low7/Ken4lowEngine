#pragma once

#include "MaterialAsset.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// Json/glTF/MaterialEditorなど外部入力元の種類を表すCPU側メタ情報です。<br/>
	/// 描画処理には使わず、将来ログやEditor表示で「どこから来たMaterialか」を判断するために保持します。
	/// </summary>
	enum class MaterialSourceKind
	{
		Manual,
		Json,
		Gltf,
		MaterialEditor,
	};

	/// <summary>
	/// Json/glTF/MaterialEditorから来るTexture slotを一時保持する中間入力データです。<br/>
	/// TextureManagerには接続せず、semanticとpathをMaterialDesc変換前のCPU側情報として持つだけにします。
	/// </summary>
	struct MaterialSourceTextureSlot
	{
		std::string semantic;
		std::string texturePath;
	};

	/// <summary>
	/// Json/glTF/手動定義などから来るMaterial情報を受け取る中間入力構造体です。<br/>
	/// 直接MaterialDescへ変換せず一度Sourceとして保持することで、Legacy/PBR変換方針、Texture slot、
	/// sourcePathなどの入力元メタ情報を失わずに整理できます。
	/// </summary>
	struct MaterialDescSource
	{
		MaterialSourceKind sourceKind = MaterialSourceKind::Manual;
		std::string materialId;
		std::string materialName;
		std::string sourcePath;
		bool preferPbrWorkflow = false;

		Vector4 legacyColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		float legacyShininess = 32.0f;
		float legacyReflectionRate = 0.0f;
		float legacyRoughness = 0.5f;
		bool usePointSampling = false;

		Vector4 baseColorFactor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		float metallicFactor = 0.0f;
		float roughnessFactor = 0.5f;
		float normalScale = 1.0f;
		float occlusionStrength = 1.0f;
		Vector4 emissiveFactor = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

		std::string baseColorTexturePath;
		std::string metallicRoughnessTexturePath;
		std::string normalTexturePath;
		std::string occlusionTexturePath;
		std::string emissiveTexturePath;
		std::vector<MaterialSourceTextureSlot> textureSlots; // 未知のTexture slotも失わず保持し、将来Texture slot管理へ渡すための入口にする。
	};

	/// <summary>
	/// Json/glTF/手動定義などの入力からMaterialDescを組み立てるためのCPU側Loaderです。<br/>
	/// 今回は既存ModelLoader、TextureManager、MaterialCBData、HLSLへ接続せず、MaterialRepository登録前の
	/// 値生成・正規化・検証だけを担当する薄い入口として扱います。
	/// </summary>
	class MaterialDescLoader
	{
	public:
		/// <summary>
		/// 既存Material::Initializeに近いDefault MaterialDescを作成します。<br/>
		/// 既存Forward描画のMaterialCBData互換を守るため、PBRではなくLegacyワークフローを既定にします。
		/// </summary>
		static MaterialDesc CreateDefaultDesc();

		/// <summary>
		/// Legacyワークフロー用のMaterialDescを作成します。<br/>
		/// 旧来のcolor/shininess/reflection/roughnessを保持し、HLSL定数バッファへはまだ転送しません。
		/// </summary>
		static MaterialDesc CreateLegacyDesc(
			Vector4 color = Vector4(1.0f, 1.0f, 1.0f, 1.0f),
			float shininess = 32.0f,
			float reflection = 0.0f,
			float roughness = 0.5f,
			const std::string& baseColorTexturePath = "");

		/// <summary>
		/// PBRワークフロー用のMaterialDescを作成します。<br/>
		/// glTF Material拡張やMaterialEditorから値を受け取る入口を想定しますが、Shader切り替えやTextureロードは行いません。
		/// </summary>
		static MaterialDesc CreatePbrDesc(
			Vector4 baseColorFactor = Vector4(1.0f, 1.0f, 1.0f, 1.0f),
			float metallicFactor = 0.0f,
			float roughnessFactor = 0.5f,
			const std::string& baseColorTexturePath = "",
			const std::string& metallicRoughnessTexturePath = "",
			const std::string& normalTexturePath = "",
			const std::string& occlusionTexturePath = "",
			const std::string& emissiveTexturePath = "");

		/// <summary>
		/// MaterialDescSourceからMaterialDescを作成します。<br/>
		/// SourceはJson/glTF/MaterialEditor入力の中間表現であり、この関数ではCPU側Descへ変換するだけで描画へ接続しません。
		/// </summary>
		static MaterialDesc CreateFromSource(const MaterialDescSource& source);

		/// <summary>
		/// MaterialDescを安全な範囲へ正規化して返します。<br/>
		/// Json/glTF/手動入力から来る値が範囲外でも、将来GPUへ渡す前に破綻しにくいCPU側値へ整えます。
		/// </summary>
		static MaterialDesc NormalizeDesc(const MaterialDesc& desc);

		/// <summary>
		/// MaterialDescSourceを安全な範囲へ正規化して返します。<br/>
		/// 外部入力から来る値をMaterialDescへ変換する前に整え、Legacy/PBRどちらの経路でも破綻しにくくします。
		/// </summary>
		static MaterialDescSource NormalizeSource(const MaterialDescSource& source);

		/// <summary>
		/// MaterialDescが基本的に利用可能かを検証します。<br/>
		/// outMessageを指定すると、将来MaterialEditorやログ表示へ出すための理由文字列を受け取れます。
		/// </summary>
		static bool ValidateDesc(const MaterialDesc& desc, std::string* outMessage = nullptr);

		/// <summary>
		/// MaterialDescSourceが基本的に利用可能かを検証します。<br/>
		/// ファイル存在確認やTextureロードは行わず、数値範囲とTexture slotの最低限の整合性だけを確認します。
		/// </summary>
		static bool ValidateSource(const MaterialDescSource& source, std::string* outMessage = nullptr);

		/// <summary>
		/// MaterialRepositoryへ登録しやすいMaterialAssetを作成します。<br/>
		/// Descを正規化してからAsset化しますが、既存描画へ自動接続しないことで見た目の変化を防ぎます。
		/// </summary>
		static MaterialAsset CreateAsset(const std::string& id, const std::string& name, const MaterialDesc& desc);

	private:
		/// <summary>
		/// 値を0.0fから1.0fの範囲へ丸めます。
		/// </summary>
		static float Clamp01(float value);

		/// <summary>
		/// 負の値を0.0fへ丸めます。
		/// </summary>
		static float ClampNonNegative(float value);

		/// <summary>
		/// 色要素を0.0fから1.0fの範囲へ丸めます。
		/// </summary>
		static Vector4 ClampColor(Vector4 value);

		/// <summary>
		/// 明示パスが空の場合だけ、Texture slotのsemanticから対応するパスを補完します。
		/// </summary>
		static std::string SelectTexturePath(const std::string& explicitPath, const MaterialDescSource& source, const char* semanticA, const char* semanticB = nullptr);
	};
}
