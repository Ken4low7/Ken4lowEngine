#pragma once
#include "Collider.h"
#include "ContactRecord.h"
#include "Object3D.h"
#include "CollisionTypeIdDef.h"
#include "Scene/Actor/Character/CharacterDamage.h"
#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "EnemyBase.h"
#include "EnemySpawnCrystal.h"
#include <Vector3.h>
#include <Vector4.h>

#include <memory>
#include <functional>
#include "WeaponMasterData.h"

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine { class Input; }
struct WeaponParams;
class CollisionManager;

/// -------------------------------------------------------------
/// 弾クラス
/// -------------------------------------------------------------
class Bullet : public K4E::Collider
{
public:
	Bullet() = default;
	void Initialize(const K4E::Vector3& startPos,
		const K4E::Vector3& velocity,
		int damage = 1,
		float lifeTimeSec = 3.0f,
		const K4E::Vector3& shooterPosition = { 0.0f, 0.0f, 0.0f },
		uint32_t shooterColliderId = 0u,
		uint32_t typeId = static_cast<uint32_t>(CollisionTypeIdDef::kBullet));

	void Update(float dt);
	void Draw();
	void DrawImGui();
	void OnCollisionEnter(K4E::Collider* other) override;
	void OnCollisionEnter(const K4E::CollisionHit& hit) override;
	void OnOverlapBegin(const K4E::CollisionHit& hit) override;
	void OnCollisionExit(K4E::Collider* other) override
	{
		if (!other || GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kBullet) || !damageableHitCallback_) return;
		const uint32_t type = other->GetTypeID();
		bool killed = false;
		if (type == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
		{
			const EnemyBase* enemy = other->GetOwner<EnemyBase>();
			if (!enemy) return;
			killed = enemy->IsDead();
		}
		else if (type == static_cast<uint32_t>(CollisionTypeIdDef::kBoss))
		{
			const K4E::BossActor* boss = other->GetOwner<K4E::BossActor>();
			if (!boss) return;
			killed = boss->IsDead();
		}
		else if (type == static_cast<uint32_t>(CollisionTypeIdDef::kCrystal))
		{
			const EnemySpawnCrystal* crystal = other->GetOwner<EnemySpawnCrystal>();
			if (!crystal) return;
			killed = crystal->IsDestroyed();
		}
		else return;
		damageableHitCallback_(killed); // Exit時点のActor状態からHITとKILLを分ける。
	}

	bool IsDead() const { return isDead_; }
	bool IsRemovable() const { return removable_; }
	int GetDamage() const { return damage_; }
	const K4E::Vector3& GetMoveVelocity() const { return moveVelocity_; }
	float GetCollisionRadius() const { return scale_.x; }
	bool UsesPhysicsTrigger() const { return usePhysicsTrigger_; }
	void SetUsePhysicsTrigger(bool enabled);
	bool HasPhysicsHit() const { return hasPhysicsHit_; }
	void MarkPhysicsHit();
	void ClearPhysicsHit();
	bool IsEligibleForPhysicsTrigger() const;
	void HandlePhysicsTriggerHit(K4E::Collider* other);
	void SetShooterPosition(const K4E::Vector3& pos) { shooterPosition_ = pos; }
	const K4E::Vector3& GetShooterPosition() const { return shooterPosition_; }
	void SetShooterColliderId(uint32_t id) { shooterColliderId_ = id; }
	uint32_t GetShooterColliderId() const { return shooterColliderId_; }
	void SetCollisionManager(CollisionManager* collisionManager) { collisionManager_ = collisionManager; }
	static void SetDamageableHitCallback(std::function<void(bool)> callback) { damageableHitCallback_ = std::move(callback); }
	void SetWorldImpactCallback(std::function<void(const K4E::Vector3&, const K4E::Vector3&)> callback) { worldImpactCallback_ = std::move(callback); }
	void SetModelDrawEnabled(bool enabled) { drawModel_ = enabled; }
	bool IsModelDrawEnabled() const { return drawModel_; }
	void ConfigureSplashDamage(const WeaponParams& params);
	bool HasSplashDamage() const { return splashRadius_ > 0.0f && splashDamage_ > 0; }
	float GetSplashRadius() const { return splashRadius_; }
	int GetSplashDamage() const { return splashDamage_; }
	void SetWeaponMetadata(const WeaponParams& params);
	int32_t GetWeaponID() const { return weaponID_; }
	EWeaponCategory GetWeaponCategory() const { return weaponCategory_; }
	EDeathKnockbackType GetDeathKnockbackType() const { return deathKnockbackType_; }
	float GetDeathKnockbackPower() const { return deathKnockbackPower_; }
	float GetDeathKnockbackUpPower() const { return deathKnockbackUpPower_; }
	float GetDeathExplosionRadius() const { return deathExplosionRadius_; }
	float GetDeathImpulseScale() const { return deathImpulseScale_; }
	K4E::CharacterDamageInfo BuildDamageInfo(const K4E::Vector3& hitPosition, const K4E::Vector3& hitDirection, K4E::CharacterDamageType damageType = K4E::CharacterDamageType::Projectile) const;

private:
	void KillAndMoveFar();
	void ProcessHit(K4E::Collider* other, bool fromPhysicsTrigger);
	void TriggerSplashDamageAt(const K4E::Vector3& center);
	void ApplySplashDamageToType(uint32_t targetType, const K4E::Vector3& center);

private:
	inline static std::function<void(bool)> damageableHitCallback_{};
	K4E::Vector3 moveVelocity_{};
	K4E::Vector4 debugColor_{ 1.0f, 1.0f, 0.0f, 1.0f };
	K4E::Vector3 shooterPosition_{};
	uint32_t shooterColliderId_ = 0u;
	CollisionManager* collisionManager_ = nullptr;
	std::function<void(const K4E::Vector3&, const K4E::Vector3&)> worldImpactCallback_{};
	float splashRadius_ = 0.0f;
	int splashDamage_ = 0;
	bool splashCanDamageSelf_ = false;
	bool splashTriggered_ = false;
	int32_t weaponID_ = 0;
	EWeaponCategory weaponCategory_ = EWeaponCategory::Primary;
	EDeathKnockbackType deathKnockbackType_ = EDeathKnockbackType::Default;
	float deathKnockbackPower_ = 8.0f;
	float deathKnockbackUpPower_ = 2.0f;
	float deathExplosionRadius_ = 0.0f;
	float deathImpulseScale_ = 1.0f;
	bool usePhysicsTrigger_ = false;
	bool hasPhysicsHit_ = false;
	std::unique_ptr<K4E::Object3D> model_ = nullptr;
	bool drawModel_ = true;
	K4E::ContactRecord contactRecord_{};
	bool isDead_ = false;
	bool removable_ = false;
	int deadFrames_ = 0;
	int damage_ = 1;
	float lifeTimer_ = 0.0f;
	float lifeTimeSec_ = 3.0f;
	K4E::Vector3 prevPos_{};
	K4E::Vector3 scale_{ 0.1f, 0.1f, 0.1f };
};
