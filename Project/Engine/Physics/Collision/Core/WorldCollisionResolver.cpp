#include "WorldCollisionResolver.h"

#include <cfloat>
#include <cmath>

namespace Ken4lowEngine
{
	const char* ToString(StageHitType hitType)
	{
		switch (hitType)
		{
		case StageHitType::Floor: return "Floor";
		case StageHitType::Wall: return "Wall";
		case StageHitType::Ceiling: return "Ceiling";
		default: return "None";
		}
	}

	const char* ToString(StageHitShape hitShape)
	{
		switch (hitShape)
		{
		case StageHitShape::AABB: return "AABB";
		case StageHitShape::OBB: return "OBB";
		default: return "None";
		}
	}

	const char* ToString(StageCorrectionAxis correctionAxis)
	{
		switch (correctionAxis)
		{
		case StageCorrectionAxis::X: return "X";
		case StageCorrectionAxis::Y: return "Y";
		case StageCorrectionAxis::Z: return "Z";
		case StageCorrectionAxis::OBBNormal: return "OBBNormal";
		default: return "None";
		}
	}

	namespace
	{
		struct VerticalContactCandidate
		{
			bool valid = false;
			StageHitType hitType = StageHitType::None;
			StageHitShape hitShape = StageHitShape::None;
			size_t shapeIndex = 0;
			float correctedCenterY = 0.0f;
			float surfaceY = 0.0f;
		};

		bool IsSameAABB(const AABB& lhs, const AABB& rhs)
		{
			constexpr float kEpsilon = 0.0001f;
			return std::fabs(lhs.min.x - rhs.min.x) <= kEpsilon &&
				std::fabs(lhs.min.y - rhs.min.y) <= kEpsilon &&
				std::fabs(lhs.min.z - rhs.min.z) <= kEpsilon &&
				std::fabs(lhs.max.x - rhs.max.x) <= kEpsilon &&
				std::fabs(lhs.max.y - rhs.max.y) <= kEpsilon &&
				std::fabs(lhs.max.z - rhs.max.z) <= kEpsilon;
		}

		bool IsObstacleBroadPhaseAABB(const AABB& candidate, const std::vector<AABB>* obstacleAABBs)
		{
			// Obstacleの包み込みAABBは最終押し戻しから除外し、対応するOBBへNarrowPhaseを任せる。
			if (!obstacleAABBs)
			{
				return false;
			}
			for (const AABB& obstacleAABB : *obstacleAABBs)
			{
				if (IsSameAABB(candidate, obstacleAABB))
				{
					return true;
				}
			}
			return false;
		}

		bool OverlapsAABBXZ(const Vector3& center, const Vector3& half, const AABB& aabb)
		{
			return center.x + half.x >= aabb.min.x && center.x - half.x <= aabb.max.x &&
				center.z + half.z >= aabb.min.z && center.z - half.z <= aabb.max.z;
		}

		bool OverlapsOBBXZ(const Vector3& center, const Vector3& half, const OBB& obb)
		{
			const Vector3 fromOBB = center - obb.center;
			const Vector3 axisX = obb.orientations[0];
			const Vector3 axisZ = obb.orientations[2];
			const float localX = Vector3::Dot(fromOBB, axisX);
			const float localZ = Vector3::Dot(fromOBB, axisZ);
			const float playerRadiusX = std::fabs(axisX.x) * half.x + std::fabs(axisX.z) * half.z;
			const float playerRadiusZ = std::fabs(axisZ.x) * half.x + std::fabs(axisZ.z) * half.z;
			// OBBローカルXZへPlayer幅を投影し、斜め上面とPlayer足元の重なりを判定する。
			return std::fabs(localX) <= obb.size.x + playerRadiusX &&
				std::fabs(localZ) <= obb.size.z + playerRadiusZ;
		}

		VerticalContactCandidate FindPriorityVerticalContact(
			const std::vector<AABB>& worldAABBs,
			const std::vector<AABB>* obstacleBroadPhaseAABBs,
			const std::vector<OBB>* obstacleOBBs,
			const std::vector<uint8_t>* obstacleWalkable,
			const WorldCollisionSettings& settings,
			const Vector3& oldCenter,
			const Vector3& newCenter)
		{
			VerticalContactCandidate best{};
			const float deltaY = newCenter.y - oldCenter.y;
			const bool movingDown = deltaY <= 0.0f;
			const bool movingUp = deltaY > 0.0f;
			const float oldFoot = oldCenter.y - settings.half.y;
			const float newFoot = newCenter.y - settings.half.y;
			const float oldHead = oldCenter.y + settings.half.y;
			const float newHead = newCenter.y + settings.half.y;

			for (size_t i = 0; i < worldAABBs.size(); ++i)
			{
				const AABB& aabb = worldAABBs[i];
				if (IsObstacleBroadPhaseAABB(aabb, obstacleBroadPhaseAABBs) || !OverlapsAABBXZ(newCenter, settings.half, aabb))
				{
					continue;
				}

				if (movingDown && oldFoot >= aabb.max.y - settings.topContactTolerance && newFoot <= aabb.max.y + settings.topContactTolerance)
				{
					if (!best.valid || best.hitType != StageHitType::Floor || aabb.max.y > best.surfaceY)
					{
						// 下降中に上面を跨いだAABBは、横押し戻しより先に床候補として記録する。
						best = { true, StageHitType::Floor, StageHitShape::AABB, i, aabb.max.y + settings.half.y + settings.eps, aabb.max.y };
					}
				}
				else if (movingUp && oldHead <= aabb.min.y + settings.topContactTolerance && newHead >= aabb.min.y - settings.topContactTolerance)
				{
					if (!best.valid || best.hitType != StageHitType::Ceiling || aabb.min.y < best.surfaceY)
					{
						// 上昇中に下面を跨いだAABBは天井候補とし、同じ形状からの横押し戻しを避ける。
						best = { true, StageHitType::Ceiling, StageHitShape::AABB, i, aabb.min.y - settings.half.y - settings.eps, aabb.min.y };
					}
				}
			}

			if (!obstacleOBBs)
			{
				return best;
			}

			for (size_t i = 0; i < obstacleOBBs->size(); ++i)
			{
				const OBB& obb = (*obstacleOBBs)[i];
				if (!OverlapsOBBXZ(newCenter, settings.half, obb))
				{
					continue;
				}
				const float yExtent =
					std::fabs(obb.orientations[0].y) * obb.size.x +
					std::fabs(obb.orientations[1].y) * obb.size.y +
					std::fabs(obb.orientations[2].y) * obb.size.z;
				const float topY = obb.center.y + yExtent;
				const float bottomY = obb.center.y - yExtent;
				const bool walkable = !obstacleWalkable || i >= obstacleWalkable->size() || (*obstacleWalkable)[i] != 0u;

				if (movingDown && walkable && oldFoot >= topY - settings.topContactTolerance && newFoot <= topY + settings.topContactTolerance)
				{
					if (!best.valid || best.hitType != StageHitType::Floor || topY > best.surfaceY)
					{
						// 歩行可能OBBの上面交差は最優先の床候補とし、同フレームのOBB横反発を抑える。
						best = { true, StageHitType::Floor, StageHitShape::OBB, i, topY + settings.half.y + settings.eps, topY };
					}
				}
				else if (movingUp && oldHead <= bottomY + settings.topContactTolerance && newHead >= bottomY - settings.topContactTolerance)
				{
					if (!best.valid || best.hitType != StageHitType::Ceiling || bottomY < best.surfaceY)
					{
						best = { true, StageHitType::Ceiling, StageHitShape::OBB, i, bottomY - settings.half.y - settings.eps, bottomY };
					}
				}
			}

			return best;
		}

		size_t ResolveObstacleOBBsXZ(
			const std::vector<AABB>* obstacleBroadPhaseAABBs,
			const std::vector<OBB>* obstacleOBBs,
			const WorldCollisionSettings& settings,
			float targetCenterY,
			Vector3& fixedCenter,
			size_t skippedObstacleIndex,
			bool skipObstacle,
			WorldCollisionResult& result)
		{
			if (!obstacleOBBs)
			{
				return 0;
			}

			size_t hitCount = 0;
			for (size_t obstacleIndex = 0; obstacleIndex < obstacleOBBs->size(); ++obstacleIndex)
			{
				if (skipObstacle && obstacleIndex == skippedObstacleIndex)
				{
					continue;
				}
				const OBB& obstacle = (*obstacleOBBs)[obstacleIndex];
				if (obstacleBroadPhaseAABBs && obstacleIndex < obstacleBroadPhaseAABBs->size())
				{
					const Vector3 playerCenter{ fixedCenter.x, targetCenterY, fixedCenter.z };
					const AABB playerAABB{ playerCenter - settings.half, playerCenter + settings.half };
					const AABB& obstacleBroadPhase = (*obstacleBroadPhaseAABBs)[obstacleIndex];
					// 包み込みAABBで候補を絞り、重なる障害物だけ回転OBBのXZ SATへ進める。
					if (playerAABB.max.x < obstacleBroadPhase.min.x || playerAABB.min.x > obstacleBroadPhase.max.x ||
						playerAABB.max.y < obstacleBroadPhase.min.y || playerAABB.min.y > obstacleBroadPhase.max.y ||
						playerAABB.max.z < obstacleBroadPhase.min.z || playerAABB.min.z > obstacleBroadPhase.max.z)
					{
						continue;
					}
				}

				const float obstacleYExtent =
					std::fabs(obstacle.orientations[0].y) * obstacle.size.x +
					std::fabs(obstacle.orientations[1].y) * obstacle.size.y +
					std::fabs(obstacle.orientations[2].y) * obstacle.size.z;
				if (targetCenterY + settings.half.y < obstacle.center.y - obstacleYExtent ||
					targetCenterY - settings.half.y > obstacle.center.y + obstacleYExtent)
				{
					continue;
				}

				Vector3 candidateAxes[5] = {
					{ 1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f },
					{}, {}, {},
				};
				for (int i = 0; i < 3; ++i)
				{
					const Vector3 projectedAxis{ obstacle.orientations[i].x, 0.0f, obstacle.orientations[i].z };
					if (Vector3::LengthSquared(projectedAxis) > 0.000001f)
					{
						// 投影されたOBB辺の法線をXZ分離軸に加え、斜め壁の向きに沿った押し戻しを求める。
						candidateAxes[i + 2] = Vector3::Normalize({ -projectedAxis.z, 0.0f, projectedAxis.x });
					}
				}

				bool overlaps = true;
				float minimumPenetration = FLT_MAX;
				Vector3 minimumAxis{};
				float minimumDistance = 0.0f;
				for (const Vector3& rawAxis : candidateAxes)
				{
					if (Vector3::LengthSquared(rawAxis) <= 0.000001f)
					{
						continue;
					}

					const Vector3 axis = Vector3::Normalize(rawAxis);
					const float obstacleRadius =
						std::fabs(Vector3::Dot(obstacle.orientations[0] * obstacle.size.x, axis)) +
						std::fabs(Vector3::Dot(obstacle.orientations[1] * obstacle.size.y, axis)) +
						std::fabs(Vector3::Dot(obstacle.orientations[2] * obstacle.size.z, axis));
					const float playerRadius = std::fabs(axis.x) * settings.half.x + std::fabs(axis.z) * settings.half.z;
					const float distance = Vector3::Dot(fixedCenter - obstacle.center, axis);
					const float penetration = obstacleRadius + playerRadius - std::fabs(distance);
					if (penetration <= 0.0f)
					{
						overlaps = false;
						break;
					}
					if (penetration < minimumPenetration)
					{
						minimumPenetration = penetration;
						minimumAxis = axis;
						minimumDistance = distance;
					}
				}

				if (!overlaps || minimumPenetration == FLT_MAX)
				{
					continue;
				}

				const float direction = minimumDistance >= 0.0f ? 1.0f : -1.0f;
				// OBBとの最小XZ侵入量だけを戻し、床のY解決は既存AABB処理へ残す。
				fixedCenter += minimumAxis * ((minimumPenetration + settings.eps) * direction);
				if (!result.groundedByStageTop)
				{
					result.lastHitType = StageHitType::Wall;
					result.lastHitShape = StageHitShape::OBB;
					result.lastCorrectionAxis = StageCorrectionAxis::OBBNormal;
				}
				++hitCount;
			}

			return hitCount;
		}
	}

	WorldCollisionResult WorldCollisionResolver::Resolve(
		const std::vector<AABB>& worldAABBs,
		const WorldCollisionSettings& s,
		const Vector3& oldTranslate,
		const Vector3& newTranslate,
		bool useGrounded,
		float* inoutJumpVelocity,
		const std::vector<AABB>* obstacleBroadPhaseAABBs,
		const std::vector<OBB>* obstacleOBBs,
		const std::vector<uint8_t>* obstacleWalkable)
	{
		WorldCollisionResult r{};

		// StageCollision専用: 汎用CollisionManagerのイベント通知ではなく、移動後座標を静的AABBから解決する。

		// old/new の中心（物理中心）
		Vector3 oldCenter = oldTranslate - s.centerOffset;
		Vector3 newCenter = newTranslate - s.centerOffset;

		auto buildMovingBodyAABB = [&](const Vector3& c) { return AABB{ c - s.half, c + s.half }; };

		Vector3 fixedCenter = oldCenter;
		const VerticalContactCandidate priorityVerticalContact = FindPriorityVerticalContact(
			worldAABBs,
			obstacleBroadPhaseAABBs,
			obstacleOBBs,
			obstacleWalkable,
			s,
			oldCenter,
			newCenter);

		auto resolveAxis = [&](int axis, float delta)
			{
				if (delta == 0.0f) return;

				if (axis == 0) fixedCenter.x += delta;
				if (axis == 1) fixedCenter.y += delta;
				if (axis == 2) fixedCenter.z += delta;

				AABB p = buildMovingBodyAABB(fixedCenter);

				bool hit = false;
				float bestFix = 0.0f;
				float bestDist = FLT_MAX;

				for (size_t worldAABBIndex = 0; worldAABBIndex < worldAABBs.size(); ++worldAABBIndex)
				{
					const AABB& w = worldAABBs[worldAABBIndex];
					if (priorityVerticalContact.valid &&
						priorityVerticalContact.hitShape == StageHitShape::AABB &&
						priorityVerticalContact.shapeIndex == worldAABBIndex)
					{
						continue;
					}
					if (IsObstacleBroadPhaseAABB(w, obstacleBroadPhaseAABBs))
					{
						continue;
					}
					// 移動体AABBと静的ステージAABBの重なりだけを見る。ObjectChannel/Responseはここでは扱わない。
					if (!(p.min.x <= w.max.x && p.max.x >= w.min.x &&
						p.min.y <= w.max.y && p.max.y >= w.min.y &&
						p.min.z <= w.max.z && p.max.z >= w.min.z))
					{
						continue;
					}

					float cand = 0.0f;
					bool valid = false;

					if (axis == 0)
					{
						if (oldCenter.x + s.half.x <= w.min.x) { cand = (w.min.x - s.half.x) - s.eps; valid = true; }
						else if (oldCenter.x - s.half.x >= w.max.x) { cand = (w.max.x + s.half.x) + s.eps; valid = true; }
						else
						{
							float dMin = std::fabs((w.min.x - s.half.x) - oldCenter.x);
							float dMax = std::fabs((w.max.x + s.half.x) - oldCenter.x);
							cand = (dMin <= dMax) ? (w.min.x - s.half.x - s.eps) : (w.max.x + s.half.x + s.eps);
							valid = true;
						}

						if (valid)
						{
							float dist = std::fabs(cand - fixedCenter.x);
							if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
						}
					}
					else if (axis == 2)
					{
						if (oldCenter.z + s.half.z <= w.min.z) { cand = (w.min.z - s.half.z) - s.eps; valid = true; }
						else if (oldCenter.z - s.half.z >= w.max.z) { cand = (w.max.z + s.half.z) + s.eps; valid = true; }
						else
						{
							float dMin = std::fabs((w.min.z - s.half.z) - oldCenter.z);
							float dMax = std::fabs((w.max.z + s.half.z) - oldCenter.z);
							cand = (dMin <= dMax) ? (w.min.z - s.half.z - s.eps) : (w.max.z + s.half.z + s.eps);
							valid = true;
						}

						if (valid)
						{
							float dist = std::fabs(cand - fixedCenter.z);
							if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
						}
					}
					else // axis == 1
					{
						if (oldCenter.y - s.half.y >= w.max.y) { cand = (w.max.y + s.half.y) + s.eps; valid = true; }     // 床
						else if (oldCenter.y + s.half.y <= w.min.y) { cand = (w.min.y - s.half.y) - s.eps; valid = true; } // 天井
						else
						{
							float dFloor = std::fabs((w.max.y + s.half.y) - oldCenter.y);
							float dCeil = std::fabs((w.min.y - s.half.y) - oldCenter.y);
							cand = (dFloor <= dCeil) ? (w.max.y + s.half.y + s.eps) : (w.min.y - s.half.y - s.eps);
							valid = true;
						}

						if (valid)
						{
							float dist = std::fabs(cand - fixedCenter.y);
							if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
						}
					}
				}

				if (hit)
				{
					// 押し戻しは軸ごとに最も近い補正位置だけを採用し、既存の移動安定性を保つ。
					if (axis == 0)
					{
						fixedCenter.x = bestFix;
						r.lastHitType = StageHitType::Wall;
						r.lastHitShape = StageHitShape::AABB;
						r.lastCorrectionAxis = StageCorrectionAxis::X;
					}
					if (axis == 2)
					{
						fixedCenter.z = bestFix;
						r.lastHitType = StageHitType::Wall;
						r.lastHitShape = StageHitShape::AABB;
						r.lastCorrectionAxis = StageCorrectionAxis::Z;
					}
					if (axis == 1)
					{
						fixedCenter.y = bestFix;
						r.lastHitType = delta < 0.0f ? StageHitType::Floor : StageHitType::Ceiling;
						r.lastHitShape = StageHitShape::AABB;
						r.lastCorrectionAxis = StageCorrectionAxis::Y;

						// Player向け：床に落ちたら grounded / 上向き衝突なら上向き速度を0
						if (useGrounded && inoutJumpVelocity)
						{
							if (delta < 0.0f) { r.grounded = true; *inoutJumpVelocity = 0.0f; }
							else if (*inoutJumpVelocity > 0.0f) { *inoutJumpVelocity = 0.0f; }
						}
					}
				}
			};

		// Boss/Enemyと同じ順でもOK（Playerに合わせて X,Z,Y の順が安定）
		resolveAxis(0, newCenter.x - oldCenter.x);
		resolveAxis(2, newCenter.z - oldCenter.z);
		// Obstacle系だけは包み込みAABBではなく回転OBBで最終的なXZ押し戻しを行う。
		r.obbHitCount = ResolveObstacleOBBsXZ(
			obstacleBroadPhaseAABBs,
			obstacleOBBs,
			s,
			newCenter.y,
			fixedCenter,
			priorityVerticalContact.shapeIndex,
			priorityVerticalContact.valid && priorityVerticalContact.hitShape == StageHitShape::OBB,
			r);

		if (priorityVerticalContact.valid)
		{
			// 上面・下面を跨いだ形状はY補正を優先し、そのフレームの同一形状からの横反発を発生させない。
			fixedCenter.y = priorityVerticalContact.correctedCenterY;
			r.lastHitType = priorityVerticalContact.hitType;
			r.lastHitShape = priorityVerticalContact.hitShape;
			r.lastCorrectionAxis = StageCorrectionAxis::Y;
			r.groundedByStageTop = priorityVerticalContact.hitType == StageHitType::Floor;
			if (priorityVerticalContact.hitType == StageHitType::Floor)
			{
				r.grounded = true;
				if (useGrounded && inoutJumpVelocity)
				{
					*inoutJumpVelocity = 0.0f;
				}
			}
			else if (priorityVerticalContact.hitType == StageHitType::Ceiling && inoutJumpVelocity && *inoutJumpVelocity > 0.0f)
			{
				*inoutJumpVelocity = 0.0f;
			}
		}
		else
		{
			resolveAxis(1, newCenter.y - oldCenter.y);
		}

		r.fixedCenter = fixedCenter;
		return r;
	}

} // namespace Ken4lowEngine
