#define NOMINMAX
#include "Bullet.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include "CollisionManager.h"
#include "EnemyBase.h"
#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "GpuParticleManager.h"
#include "WeaponParams.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace Ken4lowEngine;

namespace
{
	float LengthSq(const K4E::Vector3& value){ return value.x * value.x + value.y * value.y + value.z * value.z; }
	K4E::Vector3 NormalizeSafe(const K4E::Vector3& value, const K4E::Vector3& fallback = { 0.0f, 1.0f, 0.0f })
	{
		const float lengthSq = LengthSq(value);
		return lengthSq > 1.0e-10f ? value * (1.0f / std::sqrt(lengthSq)) : fallback;
	}

	ECollisionPresetId GetProjectilePresetId(uint32_t typeId)
	{
		if (typeId == static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet)) return ECollisionPresetId::EnemyProjectile;
		if (typeId == static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet)) return ECollisionPresetId::BossProjectile;
		return ECollisionPresetId::PlayerProjectile;
	}

	K4E::Vector3 ResolveImpactPoint(const Bullet& bullet, const K4E::Collider* target)
	{
		if (!target) return bullet.GetCenterPosition();
		const K4E::Vector3 origin = bullet.GetSegment().origin;
		const K4E::Vector3 direction = bullet.GetSegment().diff;
		const K4E::Vector3 center = target->GetCenterPosition();
		const float directionLengthSq = LengthSq(direction);
		if (directionLengthSq <= 1.0e-8f) return bullet.GetCenterPosition();
		const float projection = std::clamp(
			((center.x - origin.x) * direction.x + (center.y - origin.y) * direction.y + (center.z - origin.z) * direction.z) / directionLengthSq,
			0.0f, 1.0f);
		return origin + direction * projection; // Segment上の対象中心に最も近い点を着弾位置として共有する。
	}
}

void Bullet::Initialize(const K4E::Vector3& startPos, const K4E::Vector3& velocity, int damage, float lifeTimeSec,
	const K4E::Vector3& shooterPosition, uint32_t shooterColliderId, uint32_t typeId)
{
	damage_ = damage;
	moveVelocity_ = velocity;
	lifeTimeSec_ = lifeTimeSec;
	lifeTimer_ = 0.0f;
	shooterPosition_ = shooterPosition;
	shooterColliderId_ = shooterColliderId;
	ApplyCollisionPreset(*this, GetProjectilePresetId(typeId));
	Collider::SetOwner(this);
	model_ = std::make_unique<K4E::Object3D>();
	model_->Initialize("Sample/cube.gltf");
	Collider::SetOBBHalfSize(scale_);
	prevPos_ = startPos;
	Collider::SetCenterPosition(startPos);
	if (model_)
	{
		model_->SetScale(scale_);
		model_->SetTranslate(startPos);
		model_->SetColor(debugColor_);
		model_->Update();
	}
	K4E::Segment segment{};
	segment.origin = startPos;
	Collider::SetSegment(segment);
	isDead_ = false;
	removable_ = false;
	deadFrames_ = 0;
	splashTriggered_ = false;
	usePhysicsTrigger_ = false;
	hasPhysicsHit_ = false;
	contactRecord_.Clear();
}

void Bullet::ConfigureSplashDamage(const WeaponParams& params)
{
	splashRadius_ = std::max(0.0f, params.splashRadius);
	splashDamage_ = std::max(0, params.splashDamage);
	splashCanDamageSelf_ = params.splashCanDamageSelf;
	if (HasSplashDamage())
	{
		debugColor_ = { 1.0f, 0.35f, 0.05f, 1.0f };
		if (model_) model_->SetColor(debugColor_);
	}
}

void Bullet::SetWeaponMetadata(const WeaponParams& params)
{
	weaponID_ = params.weaponID;
	weaponCategory_ = params.weaponCategory;
	deathKnockbackType_ = params.deathKnockbackType;
	deathKnockbackPower_ = std::max(0.0f, params.deathKnockbackPower);
	deathKnockbackUpPower_ = std::max(0.0f, params.deathKnockbackUpPower);
	deathExplosionRadius_ = std::max(0.0f, params.deathExplosionRadius);
	deathImpulseScale_ = std::max(0.01f, params.deathImpulseScale);
}

K4E::CharacterDamageInfo Bullet::BuildDamageInfo(const K4E::Vector3& hitPosition, const K4E::Vector3& hitDirection, K4E::CharacterDamageType damageType) const
{
	K4E::CharacterDamageInfo damageInfo{};
	damageInfo.amount = static_cast<float>(damage_);
	damageInfo.sourceColliderId = shooterColliderId_;
	damageInfo.weaponId = weaponID_;
	damageInfo.damageType = damageType;
	damageInfo.hitPosition = hitPosition;
	damageInfo.hasHitPosition = true;
	damageInfo.hitDirection = NormalizeSafe(hitDirection, NormalizeSafe(moveVelocity_, { 0.0f, 0.0f, 1.0f }));
	damageInfo.hasHitDirection = LengthSq(hitDirection) > 1.0e-10f || LengthSq(moveVelocity_) > 1.0e-10f;
	// 弾が持つ命中位置・方向・武器IDを一つのDamage情報へ集約する。
	return damageInfo;
}

void Bullet::SetUsePhysicsTrigger(bool enabled)
{
	usePhysicsTrigger_ = enabled && IsEligibleForPhysicsTrigger();
	if (!usePhysicsTrigger_) ClearPhysicsHit();
}

void Bullet::MarkPhysicsHit(){ hasPhysicsHit_ = true; }
void Bullet::ClearPhysicsHit(){ hasPhysicsHit_ = false; }

bool Bullet::IsEligibleForPhysicsTrigger() const
{
	return GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet) && weaponCategory_ == EWeaponCategory::Primary && !HasSplashDamage() && deathExplosionRadius_ <= 0.0f;
}

void Bullet::HandlePhysicsTriggerHit(K4E::Collider* other)
{
	if (!usePhysicsTrigger_ || hasPhysicsHit_) return;
	ProcessHit(other, true);
}

void Bullet::KillAndMoveFar()
{
	isDead_ = true;
	deadFrames_ = 0;
	SetEnabled(false);
	const K4E::Vector3 farPosition{ 1.0e9f, 1.0e9f, 1.0e9f };
	Collider::SetCenterPosition(farPosition);
	if (model_) model_->SetTranslate(farPosition);
	K4E::Segment segment{};
	segment.origin = farPosition;
	Collider::SetSegment(segment);
	if (model_) model_->Update();
}

void Bullet::ApplySplashDamageToType(uint32_t targetType, const K4E::Vector3& center)
{
	if (!collisionManager_ || !HasSplashDamage()) return;
	const float radiusSq = splashRadius_ * splashRadius_;
	for (K4E::Collider* collider : collisionManager_->GetCollidersByType(targetType))
	{
		if (!collider) continue;
		if (!splashCanDamageSelf_ && shooterColliderId_ != 0u && collider->GetUniqueID() == shooterColliderId_) continue;
		const K4E::Vector3 toTarget = collider->GetCenterPosition() - center;
		const float distanceSq = LengthSq(toTarget);
		if (distanceSq > radiusSq) continue;
		const float distanceRate = splashRadius_ > 0.0f ? std::clamp(std::sqrt(std::max(0.0f, distanceSq)) / splashRadius_, 0.0f, 1.0f) : 1.0f;
		const int finalDamage = std::max(1, static_cast<int>(std::round(static_cast<float>(splashDamage_) * (1.0f - distanceRate))));

		if (targetType == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
		{
			if (auto* enemy = collider->GetOwner<EnemyBase>(); enemy && !enemy->IsDead())
			{
				enemy->SpawnHitEffectAt(collider->GetCenterPosition());
				enemy->TakeDamage(finalDamage, NormalizeSafe(toTarget, NormalizeSafe(moveVelocity_)), 1.8f);
			}
		}
		else if (targetType == static_cast<uint32_t>(CollisionTypeIdDef::kBoss))
		{
			if (auto* boss = collider->GetOwner<K4E::BossActor>(); boss && boss->IsAlive())
			{
				K4E::CharacterDamageInfo damageInfo = BuildDamageInfo(collider->GetCenterPosition(), NormalizeSafe(toTarget, NormalizeSafe(moveVelocity_)), K4E::CharacterDamageType::Explosion);
				damageInfo.amount = static_cast<float>(finalDamage);
				boss->ApplyBulletDamage(damageInfo);
			}
		}
	}
}

void Bullet::TriggerSplashDamageAt(const K4E::Vector3& center)
{
	if (!HasSplashDamage() || splashTriggered_) return;
	splashTriggered_ = true;
	ApplySplashDamageToType(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy), center);
	ApplySplashDamageToType(static_cast<uint32_t>(CollisionTypeIdDef::kBoss), center);
	if (auto* particles = K4E::GpuParticleManager::GetInstance())
	{
		particles->EmitBurst("HeavySplashImpact", K4E::GpuParticleType::DeathBurstCore, center, 52);
		if (auto* shockwave = particles->EmitBurst("RocketSplashRadiusRing", K4E::GpuParticleType::Shockwave, center, 96))
		{
			shockwave->GetInfoMutable().radius = std::max(1.0f, splashRadius_);
			shockwave->SetPosition(center);
		}
		particles->EmitBurst("HeavySplashSmoke", K4E::GpuParticleType::Smoke, center, 18);
		particles->EmitBurst("RocketSplashDebris", K4E::GpuParticleType::Debris, center, 72);
	}
}

void Bullet::Update(float deltaTime)
{
	if (removable_) return;
	if (isDead_)
	{
		if (++deadFrames_ >= 2) removable_ = true;
		return;
	}
	lifeTimer_ += deltaTime;
	if (lifeTimer_ >= lifeTimeSec_)
	{
		if (HasSplashDamage()) TriggerSplashDamageAt(GetCenterPosition());
		KillAndMoveFar();
		return;
	}
	const K4E::Vector3 current = GetCenterPosition();
	const K4E::Vector3 delta = moveVelocity_ * deltaTime;
	const K4E::Vector3 next = current + delta;
	K4E::Segment segment{};
	segment.origin = current;
	segment.diff = delta;
	SetSegment(segment);
	prevPos_ = current;
	Collider::SetCenterPosition(next);
	if (model_) model_->SetTranslate(next);
	if (std::abs(next.x) > 1000.0f || std::abs(next.z) > 1000.0f)
	{
		KillAndMoveFar();
		return;
	}
	if (model_) model_->Update();
}

void Bullet::Draw(){ if (!removable_ && drawModel_ && model_) model_->Draw(); }
void Bullet::DrawImGui(){ if (model_) model_->DrawImGui(); }

void Bullet::OnCollisionEnter(K4E::Collider* other)
{
	if (!usePhysicsTrigger_) ProcessHit(other, false);
}

void Bullet::ProcessHit(K4E::Collider* other, bool fromPhysicsTrigger)
{
	if (!other || isDead_ || removable_) return;
	if (shooterColliderId_ != 0u && other->GetUniqueID() == shooterColliderId_) return;
	const uint32_t selfType = GetTypeID();
	const uint32_t otherType = other->GetTypeID();
	const uint32_t playerType = static_cast<uint32_t>(CollisionTypeIdDef::kPlayer);
	const uint32_t enemyType = static_cast<uint32_t>(CollisionTypeIdDef::kEnemy);
	const uint32_t bossType = static_cast<uint32_t>(CollisionTypeIdDef::kBoss);
	const uint32_t worldType = static_cast<uint32_t>(CollisionTypeIdDef::kWorld);
	const uint32_t crystalType = static_cast<uint32_t>(CollisionTypeIdDef::kCrystal);
	const bool playerBullet = selfType == static_cast<uint32_t>(CollisionTypeIdDef::kBullet);
	const bool hostileBullet = selfType == static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet) || selfType == static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet);
	const bool shouldHit = (playerBullet && (otherType == enemyType || otherType == bossType || otherType == crystalType || otherType == worldType)) ||
		(hostileBullet && (otherType == playerType || otherType == worldType));
	if (!shouldHit || contactRecord_.Check(other->GetUniqueID())) return;
	contactRecord_.Add(other->GetUniqueID());
	if (fromPhysicsTrigger) MarkPhysicsHit();

	const K4E::Vector3 impactPoint = ResolveImpactPoint(*this, other);
	if (otherType == bossType)
	{
		if (auto* boss = other->GetOwner<K4E::BossActor>())
		{
			boss->ApplyBulletDamage(BuildDamageInfo(impactPoint, moveVelocity_));
		}
	}
	if (HasSplashDamage()) TriggerSplashDamageAt(impactPoint);
	if (playerBullet && otherType == worldType && !HasSplashDamage() && worldImpactCallback_)
	{
		worldImpactCallback_(impactPoint, NormalizeSafe(moveVelocity_ * -1.0f));
	}
	KillAndMoveFar();
}

void Bullet::OnCollisionEnter(const K4E::CollisionHit& hit){ OnCollisionEnter(hit.other); }
void Bullet::OnOverlapBegin(const K4E::CollisionHit& hit){ OnCollisionEnter(hit.other); }
