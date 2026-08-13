#pragma once
#include "BaseScene.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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

		/// <summary>指定Class名をこのFactoryが生成できるか返します。</summary>
		virtual bool CanCreateScene(const std::string& sceneName) const = 0;

		/// <summary>EditorのScene一覧へ公開する登録済みClass名を返します。</summary>
		virtual std::vector<std::string> GetRegisteredSceneNames() const = 0; // Scene一覧のハードコードをなくしFactory登録を唯一の情報源にする。
	};

}
