#pragma once

#include <string>

namespace Ken4lowEngine
{
	class LightManager;

	/// <summary>
	/// LightManagerが所有するライト設定をLightPreset JSONへ保存/復元するサービスです。<br/>
	/// ライトデータの所有、ImGui表示、GPU転送は行わず、既存のLightPreset保存形式を保ったまま
	/// シリアライズ責務だけをLightManagerから分離します。
	/// </summary>
	class LightPresetService
	{
	public:
		/// <summary>
		/// 現在のLightManager状態を既存LightPreset形式のJSONへ保存します。<br/>
		/// assetIdから従来と同じResources/DataAssets/LightPresets配下のパスを組み立てます。
		/// </summary>
		static bool Save(const LightManager& lightManager, const std::string& assetId);

		/// <summary>
		/// 指定パスのLightPreset JSONをLightManagerへ反映します。<br/>
		/// 保存済みプリセットとの互換を守るため、LightPreset::FromJsonのキーと補正処理は既存のまま維持します。
		/// </summary>
		static bool ApplyByPath(LightManager& lightManager, const std::string& filePath);
	};
}
