#pragma once

#include "Material.h"

#include <json.hpp>
#include <string>
#include <string_view>

namespace Ken4lowEngine
{
	/// <summary>
	/// 描画Componentが共有MaterialAssetとComponent固有Materialを選択するためのCPU側Bindingです。<br/>
	/// GPUリソースは所有せず、既存MaterialCBDataへ渡すMaterialDescの解決だけを担当します。
	/// </summary>
	class MaterialBinding
	{
	public:
		/// <summary>共有MaterialAssetのIDを設定します。空文字はモデル既定Materialを表します。</summary>
		void SetAssetId(std::string_view assetId) { assetId_ = std::string(assetId); }

		/// <summary>共有MaterialAssetのIDを取得します。</summary>
		const std::string& GetAssetId() const { return assetId_; }

		/// <summary>Component固有Material Overrideの有効状態を切り替えます。</summary>
		void SetUseOverride(bool enabled);

		/// <summary>Component固有Material Overrideが有効か返します。</summary>
		bool IsUsingOverride() const { return useOverride_; }

		/// <summary>Component固有Material Overrideを取得します。</summary>
		const MaterialDesc& GetOverrideDesc() const { return overrideDesc_; }

		/// <summary>Component固有Material Overrideを編集する参照を取得します。</summary>
		MaterialDesc& GetMutableOverrideDesc() { return overrideDesc_; }

		/// <summary>共有AssetまたはComponent固有Overrideから描画用MaterialDescを解決します。</summary>
		bool Resolve(MaterialDesc& outDesc) const;

		/// <summary>モデル既定Material以外を明示的に選択しているか返します。</summary>
		bool HasBinding() const { return !assetId_.empty() || useOverride_; }

		/// <summary>Actor JSONへ保存するMaterial Binding情報を作成します。</summary>
		nlohmann::json ToJson() const;

		/// <summary>Actor JSONからMaterial Binding情報を安全に復元します。</summary>
		void FromJson(const nlohmann::json& json);

	private:
		/// <summary>共有MaterialAssetだけを解決し、Component固有Overrideは参照しません。</summary>
		bool ResolveAsset(MaterialDesc& outDesc) const;

	private:
		std::string assetId_; // 空文字の場合はモデルが元から持つMaterialをそのまま使用する。
		bool useOverride_ = false;
		MaterialDesc overrideDesc_{};
	};

	/// <summary>
	/// 共有MaterialAsset選択とComponent固有Overrideを編集する共通ImGuiを描画します。<br/>
	/// 値が変更された場合はtrueを返し、呼び出し側が各Rendererへ反映します。
	/// </summary>
	bool DrawMaterialBindingImGui(MaterialBinding& binding, const char* idScope);
}
