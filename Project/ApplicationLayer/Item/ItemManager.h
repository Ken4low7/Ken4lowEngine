#pragma once
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "Item.h"
#include "ItemVisualEffect.h"

namespace K4E = ::Ken4lowEngine;

class IPlayerRuntime;
class CollisionManager;

/// Item実体の所有、取得判定、Player Runtimeへの効果適用を管理する。
class ItemManager
{
private:
	struct ItemEffectDebugInfo
	{
		ItemType itemType = ItemType::None;
		bool pickupDetected = false;
		bool effectApplied = false;
		bool noEffectBecauseFull = false;
		std::string failReason = "未取得";
		float hpBefore = 0.0f;
		float hpAfter = 0.0f;
		float maxHp = 0.0f;
		int reserveBefore = 0;
		int reserveAfter = 0;
		int maxReserve = 0;
		int magazineAmmo = 0;
		int hudMagazineAmmo = 0;
		int hudReserveAmmo = 0;
		std::string currentWeaponName = "未取得";
		bool currentWeaponAmmoRecoverable = false;
	};

public:
	void Initialize();
	void Update(IPlayerRuntime* player);
	void Draw();
	void RegisterColliders(CollisionManager* collisionManager);
	void Spawn(ItemType type, const K4E::Vector3& position);
	void SpawnHealSmall(const K4E::Vector3& position);
	void SpawnAmmoSmall(const K4E::Vector3& position);
	void SpawnAmmoSmall(const K4E::Vector3& position, int ammoAmount);
	bool TryGetFirstActiveItemPosition(ItemType type, K4E::Vector3& outPosition) const;
	void TryDropFromEnemyDeath(const K4E::Vector3& deathPosition);
	void TryDropEnemyItem(const K4E::Vector3& deathPosition);
	ItemType RollEnemyDrop();
	void CheckPickup(IPlayerRuntime& player);
	bool ApplyItemEffect(Item& item, IPlayerRuntime& player);
	void Clear();
	bool ConsumeCollected(ItemType type);
	int GetActiveItemCount() const;
	int GetActiveItemCount(ItemType type) const;
	void DrawImGui();

	void SetConsumeItemWhenFull(bool enabled) { consumeItemWhenFull_ = enabled; }
	void SetEnemyDeathDropEnabled(bool enabled) { enemyDeathDropEnabled_ = enabled; }
	float GetHealDropChance() const { return healDropChance_; }
	float GetAmmoDropChance() const { return ammoDropChance_; }
	float GetNoneDropChance() const;
	int GetHealAmount() const { return healAmount_; }
	int GetAmmoAmount() const { return ammoAmount_; }
	float GetPickupRadius() const { return pickupRadius_; }
	bool IsEnemyDeathDropEnabled() const { return enemyDeathDropEnabled_; }
	bool IsForceEnemyDeathDropEnabled() const { return forceEnemyDeathDrop_; }
	ItemType GetLastPickedItemType() const { return lastPickedItemType_; }
	ItemType GetLastDroppedItemType() const { return lastDroppedItemType_; }
	const K4E::Vector3& GetLastDropPosition() const { return lastDropPosition_; }
	const ItemEffectDebugInfo& GetLastItemEffectDebugInfo() const { return lastItemEffectDebugInfo_; }

private:
	void RemoveInactiveItems();
	void SpawnDropItem(ItemType type, const K4E::Vector3& position);
	void SpawnConfigured(ItemType type, const K4E::Vector3& position, int overrideAmmoAmount = -1);
	void LogDropRollResult(ItemType type, const K4E::Vector3& position) const;
	void LogItemEffectResult() const;
	void ApplyVisualSettings(Item& item);

private:
	std::vector<std::unique_ptr<Item>> items_;
	std::vector<ItemType> collectedEvents_;
	ItemEffectDebugInfo lastItemEffectDebugInfo_{};
	CollisionManager* registeredCollisionManager_ = nullptr;
	ItemVisualEffect itemVisualEffect_;
	ItemType lastPickedItemType_ = ItemType::None;
	ItemType lastDroppedItemType_ = ItemType::None;
	K4E::Vector3 lastDropPosition_ = {};
	float healDropChance_ = 0.25f;
	float ammoDropChance_ = 0.35f;
	int healAmount_ = 25;
	int ammoAmount_ = 30;
	float pickupRadius_ = 2.0f;
	int lastKnownMagazineAmmo_ = 0;
	int lastKnownReserveAmmo_ = 0;
	int lastKnownMaxReserveAmmo_ = 0;
	int lastAmmoSmallReserveRestored_ = 0;
	bool consumeItemWhenFull_ = false;
	bool enemyDeathDropEnabled_ = true;
	bool forceEnemyDeathDrop_ = false;
	std::mt19937 rng_{ std::random_device{}() };
};
