#define NOMINMAX
#include "EnemyMigrationValidation.h"

#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAIComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAttackComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyEffectComponent.h"
#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "ApplicationLayer/Character/Player/Actor/PlayerCameraComponent.h"
#include "ApplicationLayer/Character/Player/Actor/WeaponComponent.h"

#include <ActorWorld.h>
#include <Camera.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	const K4E::Vector3 kEnemyPosition{ 8.0f, 2.0f, 9.0f };
	constexpr float kRayEpsilon = 0.00001f;
	constexpr const char* kEnemyPrefabPath = "Resources/ActorPrefabs/ComponentEnemy.json";

	/// Camera中心RayとAABBの最初の交点距離をslab法で求める。
	bool IntersectRayAabb(
		const K4E::Vector3& origin,
		const K4E::Vector3& direction,
		const K4E::Vector3& center,
		const K4E::Vector3& halfSize,
		float maxDistance,
		float& outDistance)
	{
		float tMin = 0.0f;
		float tMax = maxDistance;

		const float originValues[3] = { origin.x, origin.y, origin.z };
		const float directionValues[3] = { direction.x, direction.y, direction.z };
		const float minValues[3] = { center.x - halfSize.x, center.y - halfSize.y, center.z - halfSize.z };
		const float maxValues[3] = { center.x + halfSize.x, center.y + halfSize.y, center.z + halfSize.z };

		for (int axis = 0; axis < 3; ++axis)
		{
			const float axisDirection = directionValues[axis];
			if (std::abs(axisDirection) <= kRayEpsilon)
			{
				if (originValues[axis] < minValues[axis] || originValues[axis] > maxValues[axis]) return false;
				continue;
			}

			const float invDirection = 1.0f / axisDirection;
			float t1 = (minValues[axis] - originValues[axis]) * invDirection;
			float t2 = (maxValues[axis] - originValues[axis]) * invDirection;
			if (t1 > t2) std::swap(t1, t2);
			tMin = std::max(tMin, t1);
			tMax = std::min(tMax, t2);
			if (tMin > tMax) return false;
		}

		if (tMin < 0.0f || tMin > maxDistance) return false;
		outDistance = tMin;
		return true;
	}
}

void EnemyMigrationValidation::Initialize(K4E::ActorWorld& actorWorld)
{
	actorWorld_ = &actorWorld;
	navigationObstacles_ = {
		{ { 7.0f, 0.0f, 4.0f }, { 9.0f, 3.0f, 6.0f } },
	};

	enemy_ = &actorWorld_->SpawnActor<K4E::EnemyActor>();
	const bool loadedFromPrefab = actorWorld_->ReloadActorFromJson(*enemy_, kEnemyPrefabPath);
	if (loadedFromPrefab)
	{
		enemy_->Initialize(); // 古いPrefabに無い必須UI Componentだけを不足分として補完する。
		lastMessage_ = "EnemyActorを保存済みPrefabから自動読込しました。";
		lastSucceeded_ = true;
	}
	else
	{
		enemy_->ResetForComparison(kEnemyPosition);
		lastMessage_ = "Enemy Prefabが無いためコード既定値で生成しました。";
		lastSucceeded_ = false;
	}

	// 保存データ側の名前変更でTarget接続が切れないようDebugSceneの安定識別名は起動時に固定する。
	enemy_->SetName(enemyName_);
	enemy_->SetLayer("EnemyValidation");
	enemy_->AddTag("NormalEnemy");
	enemy_->SetNavigationObstacles(&navigationObstacles_);
	enemy_->SetHealthBarVisible(false);

	RefreshActorReferencesAndBindings();
	UpdateAimTarget();
}

void EnemyMigrationValidation::Update(float deltaTime)
{
	(void)deltaTime;
	RefreshActorReferencesAndBindings();
	UpdateAimTarget();
	ProcessWeaponShot();
	ProcessRequests();
}

void EnemyMigrationValidation::UpdateEditor()
{
	RefreshActorReferencesAndBindings();
	aimedEnemy_ = nullptr;
	aimedBoss_ = nullptr;
	if (enemy_) enemy_->SetHealthBarVisible(false);
	if (auto* player = dynamic_cast<K4E::PlayerActor*>(target_)) player->SetCrosshairTargeted(false);
	ProcessRequests();
}

void EnemyMigrationValidation::DrawImGui()
{
#ifdef USE_IMGUI
	const bool windowVisible = ImGui::Begin("通常敵 Player Target 検証");
	if (windowVisible)
	{
		const auto* ai = enemy_ ? enemy_->GetEnemyAIComponent() : nullptr;
		const auto* attack = enemy_ ? enemy_->GetEnemyAttackComponent() : nullptr;
		const auto* effect = enemy_ ? enemy_->GetEnemyEffectComponent() : nullptr;
		const auto* visual = enemy_ ? enemy_->GetHumanoidVisualComponent() : nullptr;
		const auto* collider = enemy_ ? enemy_->GetColliderComponent() : nullptr;
		const auto* health = enemy_ ? enemy_->GetHealthComponent() : nullptr;
		const auto* animation = enemy_ ? enemy_->GetAnimationComponent() : nullptr;
		const auto* rigidbodyComponent = enemy_ ? enemy_->GetComponent<K4E::RigidbodyComponent>() : nullptr;
		const auto* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;

		ImGui::Text("Target: %s", target_ ? target_->GetName().c_str() : "なし");
		ImGui::Text("Aim: %s", aimedEnemy_ ? "Enemy" : (aimedBoss_ ? "Boss" : "None"));
		ImGui::Text("AI: %s", ai ? (ai->HasPath() ? "追跡中" : "待機") : "None");
		ImGui::Text("Attack: %.0f damage / hit %d", attack ? attack->GetAttackDamage() : 0.0f, attack ? attack->GetAcceptedHitCount() : 0);
		ImGui::Text("HP: %.0f / %.0f", health ? health->GetCurrentHealth() : 0.0f, health ? health->GetMaxHealth() : 0.0f);
		ImGui::Text("Animation: %s", animation ? animation->GetAnimationName().c_str() : "None");
		ImGui::Text("Visual: %zu parts", visual ? visual->GetParts().size() : 0u);
		ImGui::Text("Collider: %s", collider && collider->IsActive() ? "有効" : "停止");
		ImGui::Text("Physics: %s / Grounded: %s", rigidbody ? "Dynamic" : "None", rigidbody && rigidbody->IsGrounded() ? "Yes" : "No");
		ImGui::Text("Effect: Hit %d / Death %d", effect ? effect->GetHitEffectCount() : 0, effect ? effect->GetDeathEffectCount() : 0);
		ImGui::TextColored(lastSucceeded_ ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", lastMessage_.c_str());

		if (ImGui::Button("Enemyへ60ダメージ")) requestDamage_ = true;
		ImGui::SameLine();
		if (ImGui::Button("Enemyを死亡させる")) requestLethalDamage_ = true;
		ImGui::SameLine();
		if (ImGui::Button("Enemy Reset")) requestReset_ = true;
		ImGui::TextDisabled("ゲームHUDはGauge/Text/Sprite ComponentからActorWorldの2D描画パスへ流れます。");
	}
	ImGui::End();
#endif
}

void EnemyMigrationValidation::Finalize()
{
	if (enemy_) enemy_->SetHealthBarVisible(false);
	if (auto* player = dynamic_cast<K4E::PlayerActor*>(target_)) player->SetCrosshairTargeted(false);
	aimedEnemy_ = nullptr;
	aimedBoss_ = nullptr;
	enemy_ = nullptr;
	boss_ = nullptr;
	target_ = nullptr;
	actorWorld_ = nullptr;
	lastProcessedShotRevision_ = 0;
	navigationObstacles_.clear();
}

void EnemyMigrationValidation::RefreshActorReferencesAndBindings()
{
	if (!actorWorld_)
	{
		aimedEnemy_ = nullptr;
		aimedBoss_ = nullptr;
		enemy_ = nullptr;
		boss_ = nullptr;
		target_ = nullptr;
		return;
	}

	K4E::CharacterActor* previousTarget = target_;
	enemy_ = dynamic_cast<K4E::EnemyActor*>(actorWorld_->FindActorByName(enemyName_));
	boss_ = dynamic_cast<K4E::BossActor*>(actorWorld_->FindActorByName(bossName_));
	target_ = dynamic_cast<K4E::CharacterActor*>(actorWorld_->FindActorByName(targetName_));

	if (previousTarget != target_)
	{
		lastProcessedShotRevision_ = 0;
		if (auto* player = dynamic_cast<K4E::PlayerActor*>(target_))
		{
			if (const auto* weapon = player->GetWeaponComponent()) lastProcessedShotRevision_ = weapon->GetShotRevision();
		}
	}

	if (!enemy_)
	{
		lastSucceeded_ = false;
		lastMessage_ = "EnemyActorが見つかりません。";
		return;
	}

	enemy_->SetTargetActor(target_);
	enemy_->SetNavigationObstacles(&navigationObstacles_);
	lastSucceeded_ = target_ != nullptr;
	lastMessage_ = lastSucceeded_
		? "DebugPlayerのWeapon、照準、Enemy/Boss UI Componentを実戦形式で検証しています。"
		: "DebugPlayerが見つからないためTargetと射撃を解除しています。";
}

void EnemyMigrationValidation::UpdateAimTarget()
{
	if (enemy_) enemy_->SetHealthBarVisible(false);
	aimedEnemy_ = nullptr;
	aimedBoss_ = nullptr;

	auto* player = dynamic_cast<K4E::PlayerActor*>(target_);
	const K4E::PlayerCameraComponent* playerCamera = player ? player->GetPlayerCameraComponent() : nullptr;
	const K4E::Camera* camera = playerCamera ? playerCamera->GetCamera() : nullptr;
	const K4E::WeaponComponent* weapon = player ? player->GetWeaponComponent() : nullptr;
	if (!player || !camera || !weapon || player->IsDead())
	{
		if (player) player->SetCrosshairTargeted(false);
		return;
	}

	const K4E::Vector3 origin = camera->GetTranslate();
	const K4E::Vector3 direction = K4E::Vector3::NormalizeSafe(camera->GetForward(), { 0.0f, 0.0f, 1.0f });
	const float maxDistance = weapon->GetRange();
	float closestDistance = maxDistance;

	auto testEnemy = [&](K4E::EnemyActor* candidate)
	{
		if (!candidate || candidate->IsDead() || !candidate->IsActive()) return;
		const K4E::CharacterColliderComponent* collider = candidate->GetColliderComponent();
		if (!collider || !collider->IsActive()) return;
		float hitDistance = 0.0f;
		if (IntersectRayAabb(origin, direction, collider->GetWorldPosition(), collider->GetHalfSize(), closestDistance, hitDistance))
		{
			closestDistance = hitDistance;
			aimedEnemy_ = candidate;
			aimedBoss_ = nullptr;
		}
	};

	auto testBoss = [&](K4E::BossActor* candidate)
	{
		if (!candidate || candidate->IsDead() || !candidate->IsActive()) return;
		const K4E::CharacterColliderComponent* collider = candidate->GetColliderComponent();
		if (!collider || !collider->IsActive()) return;
		float hitDistance = 0.0f;
		if (IntersectRayAabb(origin, direction, collider->GetWorldPosition(), collider->GetHalfSize(), closestDistance, hitDistance))
		{
			closestDistance = hitDistance;
			aimedEnemy_ = nullptr;
			aimedBoss_ = candidate;
		}
	};

	testEnemy(enemy_);
	testBoss(boss_);

	if (enemy_) enemy_->SetHealthBarVisible(aimedEnemy_ == enemy_);
	player->SetCrosshairTargeted(aimedEnemy_ != nullptr || aimedBoss_ != nullptr);
}

void EnemyMigrationValidation::ProcessWeaponShot()
{
	auto* player = dynamic_cast<K4E::PlayerActor*>(target_);
	K4E::WeaponComponent* weapon = player ? player->GetWeaponComponent() : nullptr;
	if (!player || !weapon) return;

	const unsigned int shotRevision = weapon->GetShotRevision();
	if (shotRevision == lastProcessedShotRevision_) return;
	lastProcessedShotRevision_ = shotRevision;

	if (aimedEnemy_ && !aimedEnemy_->IsDead())
	{
		aimedEnemy_->ApplyComparisonDamage(weapon->GetDamage());
		lastSucceeded_ = true;
		lastMessage_ = "Camera中心RayでEnemyへ射撃Damageを適用しました。";
		return;
	}
	if (aimedBoss_ && !aimedBoss_->IsDead())
	{
		aimedBoss_->ApplyDamage(weapon->GetDamage());
		lastSucceeded_ = true;
		lastMessage_ = "Camera中心RayでBossへ射撃Damageを適用しました。";
		return;
	}

	lastMessage_ = "発射しましたがCamera中心RayにDamage対象はありませんでした。";
}

void EnemyMigrationValidation::ProcessRequests()
{
	if (requestDamage_)
	{
		requestDamage_ = false;
		if (enemy_ && !enemy_->IsDead()) enemy_->ApplyComparisonDamage(60.0f);
	}
	if (requestLethalDamage_)
	{
		requestLethalDamage_ = false;
		if (enemy_ && !enemy_->IsDead()) enemy_->ApplyComparisonDamage(10000.0f);
	}
	if (requestReset_)
	{
		requestReset_ = false;
		if (enemy_) enemy_->ResetForComparison(kEnemyPosition);
		RefreshActorReferencesAndBindings();
		UpdateAimTarget();
	}
}
