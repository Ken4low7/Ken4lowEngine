#pragma once
#include "AABB.h"
#include "Collider.h"
#include "OBB.h"
#include "LevelData.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Ken4lowEngine
{
	// Ladderを通常Stageとは別のPhysicsWorld Responseへ割り当てる共有レイヤー。
	inline constexpr uint32_t kLadderPhysicsLayer = 8u;

	struct StageCollisionBuildResult
	{
		std::vector<AABB> worldAABBs; // Stage全体のBroadPhase・簡易範囲判定用AABB。
		std::vector<AABB> floorAABBs; // 接地とY押し戻しを維持するFloor AABB。
		std::vector<AABB> wallObstacleAABBs; // Obstacle OBBを絞り込む包み込みBroadPhase AABB。
		std::vector<AABB> navigationObstacleAABBs;
		std::vector<std::unique_ptr<Collider>> worldColliders; // PhysicsWorld/DebugDrawへ渡す正式なStatic Stage Collider。
		std::vector<OBB> wallObstacleOBBs; // 斜め障害物の最終判定・横押し戻し用OBB。
		std::vector<uint8_t> wallObstacleWalkable; // 将来のLevel設定へ拡張できるObstacle上面歩行可否。
		std::vector<OBB> navigationObstacleOBBs;
		std::vector<Collider*> ladderColliders; // worldColliders所有のStatic Triggerを参照する梯子専用一覧。
		std::vector<AABB> ladderAABBs; // 毎フレームのPlayer Trigger問い合わせに使う梯子包み込みAABB。
		std::vector<OBB> ladderOBBs; // 梯子Triggerの判定・専用デバッグ描画に使う回転OBB。
	};

	/*
	StageCollisionBuilder責務メモ:
	- LevelDataのBOX colliderから、BroadPhase用AABB、障害物NarrowPhase用OBB、正式なStatic Colliderを生成する。
	- Mesh/Polygonを直接衝突判定には使わず、ステージ衝突は単純Primitiveへ変換して扱う。
	- 将来は生成結果をStageCollisionSystemへ分けられるが、現段階ではStageが用途別配列を保持して既存挙動を保つ。
	*/

	/// -------------------------------------------------------------
	///				ステージ衝突生成
	/// -------------------------------------------------------------
	class StageCollisionBuilder
	{
	public:
		/// <summary>
		/// LevelData からワールド衝突情報を構築する
		/// </summary>
		static StageCollisionBuildResult Build(const LevelData& levelData, const Vector3& offset);

		// Build結果が所有するColliderを、PhysicsWorld登録用の参照ポインタ一覧として取得する。
		static std::vector<Collider*> GetColliders(const StageCollisionBuildResult& result);
	};
} // namespace Ken4lowEngine
