#define NOMINMAX
#include "ItemManager.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "CollisionManager.h"
#include "Stage.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <Windows.h>

namespace K4E = ::Ken4lowEngine;

namespace
{
	constexpr float kDropGroundClearance = 0.45f;
	constexpr float kFloorHorizontalTolerance = 0.05f;
	constexpr float kMaximumFloorAboveDeath = 0.5f;

	const char* ToItemTypeName(ItemType type)
	{
		switch (type)
		{
		case ItemType::HealSmall: return "HealSmall";
		case ItemType::AmmoSmall: return "AmmoSmall";
		case ItemType::NextStageKey: return "NextStageKey";
		case ItemType::None:
		default: return "None";
		}
	}

	K4E::Vector3 ResolveEnemyDropPosition(const K4E::Vector3& deathPosition)
	{
		K4E::Stage* stage = K4E::Stage::GetActiveRuntimeStage();
		if (!stage) return deathPosition;

		float highestFloorY = -std::numeric_limits<float>::infinity();
		for (const K4E::AABB& floor : stage->GetFloorAABBs())
		{
			const bool containsXZ = deathPosition.x >= floor.min.x - kFloorHorizontalTolerance &&
				deathPosition.x <= floor.max.x + kFloorHorizontalTolerance &&
				deathPosition.z >= floor.min.z - kFloorHorizontalTolerance &&
				deathPosition.z <= floor.max.z + kFloorHorizontalTolerance;
			if (!containsXZ || floor.max.y > deathPosition.y + kMaximumFloorAboveDeath) continue;
			highestFloorY = std::max(highestFloorY, floor.max.y);
		}

		if (!std::isfinite(highestFloorY)) return deathPosition;
		K4E::Vector3 groundedPosition = deathPosition;
		groundedPosition.y = highestFloorY + kDropGroundClearance; // 敵の中心位置ではなく足元の床面を基準にItemを生成する。
		return groundedPosition;
	}
}

void ItemManager::Initialize() { Clear(); }

void ItemManager::Update(IPlayerRuntime* player)
{
	for (auto& item : items_)
	{
		if (!item) continue;
		ApplyVisualSettings(*item);
		item->Update();
		if (item->IsActive()) itemVisualEffect_.UpdateIdle(*item);
	}
	RemoveInactiveItems();
	if (player) CheckPickup(*player);
}

void ItemManager::Draw()
{
	for (auto& item : items_)
	{
		if (item) item->Draw();
	}
}

void ItemManager::RegisterColliders(CollisionManager* collisionManager)
{
	if (!collisionManager) return;
	if (registeredCollisionManager_)
	{
		for (auto& item : items_)
		{
			if (item) registeredCollisionManager_->RemoveCollider(item.get());
		}
	}
	registeredCollisionManager_ = collisionManager;
	for (auto& item : items_)
	{
		if (item && item->IsActive()) registeredCollisionManager_->AddCollider(item.get());
	}
}

void ItemManager::Spawn(ItemType type, const K4E::Vector3& position)
{
	SpawnConfigured(type, position);
}

void ItemManager::SpawnHealSmall(const K4E::Vector3& position)
{
	SpawnConfigured(ItemType::HealSmall, position);
}

void ItemManager::SpawnAmmoSmall(const K4E::Vector3& position)
{
	SpawnConfigured(ItemType::AmmoSmall, position);
}

void ItemManager::SpawnAmmoSmall(const K4E::Vector3& position, int ammoAmount)
{
	SpawnConfigured(ItemType::AmmoSmall, position, ammoAmount);
}

bool ItemManager::TryGetFirstActiveItemPosition(ItemType type, K4E::Vector3& outPosition) const
{
	for (const auto& item : items_)
	{
		if (item && item->IsActive() && item->GetType() == type)
		{
			outPosition = item->GetPosition();
			return true;
		}
	}
	return false;
}

void ItemManager::TryDropFromEnemyDeath(const K4E::Vector3& deathPosition)
{
	TryDropEnemyItem(deathPosition);
}

void ItemManager::TryDropEnemyItem(const K4E::Vector3& deathPosition)
{
	const K4E::Vector3 dropPosition = ResolveEnemyDropPosition(deathPosition);
	lastDropPosition_ = dropPosition;
	if (!enemyDeathDropEnabled_)
	{
		lastDroppedItemType_ = ItemType::None;
		LogDropRollResult(lastDroppedItemType_, dropPosition);
		return;
	}
	lastDroppedItemType_ = RollEnemyDrop();
	if (lastDroppedItemType_ != ItemType::None) SpawnDropItem(lastDroppedItemType_, dropPosition);
	LogDropRollResult(lastDroppedItemType_, dropPosition);
}

ItemType ItemManager::RollEnemyDrop()
{
	const float healRate = std::max(0.0f, healDropChance_);
	const float ammoRate = std::max(0.0f, ammoDropChance_);
	const float totalDropRate = healRate + ammoRate;
	if (totalDropRate <= 0.0f) return forceEnemyDeathDrop_ ? ItemType::HealSmall : ItemType::None;

	float effectiveHealRate = healRate;
	float effectiveAmmoRate = ammoRate;
	if (forceEnemyDeathDrop_ || totalDropRate > 1.0f)
	{
		effectiveHealRate = healRate / totalDropRate;
		effectiveAmmoRate = ammoRate / totalDropRate;
	}

	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	const float roll = dist(rng_);
	if (roll < effectiveHealRate) return ItemType::HealSmall;
	if (roll < effectiveHealRate + effectiveAmmoRate) return ItemType::AmmoSmall;
	return ItemType::None;
}

void ItemManager::CheckPickup(IPlayerRuntime& player)
{
	const K4E::Vector3 playerPos = player.GetWorldPosition();
	for (auto& item : items_)
	{
		if (!item || !item->IsActive()) continue;
		if (!item->CheckCollisionWithPlayer(playerPos)) continue;

		lastItemEffectDebugInfo_ = {};
		lastItemEffectDebugInfo_.pickupDetected = true;
		lastItemEffectDebugInfo_.itemType = item->GetType();
		lastPickedItemType_ = item->GetType();
		const bool effectApplied = ApplyItemEffect(*item, player);
		const bool consumePickedItem = effectApplied || (consumeItemWhenFull_ && lastItemEffectDebugInfo_.noEffectBecauseFull);
		if (consumePickedItem)
		{
			itemVisualEffect_.PlayPickup(lastItemEffectDebugInfo_.itemType, item->GetVisualPosition());
			itemVisualEffect_.StopIdle(*item);
			item->MarkCollected();
			collectedEvents_.push_back(lastItemEffectDebugInfo_.itemType);
		}

		lastKnownMagazineAmmo_ = player.GetMagazineAmmo();
		lastKnownReserveAmmo_ = player.GetReserveAmmo();
		lastKnownMaxReserveAmmo_ = player.GetMaxReserveAmmo();
		lastAmmoSmallReserveRestored_ = (lastItemEffectDebugInfo_.itemType == ItemType::AmmoSmall)
			? std::max(0, lastItemEffectDebugInfo_.reserveAfter - lastItemEffectDebugInfo_.reserveBefore) : 0;
		LogItemEffectResult();
	}
	RemoveInactiveItems();
}

bool ItemManager::ApplyItemEffect(Item& item, IPlayerRuntime& player)
{
	ItemEffectDebugInfo& info = lastItemEffectDebugInfo_;
	info.itemType = item.GetType();
	info.effectApplied = false;
	info.failReason.clear();
	info.noEffectBecauseFull = false;
	info.hpBefore = player.GetHP();
	info.hpAfter = info.hpBefore;
	info.maxHp = player.GetMaxHP();
	info.reserveBefore = player.GetReserveAmmo();
	info.reserveAfter = info.reserveBefore;
	info.maxReserve = player.GetMaxReserveAmmo();
	info.magazineAmmo = player.GetMagazineAmmo();
	info.currentWeaponName = "PlayerActor Primary";
	info.currentWeaponAmmoRecoverable = info.maxReserve > 0;
	info.hudMagazineAmmo = info.magazineAmmo;
	info.hudReserveAmmo = info.reserveBefore;

	switch (item.GetType())
	{
	case ItemType::HealSmall:
		if (item.GetHealAmount() <= 0) info.failReason = "HealSmall回復量が0以下です";
		else if (info.hpBefore >= info.maxHp)
		{
			info.failReason = "HPが最大値のため回復しませんでした";
			info.noEffectBecauseFull = true;
		}
		else
		{
			player.HealRuntime(static_cast<float>(item.GetHealAmount()));
			info.hpAfter = player.GetHP();
			info.effectApplied = info.hpAfter > info.hpBefore;
		}
		break;
	case ItemType::AmmoSmall:
		if (item.GetAmmoAmount() <= 0) info.failReason = "AmmoSmall回復量が0以下です";
		else if (info.maxReserve <= 0) info.failReason = "最大予備弾薬が0以下です";
		else if (info.reserveBefore >= info.maxReserve)
		{
			info.failReason = "予備弾薬が最大値のため回復しませんでした";
			info.noEffectBecauseFull = true;
		}
		else
		{
			player.AddReserveAmmo(item.GetAmmoAmount());
			info.reserveAfter = player.GetReserveAmmo();
			info.effectApplied = info.reserveAfter > info.reserveBefore;
		}
		break;
	case ItemType::NextStageKey:
		info.effectApplied = true;
		break;
	case ItemType::None:
	default:
		info.failReason = "未対応のItemTypeです";
		break;
	}

	info.hpAfter = player.GetHP();
	info.maxHp = player.GetMaxHP();
	info.reserveAfter = player.GetReserveAmmo();
	info.maxReserve = player.GetMaxReserveAmmo();
	info.magazineAmmo = player.GetMagazineAmmo();
	info.hudMagazineAmmo = info.magazineAmmo;
	info.hudReserveAmmo = info.reserveAfter;
	if (info.effectApplied) info.failReason = "なし";
	return info.effectApplied;
}

void ItemManager::Clear()
{
	if (registeredCollisionManager_)
	{
		for (auto& item : items_)
		{
			if (item) registeredCollisionManager_->RemoveCollider(item.get());
		}
	}
	items_.clear();
	collectedEvents_.clear();
	lastPickedItemType_ = ItemType::None;
	lastDroppedItemType_ = ItemType::None;
	lastDropPosition_ = {};
	lastKnownMagazineAmmo_ = 0;
	lastKnownReserveAmmo_ = 0;
	lastKnownMaxReserveAmmo_ = 0;
	lastAmmoSmallReserveRestored_ = 0;
	lastItemEffectDebugInfo_ = {};
	itemVisualEffect_.Clear();
	registeredCollisionManager_ = nullptr;
}

bool ItemManager::ConsumeCollected(ItemType type)
{
	auto it = std::find(collectedEvents_.begin(), collectedEvents_.end(), type);
	if (it == collectedEvents_.end()) return false;
	collectedEvents_.erase(it);
	return true;
}

int ItemManager::GetActiveItemCount() const
{
	return static_cast<int>(std::count_if(items_.begin(), items_.end(), [](const std::unique_ptr<Item>& item)
		{
			return item && item->IsActive();
		}));
}

int ItemManager::GetActiveItemCount(ItemType type) const
{
	return static_cast<int>(std::count_if(items_.begin(), items_.end(), [type](const std::unique_ptr<Item>& item)
		{
			return item && item->IsActive() && item->GetType() == type;
		}));
}

float ItemManager::GetNoneDropChance() const
{
	return std::max(0.0f, 1.0f - std::max(0.0f, healDropChance_) - std::max(0.0f, ammoDropChance_));
}

void ItemManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("アイテム管理")) { ImGui::End(); return; }
	ImGui::Text("Active Item数: %d", GetActiveItemCount());
	ImGui::Text("HealSmall数: %d", GetActiveItemCount(ItemType::HealSmall));
	ImGui::Text("AmmoSmall数: %d", GetActiveItemCount(ItemType::AmmoSmall));
	ImGui::Checkbox("敵死亡時ドロップ有効", &enemyDeathDropEnabled_);
	ImGui::Checkbox("必ず何か落とす", &forceEnemyDeathDrop_);
	float healDropPercent = healDropChance_ * 100.0f;
	float ammoDropPercent = ammoDropChance_ * 100.0f;
	if (ImGui::SliderFloat("HealSmallドロップ確率", &healDropPercent, 0.0f, 100.0f, "%.0f%%")) healDropChance_ = healDropPercent / 100.0f;
	if (ImGui::SliderFloat("AmmoSmallドロップ確率", &ammoDropPercent, 0.0f, 100.0f, "%.0f%%")) ammoDropChance_ = ammoDropPercent / 100.0f;
	ImGui::DragInt("Heal回復量", &healAmount_, 1, 0, 999);
	ImGui::DragInt("Ammo回復量", &ammoAmount_, 1, 0, 999);
	ImGui::DragFloat("pickupRadius", &pickupRadius_, 0.1f, 0.1f, 20.0f, "%.2f");
	ImGui::Text("現在マガジン弾数: %d", lastKnownMagazineAmmo_);
	ImGui::Text("予備弾薬: %d / %d", lastKnownReserveAmmo_, lastKnownMaxReserveAmmo_);
	ImGui::Checkbox("満タン時もItemを消す", &consumeItemWhenFull_);
	ImGui::SeparatorText("最後のItem取得診断");
	ImGui::Text("最後に拾ったItemType: %s", ToItemTypeName(lastItemEffectDebugInfo_.itemType));
	ImGui::Text("効果適用成功: %s", lastItemEffectDebugInfo_.effectApplied ? "true" : "false");
	ImGui::TextWrapped("効果適用失敗理由: %s", lastItemEffectDebugInfo_.failReason.c_str());
	ImGui::Text("HP: %.1f -> %.1f / %.1f", lastItemEffectDebugInfo_.hpBefore, lastItemEffectDebugInfo_.hpAfter, lastItemEffectDebugInfo_.maxHp);
	ImGui::Text("Reserve: %d -> %d / %d", lastItemEffectDebugInfo_.reserveBefore, lastItemEffectDebugInfo_.reserveAfter, lastItemEffectDebugInfo_.maxReserve);
	itemVisualEffect_.DrawImGui();
	ImGui::End();
#else
	(void)this;
#endif
}

void ItemManager::RemoveInactiveItems()
{
	items_.erase(std::remove_if(items_.begin(), items_.end(), [this](const std::unique_ptr<Item>& item)
		{
			const bool shouldRemove = !item || item->IsCollected() || item->IsExpired();
			if (shouldRemove && item)
			{
				itemVisualEffect_.StopIdle(*item);
				if (registeredCollisionManager_) registeredCollisionManager_->RemoveCollider(item.get());
			}
			return shouldRemove;
		}), items_.end());
}

void ItemManager::SpawnDropItem(ItemType type, const K4E::Vector3& position)
{
	SpawnConfigured(type, position);
}

void ItemManager::SpawnConfigured(ItemType type, const K4E::Vector3& position, int overrideAmmoAmount)
{
	if (type == ItemType::None) return;
	auto item = std::make_unique<Item>();
	const int ammoAmount = (overrideAmmoAmount >= 0) ? overrideAmmoAmount : ammoAmount_;
	item->Initialize(type, position, healAmount_, ammoAmount, pickupRadius_);
	ApplyVisualSettings(*item);
	itemVisualEffect_.StartIdle(*item);
	if (registeredCollisionManager_) registeredCollisionManager_->AddCollider(item.get());
	items_.push_back(std::move(item));
}

void ItemManager::ApplyVisualSettings(Item& item)
{
	const auto& settings = itemVisualEffect_.GetSettings();
	item.SetVisualAnimationSettings(settings.itemFloatHeight, settings.itemFloatSpeed, settings.itemRotationSpeed);
	item.SetVisualColor(itemVisualEffect_.GetEffectColor(item.GetType()));
}

void ItemManager::LogDropRollResult(ItemType type, const K4E::Vector3& position) const
{
	std::ostringstream oss;
	oss << "Drop Roll Result ItemType=" << ToItemTypeName(type)
		<< " SpawnPosition=(" << position.x << ", " << position.y << ", " << position.z << ")"
		<< " ActiveItemCount=" << GetActiveItemCount() << "\n";
	OutputDebugStringA(oss.str().c_str());
}

void ItemManager::LogItemEffectResult() const
{
	const ItemEffectDebugInfo& info = lastItemEffectDebugInfo_;
	std::ostringstream oss;
	oss << "Item Runtime Result Type=" << ToItemTypeName(info.itemType)
		<< " Applied=" << (info.effectApplied ? "true" : "false")
		<< " Reason=" << info.failReason
		<< " HP=" << info.hpBefore << "->" << info.hpAfter
		<< " Reserve=" << info.reserveBefore << "->" << info.reserveAfter << "\n";
	OutputDebugStringA(oss.str().c_str());
}
