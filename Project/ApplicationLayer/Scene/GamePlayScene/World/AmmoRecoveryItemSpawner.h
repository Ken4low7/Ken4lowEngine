#pragma once

#include "AABB.h"
#include "Vector3.h"

#include <cstddef>
#include <vector>

class IPlayerRuntime;
class ItemManager;
namespace Ken4lowEngine
{
	class Stage;
}

namespace K4E = ::Ken4lowEngine;

/// 弾薬回復アイテムの時間スポーンを管理するクラス。
class AmmoRecoveryItemSpawner
{
public:
	struct Settings
	{
		float spawnIntervalSec = 15.0f;
		int maxActiveCount = 3;
		int recoveryAmountOverride = 0;
		float minDistanceFromPlayer = 12.0f;
		float spawnHeightOffset = 0.15f;
	};

	void Initialize();
	void Update(float deltaTime, IPlayerRuntime* player, ItemManager& itemManager, const K4E::Stage* stage, bool suppressNewSpawn);
	void DrawImGui();
	void Reset();

private:
	bool ShouldSpawnForPlayer(const IPlayerRuntime& player) const;
	int ResolveRecoveryAmount(const IPlayerRuntime& player) const;
	bool TryFindSpawnPosition(const IPlayerRuntime& player, const K4E::Stage* stage, K4E::Vector3& outPosition);
	bool TryFindFloorSpawnPosition(const K4E::Vector3& playerPosition, const std::vector<K4E::AABB>& floorAABBs, K4E::Vector3& outPosition);
	K4E::Vector3 MakeFallbackSpawnPosition(const K4E::Vector3& playerPosition) const;

private:
	Settings settings_{};
	float spawnTimerSec_ = 0.0f;
	std::size_t nextFloorIndex_ = 0;
	K4E::Vector3 lastSpawnPosition_{};
	bool lastSpawnSucceeded_ = false;
	bool lastSuppressed_ = false;
};
