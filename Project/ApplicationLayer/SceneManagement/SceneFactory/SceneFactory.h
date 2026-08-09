#pragma once
#include "AbstractSceneFactory.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				　		ゲーム用のシーン工場
	/// -------------------------------------------------------------
	class SceneFactory : public AbstractSceneFactory
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// シーン生成
		/// </summary>
		/// <param name="sceneName">シーン名</param>
		/// <returns>生成したシーン</returns>
		std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;
	};

}