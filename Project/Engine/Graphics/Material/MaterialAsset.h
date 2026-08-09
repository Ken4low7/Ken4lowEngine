#pragma once

#include "Material.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// MaterialDescのうち、現在どのワークフローを優先して扱うかを表すCPU側の分類です。<br/>
	/// 既存Forward用MaterialCBDataとは独立しており、PBR描画へ接続する前段階の判定だけに使います。
	/// </summary>
	enum class MaterialWorkflow
	{
		Legacy,
		PBR,
	};

	/// <summary>
	/// 将来のTexture slot管理へ進むためのCPU側スロット情報です。<br/>
	/// 今回はTextureManagerへロードを依頼せず、glTF/Json/MaterialEditorから受け取ったパスを保持するだけに留めます。
	/// </summary>
	struct MaterialTextureSlot
	{
		std::string semantic;
		std::string texturePath;
	};

	/// <summary>
	/// 1つのMaterialデータを表すCPU側アセットです。<br/>
	/// MaterialDescを保持しますが、GPU定数バッファ生成、HLSL転送、Shader/PSO管理は行いません。
	/// 既存MaterialCBDataの描画互換を守りながら、PBR/IBL/glTF Materialへ接続するための薄い受け皿として扱います。
	/// </summary>
	class MaterialAsset
	{
	public:
		/// <summary>
		/// 空のMaterialAssetを作成します。<br/>
		/// Repositoryへ登録する場合は、SetIdで一意なIDを設定してから使います。
		/// </summary>
		MaterialAsset() = default;

		/// <summary>
		/// ID、表示名、MaterialDescを指定してMaterialAssetを作成します。<br/>
		/// ここではCPU側データだけを保持し、既存描画へ自動接続しないことで見た目の変化を避けます。
		/// </summary>
		MaterialAsset(std::string id, std::string name, const MaterialDesc& desc);

		/// <summary>
		/// 既存Forward描画と同じ白ベースのDefault Materialを作成します。<br/>
		/// MaterialCBDataの初期値に近いLegacy設定を保持するだけで、GPUリソースは生成しません。
		/// </summary>
		static MaterialAsset CreateDefault(const std::string& id = "DefaultMaterial", const std::string& name = "Default Material");

		/// <summary>
		/// MaterialAssetの一意IDを取得します。
		/// </summary>
		const std::string& GetId() const { return id_; }

		/// <summary>
		/// MaterialAssetの一意IDを設定します。<br/>
		/// Repository登録後のID変更は検索キーとずれるため、登録前の初期化用途を想定しています。
		/// </summary>
		void SetId(const std::string& id) { id_ = id; }

		/// <summary>
		/// エディタやデバッグ表示で使うMaterial名を取得します。
		/// </summary>
		const std::string& GetName() const { return name_; }

		/// <summary>
		/// エディタやデバッグ表示で使うMaterial名を設定します。
		/// </summary>
		void SetName(const std::string& name) { name_ = name; }

		/// <summary>
		/// 保持しているMaterialDescを取得します。<br/>
		/// 現時点では描画側へ反映せず、将来のMaterialEditor/glTFローダー連携の読み取り口として使います。
		/// </summary>
		const MaterialDesc& GetDesc() const { return desc_; }

		/// <summary>
		/// MaterialDescを差し替えます。<br/>
		/// 既存MaterialCBDataとの互換を守るため、この関数ではGPU転送やHLSL用バッファ更新を行いません。
		/// </summary>
		void SetDesc(const MaterialDesc& desc);

		/// <summary>
		/// Legacy/PBRのどちらを優先するMaterialかを返します。
		/// </summary>
		MaterialWorkflow GetWorkflow() const;

		/// <summary>
		/// PBRワークフローを優先するかを設定します。<br/>
		/// 今回はCPU側フラグだけを更新し、ShaderやPipelineの切り替えは行いません。
		/// </summary>
		void SetPreferPbrWorkflow(bool enabled);

		/// <summary>
		/// 将来のTexture slot管理用に、任意のスロット名とテクスチャパスを追加します。<br/>
		/// TextureManagerのロードやSRV確保はここでは行いません。
		/// </summary>
		void AddTextureSlot(const std::string& semantic, const std::string& texturePath);

		/// <summary>
		/// 登録済みTexture slotを取得します。
		/// </summary>
		const std::vector<MaterialTextureSlot>& GetTextureSlots() const { return textureSlots_; }

		/// <summary>
		/// Texture slot情報を消去します。<br/>
		/// GPUリソースを所有していないため、Descriptor解放などは不要です。
		/// </summary>
		void ClearTextureSlots();

	private:
		std::string id_;
		std::string name_;
		MaterialDesc desc_{};
		std::vector<MaterialTextureSlot> textureSlots_; // CPU側のスロット候補だけを保持し、TextureManagerにはまだ接続しない。
	};
}
