#pragma once
#include "BaseScene.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				　　 　シーン工場　・　抽象クラス
	/// -------------------------------------------------------------
	class AbstractSceneFactory
	{
	public: /// ---------- 仮想メンバ関数 ---------- ///

		/// <summary>
		/// デストラクタ
		/// </summary>
		virtual ~AbstractSceneFactory() = default;

		/// <summary>
		/// シーン生成
		/// </summary>
		/// <param name="sceneName">シーン名</param>
		/// <returns>シーンを返す</returns>
		virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;

	};

}