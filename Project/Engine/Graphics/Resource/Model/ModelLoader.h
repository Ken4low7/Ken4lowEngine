#pragma once
#include "ModelData.h"
#include <string>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///		モデル読み込みの入口クラス
	/// -------------------------------------------------------------
	/// モデルのロードはこのクラスを経由して行います。
	///
	/// 優先順位:
	///   1. Compiled 側に .kmesh があれば KMeshLoader で読む
	///   2. 無ければ Sources 側の元モデルを AssimpLoader で読む
	///
	/// こうしておくことで、kmesh 導入途中でも既存モデルを壊さず移行できます。
	class ModelLoader
	{
	public:
		/// <summary>
		/// 論理パスからモデルを読み込みます。
		/// Compiled 優先、無ければ Sources を使います。
		/// </summary>
		/// <param name="logicalPath">例: "Characters/body.gltf"</param>
		/// <returns>読み込まれた ModelData</returns>
		static ModelData LoadModel(const std::string& logicalPath);
	};
}