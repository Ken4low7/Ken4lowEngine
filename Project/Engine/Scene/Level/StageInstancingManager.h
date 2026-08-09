#pragma once
#include "InstancedObject3DRenderer.h"
#include "LevelData.h"
#include "Object3D.h"

#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	struct StageInstanceSource
	{
		std::string modelPath;
		Matrix4x4 worldMatrix{};
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	struct StageInstanceBatch
	{
		std::string modelPath;
		std::unique_ptr<InstancedObject3DRenderer> renderer;
		std::vector<InstancedObject3DRenderer::InstanceData> instances;
		std::vector<StageInstanceSource> sources;
		std::vector<std::unique_ptr<Object3D>> normalFallbackObjects;
	};

	/// <summary>
	/// LevelData内の明示的なmodelPathを持つ静的配置だけを、通常Object3DまたはGPUインスタンシングへ振り分けます。
	/// </summary>
	class StageInstancingManager
	{
	public:
		/// <summary>同一modelPathが2個以上あるグループだけをインスタンスバッチへ変換します。</summary>
		void Build(const LevelData& levelData, const std::string& primaryStageModelPath, const Vector3& stageOffset);
		void Clear();

		/// <summary>単独配置など、インスタンシング対象外の静的モデルを従来のObject3Dで描画します。</summary>
		void DrawUniqueObjects();
		/// <summary>比較用に、バッチ対象を通常Object3Dで描画します。Object3D生成は必要になった時だけ行います。</summary>
		void DrawBatchSourcesNormally();
		/// <summary>同じステージモデルを複数配置している場合だけGPUインスタンシングでまとめて描画します。</summary>
		void DrawInstancedBatches();
		/// <summary>通常配置とインスタンス配置を現在の描画モードに合わせてShadow Passへ送ります。</summary>
		void DrawShadow(bool instancingEnabled, bool useInstancedDraw, bool useNormalDraw);
		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection);

		size_t GetBatchCount() const { return batches_.size(); }
		size_t GetTotalInstanceCount() const { return totalInstanceCount_; }

	private:
		void EnsureNormalFallbackObjects(StageInstanceBatch& batch);
		std::unique_ptr<Object3D> CreateNormalObject(const StageInstanceSource& source) const;

		std::vector<StageInstanceBatch> batches_{};
		std::vector<std::unique_ptr<Object3D>> uniqueObjects_{};
		size_t totalInstanceCount_ = 0;
	};
} // namespace Ken4lowEngine
