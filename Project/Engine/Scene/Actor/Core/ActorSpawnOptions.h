#pragma once
#include <Vector3.h>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///		  JSONやPrefabからActorを生成するときの追加設定
	/// -------------------------------------------------------------
	struct ActorSpawnOptions
	{
		bool applySpawnOffset = true; ///< Actor生成時にSpawnOffsetを適用するかどうか
		Vector3 spawnOffset = { 0.0f, 3.0f, 0.0f }; ///< Actor生成時に適用する位置オフセット

		bool disableAutoRegisterMainCamera = true; ///< Actor生成中にCameraComponentがMainCameraを奪わないようにするかどうか
		bool trackPrefabReference = false; ///< ファイルパスをPrefab参照元としてLevel保存へ引き継ぐかどうか
	};
}