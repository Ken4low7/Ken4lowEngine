#define NOMINMAX
#include "AmmoRecoveryItemSpawner.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ItemManager.h"
#include "Stage.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace
{
	float DistanceSqXZ(const K4E::Vector3& a, const K4E::Vector3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return dx * dx + dz * dz;
	}

	bool IsUsableFloor(const K4E::AABB& floor)
	{
		const float sizeX = floor.max.x - floor.min.x;
		const float sizeZ = floor.max.z - floor.min.z;
		return sizeX >= 2.0f && sizeZ >= 2.0f;
	}
}

void AmmoRecoveryItemSpawner::Initialize()
{
	Reset();
}

void AmmoRecoveryItemSpawner::Reset()
{
	spawnTimerSec_ = 0.0f;
	nextFloorIndex_ = 0;
	lastSpawnPosition_ = {};
	lastSpawnSucceeded_ = false;
	lastSuppressed_ = false;
}

void AmmoRecoveryItemSpawner::Update(float deltaTime, IPlayerRuntime* player, ItemManager& itemManager, const K4E::Stage* stage, bool suppressNewSpawn)
{
	lastSuppressed_ = suppressNewSpawn;
	if (suppressNewSpawn || !player) return;

	if (!ShouldSpawnForPlayer(*player))
	{
		spawnTimerSec_ = 0.0f;
		return;
	}

	const int maxActiveCount = std::max(0, settings_.maxActiveCount);
	if (itemManager.GetActiveItemCount(ItemType::AmmoSmall) >= maxActiveCount) return;

	spawnTimerSec_ += deltaTime;
	if (spawnTimerSec_ < std::max(0.1f, settings_.spawnIntervalSec)) return;

	K4E::Vector3 spawnPosition{};
	if (!TryFindSpawnPosition(*player, stage, spawnPosition)) return;

	const int recoveryAmount = ResolveRecoveryAmount(*player);
	if (recoveryAmount <= 0) return;

	itemManager.SpawnAmmoSmall(spawnPosition, recoveryAmount); // 生成管理はItemManagerへ委譲し、Runtime依存をスポーン判断だけに限定する。
	lastSpawnPosition_ = spawnPosition;
	lastSpawnSucceeded_ = true;
	spawnTimerSec_ = 0.0f;
}

bool AmmoRecoveryItemSpawner::ShouldSpawnForPlayer(const IPlayerRuntime& player) const
{
	const int maxReserveAmmo = player.GetMaxReserveAmmo();
	return maxReserveAmmo > 0 && player.GetReserveAmmo() < maxReserveAmmo;
}

int AmmoRecoveryItemSpawner::ResolveRecoveryAmount(const IPlayerRuntime& player) const
{
	if (settings_.recoveryAmountOverride > 0) return settings_.recoveryAmountOverride;
	return std::max(1, player.GetMagazineCapacity());
}

bool AmmoRecoveryItemSpawner::TryFindSpawnPosition(const IPlayerRuntime& player, const K4E::Stage* stage, K4E::Vector3& outPosition)
{
	const K4E::Vector3 playerPosition = player.GetWorldPosition();
	if (stage && TryFindFloorSpawnPosition(playerPosition, stage->GetFloorAABBs(), outPosition)) return true;
	outPosition = MakeFallbackSpawnPosition(playerPosition);
	return true;
}

bool AmmoRecoveryItemSpawner::TryFindFloorSpawnPosition(const K4E::Vector3& playerPosition, const std::vector<K4E::AABB>& floorAABBs, K4E::Vector3& outPosition)
{
	if (floorAABBs.empty()) return false;

	const float minDistanceSq = settings_.minDistanceFromPlayer * settings_.minDistanceFromPlayer;
	float farthestDistanceSq = -1.0f;
	K4E::Vector3 farthestPosition{};
	bool hasFallback = false;

	for (std::size_t i = 0; i < floorAABBs.size(); ++i)
	{
		const std::size_t index = (nextFloorIndex_ + i) % floorAABBs.size();
		const K4E::AABB& floor = floorAABBs[index];
		if (!IsUsableFloor(floor)) continue;

		K4E::Vector3 candidate{
			(floor.min.x + floor.max.x) * 0.5f,
			floor.max.y + settings_.spawnHeightOffset,
			(floor.min.z + floor.max.z) * 0.5f
		};

		const float distanceSq = DistanceSqXZ(candidate, playerPosition);
		if (distanceSq > farthestDistanceSq)
		{
			farthestDistanceSq = distanceSq;
			farthestPosition = candidate;
			hasFallback = true;
		}
		if (distanceSq >= minDistanceSq)
		{
			outPosition = candidate;
			nextFloorIndex_ = (index + 1) % floorAABBs.size();
			return true;
		}
	}

	if (hasFallback)
	{
		outPosition = farthestPosition;
		return true;
	}
	return false;
}

K4E::Vector3 AmmoRecoveryItemSpawner::MakeFallbackSpawnPosition(const K4E::Vector3& playerPosition) const
{
	return {
		playerPosition.x + settings_.minDistanceFromPlayer,
		playerPosition.y,
		playerPosition.z + settings_.minDistanceFromPlayer
	};
}

void AmmoRecoveryItemSpawner::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Ammo Recovery Spawner", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::DragFloat("Spawn Interval Sec", &settings_.spawnIntervalSec, 0.1f, 0.1f, 120.0f, "%.1f");
	ImGui::DragInt("Max Active Ammo Items", &settings_.maxActiveCount, 1.0f, 0, 20);
	ImGui::DragInt("Recovery Override(0=Magazine)", &settings_.recoveryAmountOverride, 1.0f, 0, 999);
	ImGui::DragFloat("Min Distance From Player", &settings_.minDistanceFromPlayer, 0.5f, 0.0f, 80.0f, "%.1f");
	ImGui::Text("Timer: %.2f / %.2f", spawnTimerSec_, settings_.spawnIntervalSec);
	ImGui::Text("Last Suppressed: %s", lastSuppressed_ ? "true" : "false");
	ImGui::Text("Last Spawn: %s (%.2f, %.2f, %.2f)",
		lastSpawnSucceeded_ ? "true" : "false",
		lastSpawnPosition_.x,
		lastSpawnPosition_.y,
		lastSpawnPosition_.z);
#else
	(void)this;
#endif
}
