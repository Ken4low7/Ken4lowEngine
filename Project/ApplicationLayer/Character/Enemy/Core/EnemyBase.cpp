#define NOMINMAX
#include "EnemyBase.h"

#include "EnemyParticleEffectSystem.h"
#include <Bullet.h>
#include <CollisionPreset.h>
#include <CollisionTypeIdDef.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

const std::vector<AABB>* EnemyBase::g_worldAABBs_ = nullptr;
const std::vector<AABB>* EnemyBase::g_floorAABBs_ = nullptr;
const std::vector<AABB>* EnemyBase::g_navigationObstacleAABBs_ = nullptr;
float EnemyBase::s_spawnYOffset_ = 0.15f;
bool EnemyBase::s_deathExplosionEnabled_ = true;
float EnemyBase::s_deathExplodePower_ = 3.0f;
float EnemyBase::s_deathUpwardPower_ = 1.4f;
float EnemyBase::s_deathMaxSpeed_ = 7.0f;
float EnemyBase::s_deathMaxAngularSpeed_ = 5.0f;
float EnemyBase::s_deathPieceLifetime_ = 1.8f;
Vector3 EnemyBase::s_lastDebugPlayerPosition_{};
Vector3 EnemyBase::s_lastDebugAttackCenter_{};

namespace
{
	float Clamp01(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	float Length(const Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	}

	Vector3 NormalizeSafe(const Vector3& value, const Vector3& fallback = { 0.0f, 1.0f, 0.0f })
	{
		const float length = Length(value);
		return length > 1.0e-6f ? value * (1.0f / length) : fallback;
	}

	float Rand01()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	float RandRange(float minimum, float maximum)
	{
		return minimum + (maximum - minimum) * Rand01();
	}

	Vector3 RandomUnit()
	{
		return NormalizeSafe({ RandRange(-1.0f, 1.0f), RandRange(-1.0f, 1.0f), RandRange(-1.0f, 1.0f) });
	}

	Vector3 ClampVectorLength(const Vector3& value, float maxLength)
	{
		const float safeMax = std::max(0.0f, maxLength);
		const float length = Length(value);
		return (length <= safeMax || length < 1.0e-6f) ? value : value * (safeMax / length);
	}

	bool IsNearlyZeroVector(const Vector3& value)
	{
		return Length(value) < 1.0e-4f;
	}
}

EnemyBase::EnemyBase()
	: parts_(K4E::HumanoidCharacterActor::GetBodyParts())
{
}

void EnemyBase::InitializeHumanoidVisual()
{
	if (auto* visual = GetHumanoidVisualComponent())
	{
		visual->ApplySkinToAllParts("Characters/enemy.dds"); // 敵専用スキンだけを共通人型Componentへ設定する。
	}
}

void EnemyBase::UpdateVisualHierarchy()
{
	if (isDead_ && deathBreakActive_) return;

	K4E::HumanoidCharacterActor::SetOrientation(orientation_);
	K4E::HumanoidCharacterActor::Update(0.0f); // 個別Object3D更新ではなく共通Component階層を即時同期する。
}

void EnemyBase::SetVisualColorAll(const Vector4& color)
{
	BodyPart& body = GetBody();
	if (body.object) body.object->SetColor(color);
	for (BodyPart& part : parts_)
	{
		if (part.object) part.object->SetColor(color);
	}
}

void EnemyBase::MoveVisualFar(const Vector3& position)
{
	SetCenterPosition(position);
}

void EnemyBase::Initialize()
{
	K4E::HumanoidCharacterActor::Initialize();
	InitializeHumanoidVisual();

	ApplyCollisionPreset(*this, ECollisionPresetId::Enemy);
	if (auto* collider = GetColliderComponent())
	{
		collider->SetShapeType(ECollisionShapeType::OBB);
		collider->SetHalfSize(obbHalf_);
	}
	if (Collider* primitive = GetCollisionPrimitive())
	{
		primitive->SetOwner(this); // Collision通知のOwnerは実際のEnemy Actorへ統一する。
	}

	SetColor(baseColor_);
	hitFlashTimer_ = 0.0f;
	SetCenterPosition({ 0.0f, 0.0f, 0.0f });

	isDead_ = false;
	removable_ = false;
	if (auto* health = GetHealthComponent()) health->ResetHealth(static_cast<float>(configuredMaxHp_));

	deathBreakActive_ = false;
	deathBreakInitialized_ = false;
	hasDeathEffectOrigin_ = false;
	hasDeathPartWorldTransforms_ = false;
	deathUsesMidRangeSuicideCollapseStyle_ = true;
	deathEffectInitializeCount_ = 0;
	deathEnemyPosition_ = GetCenterPosition();
	deathEffectOrigin_ = deathEnemyPosition_;
	deathEffectRotation_ = orientation_;
	deathDebugPlayerPosition_ = s_lastDebugPlayerPosition_;
	deathDebugAttackCenter_ = s_lastDebugAttackCenter_;
	deathInitialBodyPosition_ = deathEffectOrigin_;
	deathInitialBodyRotation_ = deathEffectRotation_;
	deathInitialPartPositions_.clear();
	deathInitialPartRotations_.clear();
	deathInitialPartLocalOffsets_.clear();
	deathDrawBodyPosition_ = deathEffectOrigin_;
	deathDrawPartPositions_.clear();
	deathTimer_ = 0.0f;
	deathPieces_.clear();
	lastHitDir_ = {};
	lastHitPower_ = 1.0f;

	useGravity_ = true;
	worldCol_.half = obbHalf_;
	worldCol_.centerOffset = {};
	worldColOverride_ = true;
	spawnPosition_ = CorrectSpawnPosition(GetCenterPosition());
	lastSafePosition_ = spawnPosition_;
	consecutivePushOutFrames_ = 0;
	stuckDetectionCount_ = 0;
	stuckRecoveryCount_ = 0;
}

void EnemyBase::ApplyDirectorDifficulty(float moveSpeedMultiplier, float attackCooldownMultiplier, float damageMultiplier)
{
	(void)moveSpeedMultiplier;
	(void)attackCooldownMultiplier;
	(void)damageMultiplier;
}

void EnemyBase::SetCenterPosition(const Vector3& position)
{
	K4E::HumanoidCharacterActor::SetCenterPosition(position);
	if (!deathBreakActive_) K4E::HumanoidCharacterActor::Update(0.0f);
}

void EnemyBase::SetPosition(const Vector3& position)
{
	spawnPosition_ = CorrectSpawnPosition(position);
	lastSafePosition_ = spawnPosition_;
	SetCenterPosition(spawnPosition_);
}

float EnemyBase::FindGroundY(const Vector3& position) const
{
	float groundY = kGroundY;
	if (!g_floorAABBs_) return groundY;
	for (const AABB& floor : *g_floorAABBs_)
	{
		if (position.x >= floor.min.x - obbHalf_.x && position.x <= floor.max.x + obbHalf_.x &&
			position.z >= floor.min.z - obbHalf_.z && position.z <= floor.max.z + obbHalf_.z &&
			floor.max.y <= position.y + obbHalf_.y + 0.5f)
		{
			groundY = std::max(groundY, floor.max.y);
		}
	}
	return groundY;
}

bool EnemyBase::OverlapsNavigationObstacle(const Vector3& center) const
{
	const auto* obstacles = GetResolvedNavigationObstacleAABBs();
	if (!obstacles) return false;
	for (const AABB& obstacle : *obstacles)
	{
		if (center.x + obbHalf_.x > obstacle.min.x && center.x - obbHalf_.x < obstacle.max.x &&
			center.y + obbHalf_.y > obstacle.min.y && center.y - obbHalf_.y < obstacle.max.y &&
			center.z + obbHalf_.z > obstacle.min.z && center.z - obbHalf_.z < obstacle.max.z)
		{
			return true;
		}
	}
	return false;
}

Vector3 EnemyBase::CorrectSpawnPosition(const Vector3& requestedPosition) const
{
	const Vector3 offsets[] = {
		{}, { 2.5f, 0.0f, 0.0f }, { -2.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 2.5f }, { 0.0f, 0.0f, -2.5f },
		{ 3.5f, 0.0f, 3.5f }, { -3.5f, 0.0f, 3.5f }, { 3.5f, 0.0f, -3.5f }, { -3.5f, 0.0f, -3.5f }
	};
	for (const Vector3& offset : offsets)
	{
		Vector3 candidate = requestedPosition + offset;
		candidate.y = FindGroundY(candidate) + obbHalf_.y + s_spawnYOffset_;
		if (!OverlapsNavigationObstacle(candidate)) return candidate;
	}
	Vector3 fallback = requestedPosition;
	fallback.y = FindGroundY(fallback) + obbHalf_.y + s_spawnYOffset_;
	return fallback;
}

void EnemyBase::SetOrientation(const Vector3& rotation)
{
	orientation_ = rotation;
	if (!deathBreakActive_) K4E::HumanoidCharacterActor::SetOrientation(rotation);
}

void EnemyBase::SetColor(const Vector4& color)
{
	baseColor_ = color;
	SetVisualColorAll(color);
}

void EnemyBase::Update(float deltaTime)
{
	if (removable_) return;
	deltaTime = std::clamp(deltaTime, 0.0f, kMaxUpdateDeltaTime);

	if (isDead_)
	{
		UpdateBreakApartDeath(deltaTime);
		return;
	}

	grounded_ = false;
	if (useGravity_) velocity_.y -= gravity_ * deltaTime;

	const Vector3 oldPosition = GetCenterPosition();
	Vector3 newPosition = oldPosition + velocity_ * deltaTime;
	const auto* aabbs = worldAABBs_ ? worldAABBs_ : g_worldAABBs_;

	if (useWorldResolve_ && aabbs && !aabbs->empty())
	{
		float velocityY = velocity_.y;
		const auto result = WorldCollisionResolver::Resolve(*aabbs, worldCol_, oldPosition, newPosition, true, &velocityY);
		const Vector3 desiredCenter = newPosition - worldCol_.centerOffset;
		if (std::fabs(result.fixedCenter.x - desiredCenter.x) > 0.0001f) velocity_.x = 0.0f;
		if (std::fabs(result.fixedCenter.z - desiredCenter.z) > 0.0001f) velocity_.z = 0.0f;
		velocity_.y = velocityY;
		grounded_ = result.grounded;

		const Vector3 resolvedPosition = result.fixedCenter + worldCol_.centerOffset;
		Vector3 pushOut = resolvedPosition - newPosition;
		pushOut.y = 0.0f;
		const float pushOutLength = std::sqrt(pushOut.x * pushOut.x + pushOut.z * pushOut.z);
		if (pushOutLength > kMaxPushOutPerFrame)
		{
			const float scale = kMaxPushOutPerFrame / pushOutLength;
			pushOut.x *= scale;
			pushOut.z *= scale;
		}
		newPosition.x += pushOut.x;
		newPosition.z += pushOut.z;
		newPosition.y = resolvedPosition.y;

		if (pushOutLength > 0.0001f)
		{
			++consecutivePushOutFrames_;
			++stuckDetectionCount_;
			velocity_.x = 0.0f;
			velocity_.z = 0.0f;
		}
		else
		{
			consecutivePushOutFrames_ = 0;
			lastSafePosition_ = newPosition;
		}
	}

	if (consecutivePushOutFrames_ >= kStuckRecoveryThreshold)
	{
		newPosition = !OverlapsNavigationObstacle(lastSafePosition_) ? lastSafePosition_ : spawnPosition_;
		velocity_ = {};
		consecutivePushOutFrames_ = 0;
		++stuckRecoveryCount_;
	}

	const float minimumCenterY = kGroundY + obbHalf_.y;
	if (newPosition.y < minimumCenterY)
	{
		newPosition.y = minimumCenterY;
		velocity_.y = 0.0f;
		grounded_ = true;
	}
	newPosition.x = std::clamp(newPosition.x, kWorldBoundsMinX, kWorldBoundsMaxX);
	newPosition.z = std::clamp(newPosition.z, kWorldBoundsMinZ, kWorldBoundsMaxZ);

	SetCenterPosition(newPosition);
	UpdateHitFlash(deltaTime);
	K4E::HumanoidCharacterActor::Update(deltaTime); // 人型表示・Collider・共通Componentを同じActor更新経路で進める。
}

void EnemyBase::Draw()
{
	if (removable_ || (isDead_ && !deathBreakActive_)) return;
	if (GetBody().object) deathDrawBodyPosition_ = GetBody().object->GetTranslate();
	if (deathDrawPartPositions_.size() != parts_.size()) deathDrawPartPositions_.assign(parts_.size(), {});
	for (size_t index = 0; index < parts_.size(); ++index)
	{
		if (parts_[index].object) deathDrawPartPositions_[index] = parts_[index].object->GetTranslate();
	}
	K4E::HumanoidCharacterActor::Draw(); // Object3D単位の独自Draw列挙を廃止し、Component描画へ統一する。
}

void EnemyBase::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Text("敵死亡座標: %.2f, %.2f, %.2f", deathEnemyPosition_.x, deathEnemyPosition_.y, deathEnemyPosition_.z);
	ImGui::Text("死亡演出原点: %.2f, %.2f, %.2f", deathEffectOrigin_.x, deathEffectOrigin_.y, deathEffectOrigin_.z);
	ImGui::Text("死亡演出初期化回数: %d", deathEffectInitializeCount_);
	ImGui::Text("HP: %d / %d", GetHp(), GetMaxHp());
	ImGui::Checkbox("死亡部位 爆散有効", &s_deathExplosionEnabled_);
	ImGui::SliderFloat("死亡部位 爆散力", &s_deathExplodePower_, 0.0f, 8.0f, "%.2f");
	ImGui::SliderFloat("死亡部位 上方向力", &s_deathUpwardPower_, 0.0f, 5.0f, "%.2f");
	ImGui::SliderFloat("死亡部位 最大速度", &s_deathMaxSpeed_, 0.5f, 12.0f, "%.2f");
	ImGui::SliderFloat("死亡部位 最大回転速度", &s_deathMaxAngularSpeed_, 0.5f, 10.0f, "%.2f");
	ImGui::SliderFloat("死亡部位 寿命", &s_deathPieceLifetime_, 0.2f, 5.0f, "%.2f 秒");
#endif
}

void EnemyBase::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
{
	K4E::HumanoidCharacterActor::UpdateShadowMatrix(lightViewProjection);
}

void EnemyBase::DrawShadow()
{
	if (removable_ || (isDead_ && !deathBreakActive_)) return;
	K4E::HumanoidCharacterActor::DrawShadow();
}

void EnemyBase::SetCurrentHp(int value)
{
	if (auto* health = GetHealthComponent()) health->SetCurrentHealth(static_cast<float>(value));
}

void EnemyBase::SetMaxHp(int value)
{
	configuredMaxHp_ = std::max(1, value);
	if (auto* health = GetHealthComponent()) health->ResetHealth(static_cast<float>(configuredMaxHp_));
}

int EnemyBase::GetHp() const
{
	const auto* health = GetHealthComponent();
	return health ? static_cast<int>(health->GetCurrentHealth()) : configuredMaxHp_;
}

int EnemyBase::GetMaxHp() const
{
	const auto* health = GetHealthComponent();
	return health ? static_cast<int>(health->GetMaxHealth()) : configuredMaxHp_;
}

float EnemyBase::GetHpRate() const
{
	const auto* health = GetHealthComponent();
	return health ? health->GetHealthRatio() : 1.0f;
}

void EnemyBase::TakeDamage(int amount)
{
	TakeDamage(amount, {}, 50.0f);
}

void EnemyBase::TakeDamage(int amount, const Vector3& hitDirection, float hitPower)
{
	if (isDead_ || amount <= 0) return;
	if (Length(hitDirection) > 1.0e-4f) lastHitDir_ = NormalizeSafe(hitDirection, { 0.0f, 0.0f, 1.0f });
	lastHitPower_ = hitPower > 0.0f ? hitPower : 1.0f;

	CharacterDamageInfo damageInfo{};
	damageInfo.amount = static_cast<float>(amount);
	const CharacterDamageResult result = K4E::CharacterActor::ApplyDamage(damageInfo);
	if (!result.accepted) return;
	StartHitFlash();

	if (result.killed)
	{
		const Vector3 deathOrigin = GetCenterPosition();
		const Vector3 deathRotation = orientation_;
		CaptureDeathEffectOrigin(deathOrigin, deathRotation);
		isDead_ = true;
		removable_ = false;
		OnKilled();
		DisableColliderOnly();
	}
}

void EnemyBase::SetGlobalStageWorldAABBs(const std::vector<AABB>* aabbs) { g_worldAABBs_ = aabbs; }
void EnemyBase::SetGlobalStageFloorAABBs(const std::vector<AABB>* aabbs) { g_floorAABBs_ = aabbs; }
void EnemyBase::SetGlobalStageNavigationObstacleAABBs(const std::vector<AABB>* aabbs) { g_navigationObstacleAABBs_ = aabbs; }
void EnemyBase::SetSpawnYOffset(float offset) { s_spawnYOffset_ = std::clamp(offset, 0.0f, 0.5f); }
void EnemyBase::SetDeathExplosionEnabled(bool enabled) { s_deathExplosionEnabled_ = enabled; }
void EnemyBase::SetDeathExplodePower(float power) { s_deathExplodePower_ = std::clamp(power, 0.0f, 12.0f); }
void EnemyBase::SetDeathUpwardPower(float power) { s_deathUpwardPower_ = std::clamp(power, 0.0f, 8.0f); }
void EnemyBase::SetDeathMaxSpeed(float speed) { s_deathMaxSpeed_ = std::clamp(speed, 0.5f, 20.0f); }
void EnemyBase::SetDeathMaxAngularSpeed(float speed) { s_deathMaxAngularSpeed_ = std::clamp(speed, 0.5f, 20.0f); }
void EnemyBase::SetDeathPieceLifetime(float lifetime) { s_deathPieceLifetime_ = std::clamp(lifetime, 0.2f, 5.0f); }
void EnemyBase::SetDeathDebugComparePositions(const Vector3& playerPosition, const Vector3& attackCenter)
{
	s_lastDebugPlayerPosition_ = playerPosition;
	s_lastDebugAttackCenter_ = attackCenter;
}

void EnemyBase::SpawnHitEffectAt(const Vector3& worldPosition)
{
	if (isDead_ || removable_ || !particleEffectSystem_ || !particleEffectSystem_->IsInitialized()) return;
	particleEffectSystem_->SpawnHitEffect(worldPosition);
}

void EnemyBase::OnKilled()
{
	CaptureDeathEffectOrigin(deathEffectOrigin_, deathEffectRotation_);
	if (particleEffectSystem_ && particleEffectSystem_->IsInitialized()) particleEffectSystem_->SpawnDeathEffect(deathEffectOrigin_);
	StartBreakApartDeath(deathEffectOrigin_, deathEffectRotation_);
}

void EnemyBase::DisableColliderOnly()
{
	if (auto* collider = GetColliderComponent()) collider->SetActive(false);
	if (Collider* primitive = GetCollisionPrimitive()) primitive->SetEnabled(false); // 登録解除までの同フレーム衝突を止める。
}

void EnemyBase::StartHitFlash()
{
	if (hitFlashEnabled_) hitFlashTimer_ = hitFlashDuration_;
}

void EnemyBase::UpdateHitFlash(float deltaTime)
{
	if (hitFlashTimer_ > 0.0f)
	{
		hitFlashTimer_ = std::max(0.0f, hitFlashTimer_ - deltaTime);
		const float ratio = hitFlashDuration_ > 0.0f ? hitFlashTimer_ / hitFlashDuration_ : 0.0f;
		const float elapsed = hitFlashDuration_ - hitFlashTimer_;
		const float blink = 0.5f * (1.0f + std::sin(elapsed * hitFlashFrequencyHz_ * 6.28318530718f));
		const float amount = blink * ratio;
		Vector4 color{};
		color.x = baseColor_.x + (hitFlashColor_.x - baseColor_.x) * amount;
		color.y = baseColor_.y + (hitFlashColor_.y - baseColor_.y) * amount;
		color.z = baseColor_.z + (hitFlashColor_.z - baseColor_.z) * amount;
		color.w = baseColor_.w + (hitFlashColor_.w - baseColor_.w) * amount;
		SetVisualColorAll(color);
	}
	else
	{
		SetVisualColorAll(baseColor_);
	}
}

void EnemyBase::DetachAllPartsToWorldSpace()
{
	UpdateVisualHierarchy();
	BodyPart& body = GetBody();
	body.transform.parent_ = nullptr;
	body.transform.Update();
	for (BodyPart& part : parts_)
	{
		part.transform.translate_ = part.transform.worldTranslate_;
		part.transform.rotate_ = part.transform.worldRotate_;
		part.transform.parent_ = nullptr;
		part.transform.Update();
	}
}

Vector3 EnemyBase::ResolveDeathOrigin(const Vector3& requestedOrigin)
{
	if (!IsNearlyZeroVector(requestedOrigin)) return requestedOrigin;
	UpdateVisualHierarchy();
	if (!IsNearlyZeroVector(GetBody().transform.worldTranslate_)) return GetBody().transform.worldTranslate_;
	if (!IsNearlyZeroVector(GetCenterPosition())) return GetCenterPosition();
	if (!IsNearlyZeroVector(lastSafePosition_)) return lastSafePosition_;
	return spawnPosition_;
}

void EnemyBase::CaptureDeathEffectOrigin(const Vector3& deathOrigin, const Vector3& deathRotation)
{
	if (hasDeathEffectOrigin_) return;
	deathEnemyPosition_ = ResolveDeathOrigin(deathOrigin);
	deathEffectOrigin_ = deathEnemyPosition_;
	deathEffectRotation_ = deathRotation;
	deathDebugPlayerPosition_ = s_lastDebugPlayerPosition_;
	deathDebugAttackCenter_ = s_lastDebugAttackCenter_;
	CaptureDeathPartWorldTransforms();
	hasDeathEffectOrigin_ = true;
}

void EnemyBase::CaptureDeathPartWorldTransforms()
{
	if (hasDeathPartWorldTransforms_) return;
	UpdateVisualHierarchy();
	BodyPart& body = GetBody();
	deathInitialBodyPosition_ = body.transform.worldTranslate_;
	deathInitialBodyRotation_ = body.transform.worldRotate_;
	deathInitialPartPositions_.clear();
	deathInitialPartRotations_.clear();
	deathInitialPartLocalOffsets_.clear();
	for (BodyPart& part : parts_)
	{
		const Vector3 localOffset = part.transform.parent_ ? part.transform.translate_ : part.transform.translate_ - deathEffectOrigin_;
		deathInitialPartLocalOffsets_.push_back(localOffset);
		deathInitialPartPositions_.push_back(BuildDeathPartWorldPosition(localOffset));
		deathInitialPartRotations_.push_back(deathEffectRotation_ + part.transform.rotate_);
	}
	hasDeathPartWorldTransforms_ = true;
}

Vector3 EnemyBase::RotateLocalOffsetByDeathRotation(const Vector3& localOffset) const
{
	return Matrix4x4::Transform(localOffset, Matrix4x4::MakeRotateMatrix(deathEffectRotation_));
}

Vector3 EnemyBase::BuildDeathPartWorldPosition(const Vector3& localOffset) const
{
	return deathEffectOrigin_ + RotateLocalOffsetByDeathRotation(localOffset);
}

void EnemyBase::StartBreakApartDeath(const Vector3& deathOrigin, const Vector3& deathRotation)
{
	if (deathBreakInitialized_) return;
	CaptureDeathEffectOrigin(deathOrigin, deathRotation);
	if (!hasDeathPartWorldTransforms_) CaptureDeathPartWorldTransforms();

	deathPieces_.clear();
	deathBreakActive_ = true;
	deathBreakInitialized_ = true;
	++deathEffectInitializeCount_;
	deathSimDuration_ = s_deathPieceLifetime_;
	deathTimer_ = deathSimDuration_;

	BodyPart& body = GetBody();
	body.transform.parent_ = nullptr;
	body.transform.translate_ = deathEffectOrigin_;
	body.transform.rotate_ = deathInitialBodyRotation_;
	body.transform.Update();
	if (body.object)
	{
		body.object->SetTranslate(body.transform.translate_);
		body.object->SetRotate(body.transform.rotate_);
		body.object->Update();
	}

	for (size_t index = 0; index < parts_.size(); ++index)
	{
		BodyPart& part = parts_[index];
		part.transform.parent_ = nullptr;
		const Vector3 localOffset = index < deathInitialPartLocalOffsets_.size() ? deathInitialPartLocalOffsets_[index] : part.transform.translate_;
		part.transform.translate_ = BuildDeathPartWorldPosition(localOffset);
		part.transform.rotate_ = index < deathInitialPartRotations_.size() ? deathInitialPartRotations_[index] : deathEffectRotation_;
		part.transform.Update();
		if (part.object)
		{
			part.object->SetTranslate(part.transform.translate_);
			part.object->SetRotate(part.transform.rotate_);
			part.object->Update();
		}
	}

	SetVisualColorAll(baseColor_);
	const Vector3 center = deathEffectOrigin_;
	deathGroundY_ = FindGroundY(center) + 0.05f;

	auto makePiece = [this, &center](BodyPart* part, float outwardScale, float upwardScale, float angularScale)
	{
		DeathPiece piece{};
		piece.part = part;
		if (!part) return piece;
		const Vector3 outward = NormalizeSafe(part->transform.translate_ - center, { 0.0f, 0.0f, 1.0f });
		Vector3 jitter = RandomUnit();
		jitter.y = 0.0f;
		jitter = NormalizeSafe(jitter, { outward.x, 0.0f, outward.z });
		const Vector3 direction = NormalizeSafe(outward * 0.9f + jitter * 0.1f, outward);
		const float enabledScale = s_deathExplosionEnabled_ ? 1.0f : 0.15f;
		const float speed = s_deathExplodePower_ * outwardScale * enabledScale * RandRange(0.85f, 1.15f);
		const float lift = s_deathUpwardPower_ * upwardScale * enabledScale * RandRange(0.85f, 1.15f);
		piece.velocity = ClampVectorLength(direction * speed + Vector3{ 0.0f, lift, 0.0f }, s_deathMaxSpeed_);
		piece.angularVel = ClampVectorLength({ RandRange(-1.1f, 1.1f) * angularScale, RandRange(-1.4f, 1.4f) * angularScale, RandRange(-1.1f, 1.1f) * angularScale }, s_deathMaxAngularSpeed_);
		return piece;
	};

	deathPieces_.push_back(makePiece(&body, 0.20f, 0.25f, 0.35f));
	for (size_t index = 0; index < parts_.size(); ++index)
	{
		if (index == partIndices_.head) deathPieces_.push_back(makePiece(&parts_[index], 0.85f, 1.35f, 0.70f));
		else if (index == partIndices_.leftArm || index == partIndices_.rightArm) deathPieces_.push_back(makePiece(&parts_[index], 1.15f, 0.85f, 1.00f));
		else deathPieces_.push_back(makePiece(&parts_[index], 0.85f, 0.55f, 0.80f));
	}
}

void EnemyBase::UpdateBreakApartDeath(float deltaTime)
{
	deltaTime = std::clamp(deltaTime, 0.0f, kMaxUpdateDeltaTime);
	if (!deathBreakActive_) StartBreakApartDeath(GetCenterPosition(), orientation_);
	deathTimer_ = std::max(0.0f, deathTimer_ - deltaTime);

	float alpha = 1.0f;
	if (deathTimer_ <= deathFadeDuration_) alpha = Clamp01(deathTimer_ / deathFadeDuration_);
	Vector4 color = baseColor_;
	color.w *= alpha;
	SetVisualColorAll(color);

	for (DeathPiece& piece : deathPieces_)
	{
		if (!piece.part || !piece.part->object) continue;
		piece.velocity.y -= gravity_ * deltaTime;
		piece.velocity = ClampVectorLength(piece.velocity, s_deathMaxSpeed_);
		piece.angularVel = ClampVectorLength(piece.angularVel, s_deathMaxAngularSpeed_);
		piece.velocity = ClampVectorLength(piece.velocity * std::max(0.0f, 1.0f - deathLinearDamping_ * deltaTime), s_deathMaxSpeed_);
		piece.angularVel = ClampVectorLength(piece.angularVel * std::max(0.0f, 1.0f - deathAngularDamping_ * deltaTime), s_deathMaxAngularSpeed_);

		Vector3 frameMove = ClampVectorLength(piece.velocity * deltaTime, deathMaxMovePerFrame_);
		piece.part->transform.translate_ = piece.part->transform.translate_ + frameMove;
		piece.part->transform.rotate_ = piece.part->transform.rotate_ + piece.angularVel * deltaTime;
		if (piece.part->transform.translate_.y < deathGroundY_)
		{
			piece.part->transform.translate_.y = deathGroundY_;
			if (piece.velocity.y < 0.0f)
			{
				piece.velocity.y = -piece.velocity.y * deathBounce_;
				piece.velocity.x *= deathFriction_;
				piece.velocity.z *= deathFriction_;
			}
		}
		piece.part->transform.parent_ = nullptr;
		piece.part->transform.Update();
		piece.part->object->SetTranslate(piece.part->transform.translate_);
		piece.part->object->SetRotate(piece.part->transform.rotate_);
		piece.part->object->Update();
	}

	if (deathTimer_ <= 0.0f)
	{
		removable_ = true;
		deathBreakActive_ = false;
	}
}

void EnemyBase::OnBulletHit(Collider* bulletCollider)
{
	Vector3 hitDirection{};
	float hitPower = 1.0f;
	int damage = 10;
	Vector3 hitPosition = GetCenterPosition();
	hitPosition.y += 1.0f;

	if (bulletCollider)
	{
		hitPosition = bulletCollider->GetCenterPosition();
		const Segment segment = bulletCollider->GetSegment();
		hitDirection = Length(segment.diff) > 1.0e-4f ? segment.diff : GetCenterPosition() - bulletCollider->GetCenterPosition();

		if (auto* bullet = bulletCollider->GetOwner<Bullet>())
		{
			damage = std::max(1, bullet->GetDamage());
			const Vector3 bulletDirection = NormalizeSafe(bullet->GetMoveVelocity(), NormalizeSafe(hitDirection, { 0.0f, 0.0f, 1.0f }));
			const Vector3 attackerDirection = NormalizeSafe(GetCenterPosition() - bullet->GetShooterPosition(), bulletDirection);
			switch (bullet->GetDeathKnockbackType())
			{
			case EDeathKnockbackType::Sniper:
				hitDirection = bulletDirection;
				hitPower = bullet->GetDeathKnockbackPower() * bullet->GetDeathImpulseScale();
				lastHitUpPower_ = std::max(0.3f, bullet->GetDeathKnockbackUpPower());
				break;
			case EDeathKnockbackType::Heavy:
				hitDirection = NormalizeSafe(bulletDirection + Vector3{ 0.0f, 0.35f, 0.0f }, attackerDirection);
				hitPower = bullet->GetDeathKnockbackPower() * bullet->GetDeathImpulseScale();
				lastHitUpPower_ = bullet->GetDeathKnockbackUpPower();
				break;
			case EDeathKnockbackType::Explosion:
				hitDirection = NormalizeSafe(NormalizeSafe(GetCenterPosition() - bulletCollider->GetCenterPosition(), attackerDirection) + Vector3{ 0.0f, 0.55f, 0.0f }, attackerDirection);
				hitPower = bullet->GetDeathKnockbackPower() * bullet->GetDeathImpulseScale();
				lastHitUpPower_ = std::max(2.0f, bullet->GetDeathKnockbackUpPower());
				break;
			case EDeathKnockbackType::Light:
			case EDeathKnockbackType::Default:
			default:
				hitDirection = attackerDirection;
				hitPower = bullet->GetDeathKnockbackPower() * bullet->GetDeathImpulseScale();
				lastHitUpPower_ = bullet->GetDeathKnockbackUpPower();
				break;
			}
		}
	}

	SpawnHitEffectAt(hitPosition);
	TakeDamage(damage, hitDirection, hitPower);
}

void EnemyBase::OnCollisionEnter(Collider* other)
{
	if (!other || isDead_) return;
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		if (auto* bullet = other->GetOwner<Bullet>(); bullet && bullet->UsesPhysicsTrigger()) return;
		OnBulletHit(other);
	}
}
