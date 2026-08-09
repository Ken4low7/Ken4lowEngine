#define NOMINMAX
#include "EnemyHPBarManager.h"

#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "EnemyBase.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void EnemyHPBarManager::Initialize()
{
	entries_.clear();
}

EnemyHPBarManager::Entry* EnemyHPBarManager::FindEntry(EnemyBase* enemy)
{
	const auto it = std::find_if(entries_.begin(), entries_.end(), [enemy](const Entry& entry)
		{
			return entry.enemy == enemy;
		});
	return it != entries_.end() ? &(*it) : nullptr;
}

EnemyHPBarManager::Entry& EnemyHPBarManager::FindOrCreateEntry(EnemyBase* enemy)
{
	if (Entry* entry = FindEntry(enemy)) return *entry;
	entries_.push_back(Entry{ .enemy = enemy });
	return entries_.back();
}

void EnemyHPBarManager::Update(
	const std::vector<EnemyBase*>& enemies,
	const K4E::Matrix4x4& viewMatrix,
	const K4E::Matrix4x4& projMatrix,
	float screenWidth,
	float screenHeight,
	float deltaTime,
	const EnemyBase* aimedEnemy,
	bool showOnlyWhenAimed,
	float visibleHoldTime)
{
	(void)viewMatrix;
	(void)projMatrix;
	(void)screenWidth;
	(void)screenHeight;

	const float safeDeltaTime = std::max(0.0f, deltaTime);
	const float safeVisibleHoldTime = std::max(0.0f, visibleHoldTime);
	for (Entry& entry : entries_)
	{
		entry.updatedThisFrame = false;
		entry.visibleThisFrame = false;
	}

	for (EnemyBase* enemy : enemies)
	{
		auto* enemyActor = dynamic_cast<K4E::EnemyActor*>(enemy);
		if (!enemyActor) continue;

		Entry& entry = FindOrCreateEntry(enemy);
		entry.updatedThisFrame = true;
		if (enemy == aimedEnemy)
		{
			entry.aimVisibleTimer = safeVisibleHoldTime;
		}
		else
		{
			entry.aimVisibleTimer = std::max(0.0f, entry.aimVisibleTimer - safeDeltaTime);
		}

		const bool aimVisible = !showOnlyWhenAimed || enemy == aimedEnemy || entry.aimVisibleTimer > 0.0f;
		entry.visibleThisFrame = !enemy->IsDead() && aimVisible;
		enemyActor->SetHealthBarVisible(entry.visibleThisFrame); // 通常敵のHP描画はActor所有のWorldGaugeだけへ統一する。
	}

	std::erase_if(entries_, [](const Entry& entry)
		{
			return !entry.updatedThisFrame;
		});
}

void EnemyHPBarManager::Draw()
{
	// WorldGaugeComponentはActorWorldの通常描画で処理されるため、追加描画は不要。
}

void EnemyHPBarManager::DrawImGuiContent() const
{
#ifdef USE_IMGUI
	const int visibleCount = static_cast<int>(std::count_if(entries_.begin(), entries_.end(), [](const Entry& entry)
		{
			return entry.visibleThisFrame;
		}));
	ImGui::Text("WorldGauge Entries: %d", static_cast<int>(entries_.size()));
	ImGui::Text("Visible This Frame: %d", visibleCount);
#endif
}
