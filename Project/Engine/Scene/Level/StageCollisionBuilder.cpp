#include "StageCollisionBuilder.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include "Matrix4x4.h"
#include "MineHiddenArenaLayout.h"
#include "CollapsedCityLayout.h"
#include "CollapsedCityPassageLayout.h"
#include "CentralControlColosseumLayout.h"
#include <algorithm>
#include <cctype>
#include <limits>

namespace Ken4lowEngine
{
	namespace
	{
		AABB BuildAABBFromRotatedOBB(const Vector3& center, const Vector3& half, const Vector3& rotationRad)
		{
			// StageCollision専用: 回転BOXを移動解決向けAABBへ保守的に変換する。
			const Matrix4x4 rotation = Matrix4x4::MakeRotateMatrix(rotationRad);
			AABB aabb{};
			aabb.min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
			aabb.max = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };

			for (int ix = -1; ix <= 1; ix += 2)
			{
				for (int iy = -1; iy <= 1; iy += 2)
				{
					for (int iz = -1; iz <= 1; iz += 2)
					{
						Vector3 cornerLocal = {
							half.x * static_cast<float>(ix),
							half.y * static_cast<float>(iy),
							half.z * static_cast<float>(iz),
						};
						const Vector3 cornerWorld = Vector3::Transform(cornerLocal, rotation) + center;
						aabb.min.x = (std::min)(aabb.min.x, cornerWorld.x);
						aabb.min.y = (std::min)(aabb.min.y, cornerWorld.y);
						aabb.min.z = (std::min)(aabb.min.z, cornerWorld.z);
						aabb.max.x = (std::max)(aabb.max.x, cornerWorld.x);
						aabb.max.y = (std::max)(aabb.max.y, cornerWorld.y);
						aabb.max.z = (std::max)(aabb.max.z, cornerWorld.z);
					}
				}
			}
			return aabb;
		}

		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		bool ContainsLadderName(const std::string& name)
		{
			return ToLower(name).find("ladder") != std::string::npos;
		}

		std::string ResolveCollisionType(const ObjectData& data)
		{
			if (!data.collider.collisionType.empty()) return data.collider.collisionType;
			const std::string loweredName = ToLower(data.name);
			if (loweredName.find("ladder") != std::string::npos) return "Ladder";
			if (loweredName.find("floor") != std::string::npos ||
				loweredName.find("ground") != std::string::npos ||
				loweredName.find("catwalk") != std::string::npos ||
				loweredName.find("platform") != std::string::npos ||
				loweredName.find("bridge") != std::string::npos ||
				loweredName.find("road") != std::string::npos ||
				loweredName.find("path") != std::string::npos)
			{
				return "Floor";
			}
			return "Obstacle"; // collision_type未出力のStage3 BOXもA*と自動乗越へ接続する。
		}

		bool IsWalkableObstacleSurface(const ObjectData& data)
		{
			const std::string loweredName = ToLower(data.name);
			const bool lowCover = loweredName.find("cover") != std::string::npos ||
				loweredName.find("rubble") != std::string::npos ||
				loweredName.find("brokenbeam") != std::string::npos ||
				loweredName.find("crate") != std::string::npos ||
				loweredName.find("box") != std::string::npos ||
				loweredName.find("block") != std::string::npos ||
				loweredName.find("prop") != std::string::npos ||
				loweredName.find("barricade") != std::string::npos;
			const bool structural = loweredName.find("wall") != std::string::npos ||
				loweredName.find("gate") != std::string::npos ||
				loweredName.find("pillar") != std::string::npos ||
				loweredName.find("ceiling") != std::string::npos ||
				loweredName.find("rockdetail") != std::string::npos ||
				loweredName.find("support") != std::string::npos ||
				loweredName.find("container") != std::string::npos ||
				loweredName.find("dome") != std::string::npos ||
				loweredName.find("colosseum") != std::string::npos ||
				(loweredName.find("beam") != std::string::npos && !lowCover);
			return lowCover && !structural; // 箱・Prop・低いCoverは登れ、外壁やContainerは必ず迂回させる。
		}
	}

	StageCollisionBuildResult StageCollisionBuilder::Build(const LevelData& levelData, const Vector3& offset)
	{
		StageCollisionBuildResult result{};
		const std::vector<ObjectData> hiddenArenaObjects = MineHiddenArenaLayout::Build(levelData);
		const std::vector<ObjectData> collapsedCityObjects = CollapsedCityLayout::Build(levelData);
		const std::vector<ObjectData> collapsedCityPassageObjects = CollapsedCityPassageLayout::Build(levelData);
		const std::vector<ObjectData> centralControlObjects = CentralControlColosseumLayout::Build(levelData);
		const bool hasHiddenArena = !hiddenArenaObjects.empty();
		const size_t estimatedCount = levelData.objects.size() + hiddenArenaObjects.size() + collapsedCityObjects.size() + collapsedCityPassageObjects.size() + centralControlObjects.size();
		result.worldAABBs.reserve(estimatedCount);
		result.floorAABBs.reserve(estimatedCount);
		result.wallObstacleAABBs.reserve(estimatedCount);
		result.navigationObstacleAABBs.reserve(estimatedCount);
		result.worldColliders.reserve(estimatedCount);
		result.wallObstacleOBBs.reserve(estimatedCount);
		result.wallObstacleWalkable.reserve(estimatedCount);
		result.navigationObstacleOBBs.reserve(estimatedCount);
		result.ladderColliders.reserve(estimatedCount);
		result.ladderAABBs.reserve(estimatedCount);
		result.ladderOBBs.reserve(estimatedCount);

		auto appendCollider = [&](const ObjectData& data)
		{
			if (!data.collider.enabled || data.collider.type != "BOX") return;

			const Vector3 centerW = {
				data.position.x + data.collider.center.x * data.scale.x + offset.x,
				data.position.y + data.collider.center.y * data.scale.y + offset.y,
				data.position.z + data.collider.center.z * data.scale.z + offset.z
			};
			const Vector3 halfW = {
				0.5f * data.collider.size.x * data.scale.x,
				0.5f * data.collider.size.y * data.scale.y,
				0.5f * data.collider.size.z * data.scale.z,
			};

			auto collider = std::make_unique<Collider>();
			ApplyCollisionPreset(*collider, ECollisionPresetId::WorldStatic);
			collider->SetCenterPosition(centerW);
			collider->SetOBBHalfSize(halfW);
			const Vector3 colliderRotation = {
				data.rotation.x + data.collider.rotation.x,
				data.rotation.y + data.collider.rotation.y,
				data.rotation.z + data.collider.rotation.z,
			};
			collider->SetOrientation(colliderRotation);
			collider->SetDebugName(data.name);
			const AABB aabb = BuildAABBFromRotatedOBB(centerW, halfW, colliderRotation);
			const OBB colliderObb = collider->GetOBB();
			const std::string collisionType = ResolveCollisionType(data);
			const bool isLadder = collisionType == "Ladder" || ContainsLadderName(data.name);
			if (isLadder)
			{
				collider->SetTrigger(true);
				collider->SetCollisionTag("Ladder");
				collider->SetCollisionLayer(kLadderPhysicsLayer);
				result.ladderColliders.push_back(collider.get());
				result.ladderAABBs.push_back(aabb);
				result.ladderOBBs.push_back(colliderObb);
			}
			else if (collisionType == "Floor")
			{
				result.worldAABBs.push_back(aabb);
				result.floorAABBs.push_back(aabb);
			}
			else if (collisionType == "Obstacle" || collisionType == "Pillar" || collisionType == "Fence" || collisionType == "Tree")
			{
				const bool walkableSurface = IsWalkableObstacleSurface(data);
				const float obstacleHeight = std::max(0.0f, aabb.max.y - aabb.min.y);
				AABB navigationAabb = aabb;
				if (!walkableSurface) navigationAabb.max.y = std::max(navigationAabb.max.y, navigationAabb.min.y + 8.0f);

				result.worldAABBs.push_back(aabb);
				result.wallObstacleAABBs.push_back(aabb);
				result.navigationObstacleAABBs.push_back(navigationAabb);
				result.wallObstacleOBBs.push_back(colliderObb);
				result.wallObstacleWalkable.push_back(walkableSurface ? 1u : 0u);
				result.navigationObstacleOBBs.push_back(colliderObb);
				if (walkableSurface && obstacleHeight <= 4.5f) result.floorAABBs.push_back(aabb); // 登った後の上面をA*の床支持として扱い、頂上で停止しないようにする。
			}
			else
			{
				result.worldAABBs.push_back(aabb);
			}

			result.worldColliders.push_back(std::move(collider));
		};

		for (const ObjectData& data : levelData.objects)
		{
			if (hasHiddenArena && data.name == "Wall_North") continue; // 描画側と同じ旧終端壁を除き、隠し通路に見えない壁を残さない。
			appendCollider(data);
		}
		for (const ObjectData& data : hiddenArenaObjects) appendCollider(data);
		for (const ObjectData& data : collapsedCityObjects) appendCollider(data); // 描画と同じ生成物へColliderを付け、崩落路面と高架を実際に歩けるようにする。
		for (const ObjectData& data : collapsedCityPassageObjects) appendCollider(data); // 仮設橋の各床面へColliderを付け、陥没区画を確実に通過できるようにする。
		for (const ObjectData& data : centralControlObjects) appendCollider(data); // 円形床と外周壁を描画配置と同じデータから生成し、BossがArena外へ抜けないようにする。

		return result;
	}

	std::vector<Collider*> StageCollisionBuilder::GetColliders(const StageCollisionBuildResult& result)
	{
		std::vector<Collider*> colliders{};
		colliders.reserve(result.worldColliders.size());
		for (const std::unique_ptr<Collider>& collider : result.worldColliders)
		{
			if (collider) colliders.push_back(collider.get());
		}
		return colliders;
	}
} // namespace Ken4lowEngine
