#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "Vector3.h"
#include "AABB.h"
#include "OBB.h"

namespace Ken4lowEngine
{
	enum class StageHitType : uint8_t
	{
		None,
		Floor,
		Wall,
		Ceiling,
	};

	enum class StageHitShape : uint8_t
	{
		None,
		AABB,
		OBB,
	};

	enum class StageCorrectionAxis : uint8_t
	{
		None,
		X,
		Y,
		Z,
		OBBNormal,
	};

	const char* ToString(StageHitType hitType);
	const char* ToString(StageHitShape hitShape);
	const char* ToString(StageCorrectionAxis correctionAxis);

	/// -------------------------------------------------------------
	///				　		ワールド衝突結果構造体
	/// -------------------------------------------------------------
	struct WorldCollisionResult
	{
		Vector3 fixedCenter{};
		bool grounded = false;   // Player用（Boss/Enemyは無視でOK）
		size_t obbHitCount = 0; // 障害物OBBによるXZ押し戻し回数。
		StageHitType lastHitType = StageHitType::None;
		StageHitShape lastHitShape = StageHitShape::None;
		StageCorrectionAxis lastCorrectionAxis = StageCorrectionAxis::None;
		bool groundedByStageTop = false;
	};

	/// -------------------------------------------------------------
	///				　		ワールド衝突設定構造体
	/// -------------------------------------------------------------
	struct WorldCollisionSettings
	{
		Vector3 half{ 0.8f, 2.0f, 0.8f };
		Vector3 centerOffset{ 0.0f, 0.0f, 0.0f }; // 見た目座標→物理中心の差
		float eps = 0.002f;
		float topContactTolerance = 0.08f; // 上面交差を取りこぼさないための着地許容幅。
	};

	/// -------------------------------------------------------------
	///				　		ワールド衝突解決クラス
	/// -------------------------------------------------------------
	class WorldCollisionResolver
	{
	public: /// ---------- 静的メンバ関数 ---------- ///

		/*
		Stage Collision責務メモ:
		- 汎用CollisionSystemはCollider同士の通知、ObjectChannel/Response、Query/Traceを担当する。
		- WorldCollisionResolverはキャラクター移動に対する静的AABB押し戻し、接地、縦速度補正を担当する。
		- StageCollisionBuilder/StageはLevelDataからFloor/Obstacle/Navigation用AABB/OBBを構築・保持する。
		- 将来統合する場合も、押し戻し順序(X,Z,Y)やgrounded更新はゲーム挙動に直結するため最後まで分離して検証する。
		*/

		/// <summary>
		/// ワールド内のAABB群と移動前後の座標をもとに衝突を検出・解決する静的関数。プレイヤーの着地判定やジャンプ速度の更新に対応する。
		/// </summary>
		/// <param name="worldAABBs">ワールド内の AABB（軸平行境界ボックス）の配列。衝突判定の対象となる環境データ。</param>
		/// <param name="s">WorldCollisionSettings 型の設定。衝突解決に用いるパラメータ（閾値やマージンなど）。</param>
		/// <param name="oldTranslate">描画座標での移動前の位置。（コメント通り、従来通り oldPos を渡す）</param>
		/// <param name="newTranslate">描画座標での移動後の位置。（現在の body_.transform.translate_ を渡す）</param>
		/// <param name="useGrounded">着地判定を考慮するかどうか。プレイヤーでは true を指定することが想定される。</param>
		/// <param name="inoutJumpVelocity">（オプション）プレイヤー用の入出力ジャンプ速度へのポインタ。衝突解決によりジャンプ速度を修正する場合に値が書き換えられる。Boss/Enemy 等では nullptr を渡す。デフォルトは nullptr。</param>
		/// <param name="obstacleBroadPhaseAABBs">最終AABB押し戻しから除外し、OBB候補絞り込みに使う障害物AABB。</param>
		/// <param name="obstacleOBBs">斜め障害物のXZ NarrowPhaseと最小押し戻しに使うOBB。</param>
		/// <returns>WorldCollisionResult 型。衝突の有無や補正後の位置・法線、着地状態など、衝突解決の結果を含む。</returns>
		static WorldCollisionResult Resolve(
			const std::vector<AABB>& worldAABBs,
			const WorldCollisionSettings& s,
			const Vector3& oldTranslate,      // 描画座標（今まで通り oldPos を渡す）
			const Vector3& newTranslate,      // 描画座標（今の body_.transform.translate_）
			bool useGrounded,                 // Playerだけ true
			float* inoutJumpVelocity = nullptr, // Playerだけ渡す（Boss/Enemyはnullptr）
			const std::vector<AABB>* obstacleBroadPhaseAABBs = nullptr,
			const std::vector<OBB>* obstacleOBBs = nullptr,
			const std::vector<uint8_t>* obstacleWalkable = nullptr
		);
	};


} // namespace Ken4lowEngine
