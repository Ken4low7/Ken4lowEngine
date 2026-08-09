#pragma once
#include "LevelData.h"
#include "Object3D.h"

#include <memory>
#include <string>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				ステージ描画アセット生成
	/// -------------------------------------------------------------
	class StageAssetLoader
	{
	public:
		/// <summary>一体型ステージとして使うモデルを解決します。既定モデルを最優先します。</summary>
		static std::string ResolveStageModelName(const LevelData& levelData, const std::string& defaultModelName);

		/// <summary>
		/// LevelData からステージ用描画モデルを構築する
		/// </summary>
		static std::unique_ptr<Object3D> BuildStageModel(
			const LevelData& levelData,
			const std::string& defaultModelName,
			const Vector3& offset);
	};
} // namespace Ken4lowEngine
