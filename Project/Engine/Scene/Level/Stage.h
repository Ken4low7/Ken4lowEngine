#pragma once
#include "AABB.h"
#include "Collider.h"
#include "OBB.h"
#include "Object3D.h"
#include "LevelData.h"
#include "OcclusionCullingSystem.h"
#include "StageChunkManager.h"
#include "StageInstancingManager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class CollisionManager;

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				ステージ実行時クラス
	///	--------------------------------------------------------------
	class Stage
	{
	public: /// ---------- メンバ関数 ---------- ///

		Stage() { activeRuntimeStage_ = this; }
		~Stage()
		{
			if (activeRuntimeStage_ == this) activeRuntimeStage_ = nullptr; // P10移行ランタイムが破棄済みStageを参照しないよう寿命と同期する。
		}

		static Stage* GetActiveRuntimeStage() { return activeRuntimeStage_; }

		/// <summary>
		/// ステージを初期化する
		/// ・LevelLoader でレベルデータを読む
		/// ・通常Stageは一体型Model、Instanced-only Stageは明示的なmodel配置を使う
		/// ・StageCollisionBuilder で衝突情報を作る
		/// </summary>
		void Initialize(const std::string& levelJsonPath, const std::string& defaultModelName, bool instancedOnly = false);

		/// <summary>
		/// ステージ内部状態を破棄して空に戻す
		/// </summary>
		void Clear();

		/// <summary>
		/// 更新処理
		/// </summary>
		void Update();

		/// <summary>
		/// 描画処理
		/// </summary>
		void Draw();

		/// <summary>
		/// StageChunk Bounds のデバッグワイヤーを描画する。
		/// </summary>
		void DrawChunkDebug();

		/// <summary>
		/// シャドウ描画
		/// </summary>
		void DrawShadow();

		/// <summary>
		/// シャドウ行列更新
		/// </summary>
		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection);

		/// <summary>
		/// ステージ描画モデルを Frustum Culling 対象にするか設定する。
		/// </summary>
		void SetFrustumCullingEnabled(bool enabled);

		void SetStageChunkCullingEnabled(bool enabled);
		bool IsStageChunkCullingEnabled() const;
		void SetStageChunkBoundsVisible(bool visible);
		bool IsStageChunkBoundsVisible() const;
		void SetStageChunkObjectBoundsVisible(bool visible);
		bool IsStageChunkObjectBoundsVisible() const;
		void SetStageChunkAutoExcludeLargeObjects(bool enabled);
		bool IsStageChunkAutoExcludeLargeObjects() const;
		void SetStageChunkSize(float chunkSize);
		float GetStageChunkSize() const;
		void RebuildStageChunks();
		void SetStageInstancingEnabled(bool enabled) { stageInstancingEnabled_ = enabled; }
		bool IsStageInstancingEnabled() const { return stageInstancingEnabled_; }
		void SetUseNormalStageDraw(bool enabled) { useNormalStageDraw_ = enabled; }
		bool IsUseNormalStageDraw() const { return useNormalStageDraw_; }
		void SetUseInstancedStageDraw(bool enabled) { useInstancedStageDraw_ = enabled; }
		bool IsUseInstancedStageDraw() const { return useInstancedStageDraw_; }
		size_t GetStageInstanceBatchCount() const { return stageInstancingManager_.GetBatchCount(); }
		size_t GetStageInstanceTotalCount() const { return stageInstancingManager_.GetTotalInstanceCount(); }
		StageChunkManager& GetStageChunkManager() { return stageChunkManager_; }
		const StageChunkManager& GetStageChunkManager() const { return stageChunkManager_; }
		OcclusionCullingSystem& GetOcclusionCullingSystem() { return occlusionCullingSystem_; }
		const OcclusionCullingSystem& GetOcclusionCullingSystem() const { return occlusionCullingSystem_; }

	public: /// ---------- アクセサ ---------- ///

		// StageCollision用データは移動解決向けAABBと汎用通知向けColliderを併存させ、段階統合できるよう保持する。
		const std::vector<AABB>& GetWorldAABBs() const { return worldAABBs_; }
		const std::vector<AABB>& GetFloorAABBs() const { return floorAABBs_; }
		const std::vector<AABB>& GetWallObstacleAABBs() const { return wallObstacleAABBs_; }
		const std::vector<AABB>& GetNavigationObstacleAABBs() const { return navigationObstacleAABBs_; }
		const std::vector<std::unique_ptr<Collider>>& GetWorldColliders() const { return worldColliders_; }
		std::vector<Collider*> GetWorldColliderPointers() const;
		const std::vector<OBB>& GetWallObstacleOBBs() const { return wallObstacleOBBs_; }
		const std::vector<uint8_t>& GetWallObstacleWalkable() const { return wallObstacleWalkable_; }
		const std::vector<OBB>& GetNavigationObstacleOBBs() const { return navigationObstacleOBBs_; }
		const std::vector<Collider*>& GetLadderColliders() const { return ladderColliders_; }
		const std::vector<AABB>& GetLadderAABBs() const { return ladderAABBs_; }
		const std::vector<OBB>& GetLadderOBBs() const { return ladderOBBs_; }
		// Playerの問い合わせAABBがいずれかのStatic Ladder Triggerへ入っているかを返す。
		bool CheckLadderOverlap(const AABB& playerAABB) const;

		/// <summary>
		/// 保持しているワールドコライダーを CollisionManager に登録する
		/// </summary>
		void RegisterColliders(CollisionManager* collisionManager);

		const LevelData* GetLevelData() const { return levelData_ ? levelData_.get() : nullptr; }

	private: /// ---------- メンバ変数 ---------- ///

		inline static Stage* activeRuntimeStage_ = nullptr;
		std::unique_ptr<LevelData> levelData_;
		std::unique_ptr<Object3D> stageModel_;                 // 一体型ステージだけが所有する描画モデル
		StageChunkManager stageChunkManager_;                 // 静的ステージを Chunk 単位で Draw スキップする管理クラス
		StageInstancingManager stageInstancingManager_;       // 明示的な同一modelPath配置だけをまとめる管理クラス
		OcclusionCullingSystem occlusionCullingSystem_;       // Lv4: 遮蔽物裏の StageChunk Draw を安全側で止める管理クラス
		std::vector<AABB> worldAABBs_;                        // BroadPhase・簡易範囲判定向けのStage AABB群
		std::vector<AABB> floorAABBs_;                        // 接地・スポーン補正向けFloor AABB
		std::vector<AABB> wallObstacleAABBs_;                 // Obstacle OBBを絞るBroadPhase AABB
		std::vector<AABB> navigationObstacleAABBs_;           // Navigation向け障害物AABB(Floor除外)
		std::vector<std::unique_ptr<Collider>> worldColliders_; // PhysicsWorld/DebugDraw用の正式なStatic World Collider
		std::vector<OBB> wallObstacleOBBs_;                   // Player最終横押し戻し用の壁/障害物OBB
		std::vector<uint8_t> wallObstacleWalkable_;           // Obstacle上面を床として扱えるかの拡張用フラグ
		std::vector<OBB> navigationObstacleOBBs_;             // デバッグ表示用のNavigation障害物OBB
		std::vector<Collider*> ladderColliders_;              // worldColliders_所有のStatic Triggerを参照する梯子一覧
		std::vector<AABB> ladderAABBs_;                       // Playerとの直接Overlap問い合わせに使う梯子AABB
		std::vector<OBB> ladderOBBs_;                         // 通常壁と別色表示する梯子Trigger OBB
		Vector3 offset_ = { 0.0f, 0.0f, 0.0f };              // ステージ全体オフセット
		bool stageInstancingEnabled_ = true;
		bool useNormalStageDraw_ = true;
		bool useInstancedStageDraw_ = true;
	};
} // namespace Ken4lowEngine
