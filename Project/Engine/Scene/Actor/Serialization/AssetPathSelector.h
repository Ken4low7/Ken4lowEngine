#pragma once

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// ---------- アセットの種類を表す列挙型 ---------- ///
	enum class AssetType
	{
		Texture,	// 画像ファイル
		Model,		// 3Dモデルファイル
		SkeletalMesh, // スケルタルメッシュファイル
		Audio,		// 音声ファイル
		Font,		// フォントファイル
		Particle,	// パーティクル設定ファイル
		Animation,	// アニメーション設定ファイル
		Any			// 任意の種類
	};

	/// ----------------------------------------------------------
	///		アセットパスを選択するためのユーティリティクラス
	/// ----------------------------------------------------------
	class AssetPathSelector
	{
	public: /// ---------- メンバ関数 ---------- ///

		static const std::vector<std::string>& GetFiles(AssetType type);
		static void Refresh(AssetType type);
		static void RefreshAll();
		static bool DrawAssetSelector(const char* label, std::string& path, AssetType type);
	};
}
