#pragma once

#include "GamePlayWorld.h"
#include "PostEffect/PlayerHealthPostEffectController.h"

#include <Actor.h>
#include <Camera.h>
#include <GpuParticleManager.h>
#include <Matrix4x4.h>
#include <ModelComponent.h>
#include <PostEffectManager.h>
#include <Player.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

class GamePlayEffectController
{
public:
	void Initialize(GamePlayWorld* world)
	{
		hpPostEffectController_ = std::make_unique<PlayerHealthPostEffectController>();
		hpPostEffectController_->Initialize(Ken4lowEngine::PostEffectManager::GetInstance());
		Ken4lowEngine::EffectSystem::GetInstance()->RegisterSpriteEffect("BossBlood", Ken4lowEngine::GpuParticleType::Blood, 64, 0, 0.0f, 0.30f);
		trackedBoss_ = nullptr;
		lastBossHp_ = 0.0f;
		playerDamageListenerId_ = 0;
		BindPlayerDamageCallback(world);
		BindWeaponFireEffectTransform(world);
	}

	void Finalize(GamePlayWorld* world)
	{
		if (auto* player = world ? world->GetCharacters().GetPlayer() : nullptr)
		{
			if (playerDamageListenerId_ != 0) player->RemoveDamageListener(playerDamageListenerId_);
			player->SetOnDamageTakenCallback({});
		}
		playerDamageListenerId_ = 0;
		if (BulletManager* bulletManager = world ? world->GetBulletManager() : nullptr) bulletManager->SetShotEffectTransformResolver({});
		trackedBoss_ = nullptr;
		lastBossHp_ = 0.0f;
		if (hpPostEffectController_)
		{
			hpPostEffectController_->Finalize();
			hpPostEffectController_.reset();
		}
	}

	void Update(float deltaTime, GamePlayWorld* world)
	{
		if (hpPostEffectController_) hpPostEffectController_->Update(deltaTime, world ? world->GetCharacters().GetPlayerRuntime() : nullptr);
		UpdateBossHitBlood(world);
	}

	void DrawPlayerDebugContent(){ if (hpPostEffectController_) hpPostEffectController_->DrawImGuiContent(); }
	bool HasPlayerDebugContent() const { return hpPostEffectController_ != nullptr; }

private:
	static Ken4lowEngine::Vector3 GetMuzzleLocalOffset(int weaponId)
	{
		switch (weaponId)
		{
		case 1: return { 0.0f, -0.01f, 0.58f };
		case 3: return { 0.0f, 0.00f, 0.82f };
		case 4: return { 0.0f, 0.00f, 1.08f };
		case 5: return { 0.0f, 0.00f, 0.92f };
		default: return { 0.0f, -0.02f, 0.85f };
		}
	}

	static float DistanceSqXZ(const Ken4lowEngine::Vector3& first, const Ken4lowEngine::Vector3& second)
	{
		const float dx = first.x - second.x;
		const float dz = first.z - second.z;
		return dx * dx + dz * dz;
	}

	static bool ResolveDamageSourcePosition(GamePlayWorld* world, const Ken4lowEngine::PlayerActor& player,
		const Ken4lowEngine::CharacterDamageInfo& damageInfo, Ken4lowEngine::Vector3& outPosition)
	{
		if (damageInfo.sourceActor)
		{
			if (const Ken4lowEngine::SceneComponent* sourceRoot = damageInfo.sourceActor->GetRootComponent())
			{
				outPosition = sourceRoot->GetWorldPosition();
				return true;
			}
		}
		if (!world || damageInfo.hasHitPosition) return false;

		const Ken4lowEngine::Vector3 playerPosition = player.GetWorldPosition();
		float nearestDistanceSq = std::numeric_limits<float>::max();
		bool found = false;
		for (EnemyBase* enemy : world->GetCharacters().GetEnemyRawList())
		{
			if (!enemy || enemy->IsDead()) continue;
			const Ken4lowEngine::Vector3 enemyPosition = enemy->GetCenterPosition();
			const float distanceSq = DistanceSqXZ(playerPosition, enemyPosition);
			if (distanceSq >= nearestDistanceSq || distanceSq > 32.0f * 32.0f) continue;
			nearestDistanceSq = distanceSq;
			outPosition = enemyPosition;
			found = true;
		}
		if (Ken4lowEngine::BossActor* boss = world->GetBoss(); boss && boss->IsAlive())
		{
			const Ken4lowEngine::Vector3 bossPosition = boss->GetPosition();
			const float distanceSq = DistanceSqXZ(playerPosition, bossPosition);
			if (distanceSq < nearestDistanceSq && distanceSq <= 40.0f * 40.0f)
			{
				outPosition = bossPosition;
				found = true;
			}
		}
		return found; // 発生元情報が無い旧Damageだけ、攻撃可能距離内の最寄りCharacterへ補完する。
	}

	static bool ResolveWeaponMuzzleTransform(GamePlayWorld* world, Ken4lowEngine::Vector3& outPosition, Ken4lowEngine::Vector3& outDirection)
	{
		auto* player = world ? world->GetCharacters().GetPlayer() : nullptr;
		const auto* weaponView = player ? player->GetWeaponViewComponent() : nullptr;
		const auto* weapon = player ? player->GetWeaponComponent() : nullptr;
		Ken4lowEngine::Camera* camera = player ? player->GetCamera() : nullptr;
		if (!weaponView || !weapon || !camera || weapon->IsMeleeWeapon()) return false;
		const Ken4lowEngine::Matrix4x4 cameraRotation = Ken4lowEngine::Matrix4x4::MakeRotateMatrix(camera->GetRotate());
		const Ken4lowEngine::Vector3 weaponWorldPosition = camera->GetTranslate() + Ken4lowEngine::Vector3::Transform(weaponView->GetLocalPosition(), cameraRotation);
		const Ken4lowEngine::Vector3 weaponWorldRotation = camera->GetRotate() + weaponView->GetLocalRotation();
		const Ken4lowEngine::Matrix4x4 weaponWorldMatrix = Ken4lowEngine::Matrix4x4::MakeAffineMatrix(weaponView->GetLocalScale(), weaponWorldRotation, weaponWorldPosition);
		outPosition = Ken4lowEngine::Vector3::Transform(GetMuzzleLocalOffset(weapon->GetWeaponId()), weaponWorldMatrix);
		outDirection = Ken4lowEngine::Vector3::Normalize(camera->GetForward());
		return std::isfinite(outPosition.x) && std::isfinite(outPosition.y) && std::isfinite(outPosition.z) &&
			std::isfinite(outDirection.x) && std::isfinite(outDirection.y) && std::isfinite(outDirection.z);
	}

	void BindPlayerDamageCallback(GamePlayWorld* world)
	{
		if (auto* player = world ? world->GetCharacters().GetPlayer() : nullptr)
		{
			player->SetOnDamageTakenCallback({});
			playerDamageListenerId_ = player->AddDamageListener(
				[this, world](const Ken4lowEngine::CharacterDamageInfo& damageInfo, const Ken4lowEngine::CharacterDamageResult& damageResult)
				{
					if (!damageResult.accepted || damageResult.appliedDamage <= 0.0f) return;
					if (hpPostEffectController_) hpPostEffectController_->NotifyDamageTaken();
					if (!world) return;
					auto* currentPlayer = world->GetCharacters().GetPlayer();
					HUDManager* hud = world->GetHUDManager();
					Ken4lowEngine::Camera* camera = currentPlayer ? currentPlayer->GetCamera() : nullptr;
					if (!currentPlayer || !hud || !camera) return;
					Ken4lowEngine::Vector3 attackerPosition{};
					if (ResolveDamageSourcePosition(world, *currentPlayer, damageInfo, attackerPosition))
					{
						const Ken4lowEngine::Vector3 cameraForward = Ken4lowEngine::Vector3::NormalizeSafe(camera->GetForward(), { 0.0f, 0.0f, 1.0f });
						const Ken4lowEngine::Vector3 cameraRight{ cameraForward.z, 0.0f, -cameraForward.x };
						hud->AddDamageIndicator(currentPlayer->GetWorldPosition(), attackerPosition, cameraForward, cameraRight);
					}
					hud->NotifyPlayerHit();
				});
		}
	}

	void BindWeaponFireEffectTransform(GamePlayWorld* world)
	{
		if (BulletManager* bulletManager = world ? world->GetBulletManager() : nullptr)
		{
			bulletManager->SetShotEffectTransformResolver([world](Ken4lowEngine::Vector3& position, Ken4lowEngine::Vector3& direction)
				{ return ResolveWeaponMuzzleTransform(world, position, direction); });
		}
	}

	void UpdateBossHitBlood(GamePlayWorld* world)
	{
		Ken4lowEngine::BossActor* boss = world ? world->GetBoss() : nullptr;
		if (boss != trackedBoss_)
		{
			trackedBoss_ = boss;
			lastBossHp_ = boss ? boss->GetHP() : 0.0f;
			return;
		}
		if (!boss) return;
		const float currentHp = boss->GetHP();
		if (currentHp + 0.001f < lastBossHp_)
		{
			const float appliedDamage = lastBossHp_ - currentHp;
			Ken4lowEngine::Vector3 bloodPosition = boss->GetPosition();
			bloodPosition.y += 1.15f;
			const uint32_t emitCount = static_cast<uint32_t>(std::clamp(48L + std::lround(appliedDamage * 1.4f), 48L, 120L));
			Ken4lowEngine::EffectSystem::GetInstance()->Play("BossBlood", bloodPosition, emitCount); // 新BossActorの共通Health減少を被弾演出へ接続する。
		}
		lastBossHp_ = currentHp;
	}

	std::unique_ptr<PlayerHealthPostEffectController> hpPostEffectController_;
	Ken4lowEngine::CharacterActor::DamageListenerId playerDamageListenerId_ = 0;
	Ken4lowEngine::BossActor* trackedBoss_ = nullptr;
	float lastBossHp_ = 0.0f;
};
