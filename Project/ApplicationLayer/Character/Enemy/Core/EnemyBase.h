#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <Scene/Actor/Character/HumanoidCharacterActor.h>
#include "Vector3.h"
#include "Vector4.h"
#include "AABB.h"
#include "WorldCollisionResolver.h"

namespace K4E = ::Ken4lowEngine;

class EnemyParticleEffectSystem;

/// 通常敵の固有AI・死亡演出を保持しつつ、HP・人型表示・ColliderはCharacter共通Componentへ委譲する基底。
class EnemyBase : public K4E::HumanoidCharacterActor
{
public:
	using BodyPart = K4E::HumanoidCharacterActor::BodyPart;
	using PartIndices = K4E::HumanoidCharacterActor::PartIndices;

	EnemyBase();
	~EnemyBase() override = default;

	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;
	virtual void DrawImGui();
	virtual void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);
	void DrawShadow() override;
	virtual void ApplyDirectorDifficulty(float moveSpeedMultiplier, float attackCooldownMultiplier, float damageMultiplier);

	void SetMaxHp(int value);
	void SetCurrentHp(int v);
	int GetHp() const;
	int GetMaxHp() const;
	bool IsDead() const { return isDead_; }

	K4E::Vector3 GetHpBarWorldPosition() const
	{
		K4E::Vector3 pos = GetCenterPosition();
		pos.y += 3.0f;
		return pos;
	}

	float GetHpRate() const;
	K4E::CharacterHealthComponent* GetCharacterHealthComponent() { return GetHealthComponent(); }
	const K4E::CharacterHealthComponent* GetCharacterHealthComponent() const { return GetHealthComponent(); }
	bool IsHpBarVisibleTarget() const { return !isDead_; }
	bool IsRemovable() const { return removable_; }

	void SetPosition(const K4E::Vector3& p);
	void SetVelocity(const K4E::Vector3& v) { velocity_ = v; }
	const K4E::Vector3& GetVelocity() const { return velocity_; }
	void SetCenterPosition(const K4E::Vector3& pos) override;
	void SetOrientation(const K4E::Vector3& rot) override;

	virtual void TakeDamage(int amount);
	virtual void TakeDamage(int amount, const K4E::Vector3& hitDir, float hitPower);
	virtual void SetColor(const K4E::Vector4& color);

	void EnableHitFlash(bool enable) { hitFlashEnabled_ = enable; }
	void SetHitFlashDuration(float sec) { hitFlashDuration_ = sec; }
	void SetHitFlashFrequency(float hz) { hitFlashFrequencyHz_ = hz; }
	void SetHitFlashColor(const K4E::Vector4& c) { hitFlashColor_ = c; }
	void StartHitFlash();

	void OnCollisionEnter(K4E::Collider* other) override;
	void OnCollisionStay(K4E::Collider* other) override { OnCollisionEnter(other); }
	void OnCollisionExit(K4E::Collider* other) override { (void)other; }

	static void SetGlobalStageWorldAABBs(const std::vector<K4E::AABB>* aabbs);
	static void SetGlobalStageFloorAABBs(const std::vector<K4E::AABB>* aabbs);
	static void SetGlobalStageNavigationObstacleAABBs(const std::vector<K4E::AABB>* aabbs);
	static constexpr float GetMaxUpdateDeltaTime() { return kMaxUpdateDeltaTime; }
	static constexpr bool IsGroundSnapEnabled() { return true; }
	static constexpr bool IsWorldBoundsEnabled() { return true; }
	static constexpr float GetMaxPushOutPerFrame() { return kMaxPushOutPerFrame; }
	static float GetSpawnYOffset() { return s_spawnYOffset_; }
	static void SetSpawnYOffset(float offset);
	static bool IsDeathExplosionEnabled() { return s_deathExplosionEnabled_; }
	static void SetDeathExplosionEnabled(bool enabled);
	static float GetDeathExplodePower() { return s_deathExplodePower_; }
	static void SetDeathExplodePower(float power);
	static float GetDeathUpwardPower() { return s_deathUpwardPower_; }
	static void SetDeathUpwardPower(float power);
	static float GetDeathMaxSpeed() { return s_deathMaxSpeed_; }
	static void SetDeathMaxSpeed(float speed);
	static float GetDeathMaxAngularSpeed() { return s_deathMaxAngularSpeed_; }
	static void SetDeathMaxAngularSpeed(float speed);
	static float GetDeathPieceLifetime() { return s_deathPieceLifetime_; }
	static void SetDeathPieceLifetime(float lifetime);
	int GetStuckDetectionCount() const { return stuckDetectionCount_; }
	int GetStuckRecoveryCount() const { return stuckRecoveryCount_; }

	BodyPart& GetBody() { return K4E::HumanoidCharacterActor::GetBody(); }
	const BodyPart& GetBody() const { return K4E::HumanoidCharacterActor::GetBody(); }
	std::vector<BodyPart>& GetBodyParts() { return parts_; }
	const std::vector<BodyPart>& GetBodyParts() const { return parts_; }
	const PartIndices& GetPartIndices() const { return partIndices_; }

	void SetParticleEffectSystem(EnemyParticleEffectSystem* effectSystem) { particleEffectSystem_ = effectSystem; }
	void SpawnHitEffectAt(const K4E::Vector3& worldPos);
	static void SetDeathDebugComparePositions(const K4E::Vector3& playerPosition, const K4E::Vector3& attackCenter);

	const std::vector<K4E::AABB>* GetResolvedWorldAABBs() const { return worldAABBs_ ? worldAABBs_ : g_worldAABBs_; }
	const std::vector<K4E::AABB>* GetResolvedNavigationObstacleAABBs() const { return g_navigationObstacleAABBs_ ? g_navigationObstacleAABBs_ : GetResolvedWorldAABBs(); }

protected:
	virtual void OnKilled();
	virtual void OnBulletHit(K4E::Collider* bulletCollider);

	/// 旧個別生成は行わず、HumanoidVisualComponentへEnemyスキンを設定する。
	void InitializeHumanoidVisual();
	void UpdateVisualHierarchy();
	void SetVisualColorAll(const K4E::Vector4& color);
	void MoveVisualFar(const K4E::Vector3& pos);
	K4E::Vector3 CorrectSpawnPosition(const K4E::Vector3& requestedPosition) const;
	float FindGroundY(const K4E::Vector3& position) const;
	bool OverlapsNavigationObstacle(const K4E::Vector3& center) const;

private:
	void DisableColliderOnly();
	void UpdateHitFlash(float dt);

	struct DeathPiece
	{
		BodyPart* part = nullptr;
		K4E::Vector3 velocity{ 0, 0, 0 };
		K4E::Vector3 angularVel{ 0, 0, 0 };
		float hitBias = 0.5f;
	};

	void StartBreakApartDeath(const K4E::Vector3& deathOrigin, const K4E::Vector3& deathRotation);
	void UpdateBreakApartDeath(float dt);
	void DetachAllPartsToWorldSpace();
	void CaptureDeathEffectOrigin(const K4E::Vector3& deathOrigin, const K4E::Vector3& deathRotation);
	K4E::Vector3 ResolveDeathOrigin(const K4E::Vector3& requestedOrigin);
	void CaptureDeathPartWorldTransforms();
	K4E::Vector3 RotateLocalOffsetByDeathRotation(const K4E::Vector3& localOffset) const;
	K4E::Vector3 BuildDeathPartWorldPosition(const K4E::Vector3& localOffset) const;

protected:
	std::vector<BodyPart>& parts_; // 子部位はHumanoidVisualComponent所有配列への参照だけを保持する。
	PartIndices partIndices_{};
	K4E::Vector3 orientation_{ 0.0f, 0.0f, 0.0f };

	static constexpr float kMaxUpdateDeltaTime = 1.0f / 30.0f;
	static constexpr float kGroundY = 0.0f;
	static constexpr float kMaxPushOutPerFrame = 0.45f;
	static constexpr int kStuckRecoveryThreshold = 45;
	static constexpr float kWorldBoundsMinX = -100.0f;
	static constexpr float kWorldBoundsMaxX = 100.0f;
	static constexpr float kWorldBoundsMinZ = -100.0f;
	static constexpr float kWorldBoundsMaxZ = 100.0f;

	int configuredMaxHp_ = 240; // Initialize前に派生Enemyが設定する最大HPだけを保持し、現在HPは共通Healthが所有する。
	bool isDead_ = false;
	bool removable_ = false;

	K4E::Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
	bool useGravity_ = false;
	float gravity_ = 19.6f;
	K4E::Vector3 obbHalf_{ 1.0f, 2.0f, 1.0f };

	K4E::Vector4 baseColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
	K4E::Vector4 hitFlashColor_{ 1.0f, 0.0f, 0.0f, 1.0f };
	float hitFlashTimer_ = 0.0f;
	float hitFlashDuration_ = 0.12f;
	float hitFlashFrequencyHz_ = 18.0f;
	bool hitFlashEnabled_ = true;

	const std::vector<K4E::AABB>* worldAABBs_ = nullptr;
	K4E::WorldCollisionSettings worldCol_{};
	bool worldColOverride_ = false;
	bool useWorldResolve_ = true;
	bool grounded_ = false;
	K4E::Vector3 spawnPosition_{};
	K4E::Vector3 lastSafePosition_{};
	int consecutivePushOutFrames_ = 0;
	int stuckDetectionCount_ = 0;
	int stuckRecoveryCount_ = 0;

	K4E::Vector3 lastHitDir_{ 0.0f, 0.0f, 0.0f };
	float lastHitPower_ = 1.0f;
	float lastHitUpPower_ = 2.0f;

	bool deathBreakActive_ = false;
	bool deathBreakInitialized_ = false;
	bool hasDeathEffectOrigin_ = false;
	bool hasDeathPartWorldTransforms_ = false;
	bool deathUsesMidRangeSuicideCollapseStyle_ = true;
	int deathEffectInitializeCount_ = 0;
	K4E::Vector3 deathEnemyPosition_{};
	K4E::Vector3 deathEffectOrigin_{};
	K4E::Vector3 deathEffectRotation_{};
	K4E::Vector3 deathDebugPlayerPosition_{};
	K4E::Vector3 deathDebugAttackCenter_{};
	K4E::Vector3 deathInitialBodyPosition_{};
	K4E::Vector3 deathInitialBodyRotation_{};
	std::vector<K4E::Vector3> deathInitialPartPositions_{};
	std::vector<K4E::Vector3> deathInitialPartRotations_{};
	std::vector<K4E::Vector3> deathInitialPartLocalOffsets_{};
	K4E::Vector3 deathDrawBodyPosition_{};
	std::vector<K4E::Vector3> deathDrawPartPositions_{};
	float deathTimer_ = 0.0f;
	float deathSimDuration_ = 1.8f;
	float deathFadeDuration_ = 0.6f;
	float deathLinearDamping_ = 2.0f;
	float deathAngularDamping_ = 2.5f;
	float deathBounce_ = 0.25f;
	float deathFriction_ = 0.7f;
	float deathGroundY_ = 0.0f;
	float deathMaxMovePerFrame_ = 0.25f;
	std::vector<DeathPiece> deathPieces_;

	static float s_spawnYOffset_;
	static bool s_deathExplosionEnabled_;
	static float s_deathExplodePower_;
	static float s_deathUpwardPower_;
	static float s_deathMaxSpeed_;
	static float s_deathMaxAngularSpeed_;
	static float s_deathPieceLifetime_;
	static K4E::Vector3 s_lastDebugPlayerPosition_;
	static K4E::Vector3 s_lastDebugAttackCenter_;

	EnemyParticleEffectSystem* particleEffectSystem_ = nullptr;

private:
	static const std::vector<K4E::AABB>* g_worldAABBs_;
	static const std::vector<K4E::AABB>* g_floorAABBs_;
	static const std::vector<K4E::AABB>* g_navigationObstacleAABBs_;
};
