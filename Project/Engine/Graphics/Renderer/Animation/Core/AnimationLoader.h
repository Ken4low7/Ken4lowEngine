#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "ModelData.h"


namespace Ken4lowEngine
{

	// -------------------------------------------------------------
	///				   アニメーションローダークラス
	/// -------------------------------------------------------------
	class AnimationLoader
	{
	public: /// ---------- 構造体 ---------- ///

		struct AnimationClip
		{
			std::string name;
			Animation animation;
		};

		// ロード設定
		struct Settings
		{
			std::string animationFilePath = "Resources/Models/Sources/"; ///< アニメーションファイルのパス
			bool rightHandToLeftHand = true; ///< 右手座標系を左手座標系に変換するか
		};

	public: /// ---------- メンバ関数 ---------- ///

		// ファイル内の全アニメーションを読み込む
		static std::vector<AnimationClip> LoadAllAnimations(const std::string& filePath, const Settings& settings = {});

		// アニメーションファイルを読み込み、最初のアニメーションを返す
		static Animation LoadFirstAnimation(const std::string& filePath, const Settings& settings = {});

		// 複数アニメーションファイルからインデックス指定で読み込む
		static Animation LoadByIndexAnimation(const std::string& filePath, uint32_t animationIndex, const Settings& settings = {});
	};

}
