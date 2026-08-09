#define NOMINMAX
#include "EnemySpawnCrystal.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "CharacterWorld.h"
#include "EnemyBase.h"
#include "Bullet.h"
#include "CollisionTypes.h"
#include "CollisionTypeIdDef.h"
#include "AudioManager.h"
#include "GpuParticleManager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace Ken4lowEngine;

float EnemySpawnCrystal::s_spawnYOffset_ = 0.15f;

namespace
{
	constexpr float kTwoPi = 6.28318530718f;
	constexpr float kMaximumBreakingDuration = 0.65f;

	bool OverlapsObstacle(const Vector3& center, const Vector3& half, const std::vector<AABB>* obstacles)
	{
		if (!obstacles) { return false; }
		for (const AABB& obstacle : *obstacles)
		{
			if (center.x + half.x > obstacle.min.x && center.x - half.x < obstacle.max.x &&
				center.y + half.y > obstacle.min.y && center.y - half.y < obstacle.max.y &&
				center.z + half.z > obstacle.min.z && center.z - half.z < obstacle.max.z) {
				return true;
			}
		}
		return false;
	}

	Vector3 SnapCrystalPosition(const Vector3& requested, const Vector3& scale, const std::vector<AABB>* floors, const std::vector<AABB>* obstacles, float spawnYOffset)
	{
		const Vector3 half{ std::fabs(scale.x) * 0.5f, std::fabs(scale.y) * 0.5f, std::fabs(scale.z) * 0.5f };
		const Vector3 offsets[] = { {}, { 2.0f, 0.0f, 0.0f }, { -2.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 2.0f }, { 0.0f, 0.0f, -2.0f } };
		for (const Vector3& offset : offsets)
		{
			Vector3 candidate = requested + offset;
			float groundY = 0.0f;
			if (floors)
			{
				for (const AABB& floor : *floors)
				{
					if (candidate.x >= floor.min.x - half.x && candidate.x <= floor.max.x + half.x &&
						candidate.z >= floor.min.z - half.z && candidate.z <= floor.max.z + half.z && floor.max.y <= requested.y + half.y + 0.5f)
					{
						groundY = std::max(groundY, floor.max.y);
					}
				}
			}
			// Cube仮モデルは中心基準なので、半高さとY補正を足して足元を地面へ載せる。
			candidate.y = groundY + half.y + spawnYOffset;
			if (!OverlapsObstacle(candidate, half, obstacles)) { return candidate; }
		}
		Vector3 fallback = requested;
		float fallbackGroundY = 0.0f;
		if (floors)
		{
			for (const AABB& floor : *floors)
			{
				if (fallback.x >= floor.min.x - half.x && fallback.x <= floor.max.x + half.x &&
					fallback.z >= floor.min.z - half.z && fallback.z <= floor.max.z + half.z && floor.max.y <= requested.y + half.y + 0.5f)
				{
					fallbackGroundY = std::max(fallbackGroundY, floor.max.y);
				}
			}
		}
		fallback.y = fallbackGroundY + half.y + spawnYOffset;
		return fallback;
	}

	float RandomRange(float minValue, float maxValue)
	{
		const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}

	float Clamp01(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	float Length(const Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	}

	float MaxAbsComponent(const Vector3& value)
	{
		return std::max({ std::fabs(value.x), std::fabs(value.y), std::fabs(value.z) });
	}

	float ResolveBreakingDuration(const CrystalReactionSettings& settings)
	{
		return std::clamp(settings.breakingDuration, 0.05f, kMaximumBreakingDuration);
	}

	Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
	{
		t = Clamp01(t);
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t
		};
	}
}

void EnemySpawnCrystal::Initialize(const CrystalSpawnPoint& spawnPoint, const std::vector<AABB>* floorAABBs, const std::vector<AABB>* obstacleAABBs)
{
	isAlive = true;
	state_ = State::Normal;
	justBroken_ = false;
	hitFlashTimer_ = 0.0f;
	hitShakeTimer_ = 0.0f;
	breakingTimer_ = 0.0f;
	totalSpawnedCount = 0;
	aliveSpawnedEnemyCount = 0;
	spawnedEnemies_.clear();
	hitCount_ = 0;
	guideHighlightAlpha_ = 0.0f;
	guideHighlightTimer_ = 0.0f;
	visibilityTimer_ = 0.0f;
	visibilityParticleTimer_ = 0.0f;
	playerInsideApproachRange_ = false;
	ResetSpawnRuntime();

	ApplyInitialHpSettings(spawnPoint);
	ApplySpawnerSettings(spawnPoint, floorAABBs, obstacleAABBs);

	// Crystal専用Presetはまだ無いため、TypeID/ObjectChannel互換を保ったまま必要なResponseだけ明示する。
	SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kCrystal));
	SetCollisionPreset("Crystal");
	SetObjectChannel(EObjectChannel::Crystal);
	SetEnabled(true);
	SetQueryEnabled(true);
	SetPhysicsEnabled(true);
	SetTrigger(false);
	ResetCollisionResponses(static_cast<uint8_t>(ECollisionResponse::Ignore));
	SetCollisionResponseId(static_cast<uint32_t>(EObjectChannel::PlayerProjectile), static_cast<uint8_t>(ECollisionResponse::Block));
	SetOwner(this);

	debugCube_ = std::make_unique<Object3D>();
	debugCube_->Initialize("Sample/cube.gltf");
	debugCube_->SetPbrEnabled(true);
	debugCube_->SetMetallic(0.18f);
	debugCube_->SetRoughness(0.16f);
	debugCube_->SetReflectivity(0.65f);
	debugCube_->SetFrustumCullingEnabled(false);
	debugCube_->SetColor({ 0.20f, 0.82f, 1.0f, 1.0f });
	debugCube_->SetEmissiveFactor({ 0.15f, 1.4f, 2.5f, 1.0f });
	SyncTransformToRuntime(floorAABBs, obstacleAABBs);
}

void EnemySpawnCrystal::Update(const CharacterWorld& characters, float deltaTime, const CrystalReactionSettings& reactionSettings)
{
	reactionSettings_ = reactionSettings;
	const float safeDeltaTime = std::max(0.0f, deltaTime);
	visibilityTimer_ += safeDeltaTime;
	guideHighlightTimer_ += safeDeltaTime;
	RemoveInactiveSpawnedEnemies(characters);
	UpdateReactionTimers(safeDeltaTime, reactionSettings);
	UpdateStateFromHpRate(reactionSettings); // HP割合からNormal/Damaged/Criticalへ状態を切り替える。
	UpdateVisibilityParticles(safeDeltaTime, reactionSettings);
	UpdateApproachSound(reactionSettings);

	if (debugCube_)
	{
		Vector3 visualRotation = rotation_;
		visualRotation.y += visibilityTimer_ * 0.42f;
		// 常時回転と発光を加え、戦闘中でも背景へ埋もれない破壊対象として見せる。
		debugCube_->SetTranslate(BuildVisualPosition(reactionSettings));
		debugCube_->SetRotate(visualRotation);
		debugCube_->SetScale(BuildVisualScale(reactionSettings));
		debugCube_->SetColor(BuildVisualColor(reactionSettings));
		debugCube_->SetEmissiveFactor(BuildEmissiveColor(reactionSettings));
		debugCube_->Update();
	}
}

void EnemySpawnCrystal::Draw() const
{
	if (IsAlive() && debugCube_)
	{
		debugCube_->Draw();
	}
}

void EnemySpawnCrystal::ApplyDamage(int damage)
{
	if (!IsAlive() || IsBreaking() || damage <= 0)
	{
		return;
	}

	// クリスタルの残りHPを確認できるよう、HP割合を表示用に公開する。
	++hitCount_;
	hp = std::max(0, hp - damage);
	BeginHitReaction(); // ダメージを受けた時の点滅・揺れ・SEリアクションを開始する。
	if (hp <= 0)
	{
		BeginBreaking(reactionSettings_); // HP0からBreaking状態へ移行し、即消滅を避ける。
	}
}

void EnemySpawnCrystal::OnCollisionEnter(K4E::Collider* other)
{
	if (!IsColliderEnabled() || !other || other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		return;
	}

	if (auto* bullet = other->GetOwner<Bullet>())
	{
		ApplyDamage(bullet->GetDamage());
	}
}

bool EnemySpawnCrystal::CanSpawnEnemy() const
{
	const bool underTotalLimit = (maxSpawnCount_ <= 0) || (totalSpawnedCount < maxSpawnCount_);
	// Breaking/Broken中は破壊済み扱いとして、このクリスタルからのスポーンを停止する。
	// 累計上限は任意設定だけに使い、通常ステージ1では同時生存数で継続スポーンを制御する。
	return isActive_ && isAlive && state_ != State::Breaking && state_ != State::Broken && enableInfiniteSpawn && underTotalLimit && aliveSpawnedEnemyCount < maxAliveEnemies;
}

void EnemySpawnCrystal::SetMaxAliveEnemies(int count)
{
	maxAliveEnemies = std::max(0, count);
}

void EnemySpawnCrystal::SetSpawnYOffset(float offset)
{
	s_spawnYOffset_ = std::clamp(offset, 0.0f, 0.5f);
}

void EnemySpawnCrystal::SetSpawnInterval(float interval)
{
	spawnInterval_ = std::max(0.05f, interval);
}

void EnemySpawnCrystal::ApplySpawnerSettings(const CrystalSpawnPoint& spawnPoint, const std::vector<AABB>* floorAABBs, const std::vector<AABB>* obstacleAABBs)
{
	crystalName_ = spawnPoint.crystalName;
	isActive_ = spawnPoint.isActive;
	rotation_ = spawnPoint.rotation;
	scale_ = spawnPoint.scale;
	spawnEnemyType = spawnPoint.spawnEnemyType;
	maxHp = std::max(1, spawnPoint.maxHp);
	hp = std::clamp(hp, 0, maxHp);
	spawnInterval_ = std::max(0.05f, spawnPoint.spawnInterval);
	initialDelay_ = std::max(0.0f, spawnPoint.initialDelay);
	maxSpawnCount_ = std::max(0, spawnPoint.maxSpawnCount);
	spawnRadius = std::max(0.0f, spawnPoint.spawnRadius);
	maxAliveEnemies = std::max(0, spawnPoint.maxAliveEnemies);
	spawnPattern_ = spawnPoint.spawnPattern;
	enableInfiniteSpawn = spawnPoint.enableInfiniteSpawn;
	spawnBossTrigger_ = spawnPoint.spawnBossTrigger;

	position_ = spawnPoint.position;
	SyncTransformToRuntime(floorAABBs, obstacleAABBs); // ParameterManagerのTransform値を描画・当たり判定・スポーン基準へ同期する。
}

void EnemySpawnCrystal::ApplyInitialHpSettings(const CrystalSpawnPoint& spawnPoint)
{
	maxHp = std::max(1, spawnPoint.maxHp);
	hp = std::clamp(spawnPoint.hp, 1, maxHp);
}

void EnemySpawnCrystal::SetGuideHighlight(float alpha)
{
	guideHighlightAlpha_ = Clamp01(alpha);
}

void EnemySpawnCrystal::AdvanceSpawnTimer(float deltaTime)
{
	if (!CanSpawnEnemy())
	{
		return;
	}

	spawnTimer_ += deltaTime;
}

bool EnemySpawnCrystal::IsSpawnReady() const
{
	if (!CanSpawnEnemy())
	{
		return false;
	}

	if (!initialDelayElapsed_)
	{
		return spawnTimer_ >= initialDelay_;
	}

	if (spawnPattern_ == "Single" || spawnPattern_ == "Burst")
	{
		return totalSpawnedCount == 0;
	}

	return spawnTimer_ >= spawnInterval_;
}

void EnemySpawnCrystal::ConsumeSpawnTimer()
{
	if (!initialDelayElapsed_)
	{
		initialDelayElapsed_ = true;
	}

	spawnTimer_ = 0.0f;
}

void EnemySpawnCrystal::ResetSpawnRuntime()
{
	spawnTimer_ = 0.0f;
	initialDelayElapsed_ = false;
}

void EnemySpawnCrystal::RemoveInactiveSpawnedEnemies(const CharacterWorld& characters)
{
	const std::vector<EnemyBase*> livingEnemies = characters.GetEnemyRawList();
	spawnedEnemies_.erase(
		std::remove_if(spawnedEnemies_.begin(), spawnedEnemies_.end(),
			[&livingEnemies](const EnemyBase* spawnedEnemy)
			{
				const auto it = std::find(livingEnemies.begin(), livingEnemies.end(), spawnedEnemy);
				return it == livingEnemies.end() || (*it)->IsDead();
			}),
		spawnedEnemies_.end());
	aliveSpawnedEnemyCount = static_cast<int>(spawnedEnemies_.size());
}

EnemyBase* EnemySpawnCrystal::SpawnEnemy(CharacterWorld& characters, float moveSpeedMultiplier, float attackCooldownMultiplier, float damageMultiplier)
{
	if (!CanSpawnEnemy())
	{
		return nullptr;
	}

	const float angle = RandomRange(0.0f, kTwoPi);
	const float distance = RandomRange(0.0f, spawnRadius);
	const Vector3 spawnPosition{
		position_.x + std::cos(angle) * distance,
		position_.y,
		position_.z + std::sin(angle) * distance
	};

	// CharacterWorld 内部で EnemyFactory を経由し、設定された雑魚敵派生を生成する。
	EnemyBase& enemy = characters.SpawnEnemyAt(spawnPosition, spawnEnemyType);
	enemy.ApplyDirectorDifficulty(moveSpeedMultiplier, attackCooldownMultiplier, damageMultiplier);
	spawnedEnemies_.push_back(&enemy);
	aliveSpawnedEnemyCount = static_cast<int>(spawnedEnemies_.size());
	++totalSpawnedCount;
	return &enemy;
}

void EnemySpawnCrystal::SyncTransformToRuntime(const std::vector<AABB>* floorAABBs, const std::vector<AABB>* obstacleAABBs)
{
	position_ = SnapCrystalPosition(position_, scale_, floorAABBs, obstacleAABBs, s_spawnYOffset_);
	SetCenterPosition(IsColliderEnabled() ? position_ : Vector3{ 1.0e9f, 1.0e9f, 1.0e9f });
	SetOBBHalfSize(scale_);
	SetOrientation(rotation_);

	if (debugCube_)
	{
		debugCube_->SetTranslate(BuildVisualPosition(reactionSettings_));
		debugCube_->SetRotate(rotation_);
		debugCube_->SetScale(scale_);
		debugCube_->Update();
	}
}

void EnemySpawnCrystal::BeginHitReaction()
{
	hitFlashTimer_ = std::max(hitFlashTimer_, reactionSettings_.hitFlashTime);
	hitShakeTimer_ = std::max(hitShakeTimer_, reactionSettings_.hitShakeTime);
	Ken4lowEngine::AudioManager::GetInstance()->PlaySE(GetHitSoundName(), 0.16f);
}

void EnemySpawnCrystal::BeginBreaking(const CrystalReactionSettings& reactionSettings)
{
	if (state_ == State::Breaking || state_ == State::Broken)
	{
		return;
	}

	const float breakingDuration = ResolveBreakingDuration(reactionSettings);
	state_ = State::Breaking;
	isAlive = true;
	enableInfiniteSpawn = false;
	breakingTimer_ = 0.0f;
	hitFlashTimer_ = std::max(hitFlashTimer_, reactionSettings.hitFlashTime);
	hitShakeTimer_ = std::max(hitShakeTimer_, breakingDuration);
	SetEnabled(false);
	SetQueryEnabled(false);
	SetPhysicsEnabled(false);
	SetCenterPosition({ 1.0e9f, 1.0e9f, 1.0e9f }); // 見た目を残したまま破壊開始フレームから弾・移動の衝突対象を外す。
	Ken4lowEngine::AudioManager::GetInstance()->PlaySE(GetBreakSoundName(), 0.35f, 0.85f);
}

void EnemySpawnCrystal::UpdateStateFromHpRate(const CrystalReactionSettings& reactionSettings)
{
	if (state_ == State::Breaking || state_ == State::Broken)
	{
		return;
	}

	const float hpRate = GetHpRate();
	if (hpRate <= reactionSettings.criticalHpRate)
	{
		state_ = State::Critical;
	}
	else if (hpRate <= reactionSettings.damagedHpRate)
	{
		state_ = State::Damaged;
	}
	else
	{
		state_ = State::Normal;
	}
}

void EnemySpawnCrystal::UpdateReactionTimers(float deltaTime, const CrystalReactionSettings& reactionSettings)
{
	hitFlashTimer_ = std::max(0.0f, hitFlashTimer_ - deltaTime);
	hitShakeTimer_ = std::max(0.0f, hitShakeTimer_ - deltaTime);

	if (state_ != State::Breaking)
	{
		return;
	}

	breakingTimer_ += deltaTime;
	if (breakingTimer_ >= ResolveBreakingDuration(reactionSettings))
	{
		state_ = State::Broken;
		isAlive = false;
		SetEnabled(false);
		SetQueryEnabled(false);
		SetPhysicsEnabled(false);
		justBroken_ = true;
		playerInsideApproachRange_ = false;
		SetCenterPosition({ 1.0e9f, 1.0e9f, 1.0e9f });
	}
}

void EnemySpawnCrystal::UpdateVisibilityParticles(float deltaTime, const CrystalReactionSettings& reactionSettings)
{
	if (!IsAlive())
	{
		visibilityParticleTimer_ = 0.0f;
		return;
	}

	visibilityParticleTimer_ += deltaTime;
	const float interval = std::max(0.05f, reactionSettings.visibilityParticleInterval);
	if (visibilityParticleTimer_ < interval)
	{
		return;
	}
	visibilityParticleTimer_ = std::fmod(visibilityParticleTimer_, interval);

	GpuParticleManager* particleManager = GpuParticleManager::GetInstance();
	if (!particleManager)
	{
		return;
	}

	const std::string emitterName = BuildVisibilityEmitterName();
	GpuParticleEmitter* emitter = particleManager->GetEmitter(emitterName);
	if (!emitter)
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "Effects/white.dds";
		info.kind = GpuParticleKind::Sprite;
		info.spriteType = GpuParticleType::Ambient;
		info.billboardFlags = BillboardMode::Camera;
		info.useDescSpawnOverride = true;
		info.maxParticles = 128u;
		info.spawnShape = 1u;
		info.spawnRadius = std::max(0.75f, MaxAbsComponent(scale_) * reactionSettings.visibilityAuraRadiusScale);
		info.positionRandom = { 0.24f, 0.32f, 0.24f };
		info.velocity = { 0.0f, 0.48f, 0.0f };
		info.velocityRandom = { 0.34f, 0.22f, 0.34f };
		const float particleSizeScale = std::max(0.5f, reactionSettings.visibilityParticleSizeScale);
		info.startSize = { 0.055f * particleSizeScale, 0.055f * particleSizeScale };
		info.endSize = { 0.012f * particleSizeScale, 0.012f * particleSizeScale };
		info.startColor = { 0.25f, 0.88f, 1.0f, 0.85f };
		info.endColor = { 0.55f, 0.96f, 1.0f, 0.0f };
		info.lifeTime = 1.05f;
		info.lifeTimeRandom = 0.22f;
		info.gravity = { 0.0f, 0.10f, 0.0f };
		info.damping = 0.35f;
		info.sizeRandom = 0.30f;
		info.alphaFade = true;
		emitter = particleManager->CreateRuntimeEmitter(emitterName, info);
	}
	if (!emitter)
	{
		return;
	}

	auto& info = emitter->GetInfoMutable();
	const float normalRadiusScale = std::max(0.5f, reactionSettings.visibilityAuraRadiusScale);
	const float highlightRadiusScale = std::max(normalRadiusScale, reactionSettings.visibilityAuraHighlightScale);
	const float radiusScale = normalRadiusScale + (highlightRadiusScale - normalRadiusScale) * guideHighlightAlpha_;
	info.spawnRadius = std::max(0.75f, MaxAbsComponent(scale_) * radiusScale);
	const float particleSizeScale = std::max(0.5f, reactionSettings.visibilityParticleSizeScale);
	info.startSize = { 0.055f * particleSizeScale, 0.055f * particleSizeScale };
	info.endSize = { 0.012f * particleSizeScale, 0.012f * particleSizeScale };
	if (state_ == State::Critical || state_ == State::Breaking)
	{
		info.startColor = { 1.0f, 0.20f, 0.10f, 0.90f };
		info.endColor = { 1.0f, 0.55f, 0.12f, 0.0f };
	}
	else if (guideHighlightAlpha_ > 0.0f)
	{
		info.startColor = { 1.0f, 0.90f, 0.20f, 0.95f };
		info.endColor = { 0.35f, 0.95f, 1.0f, 0.0f };
	}
	else
	{
		info.startColor = { 0.25f, 0.88f, 1.0f, 0.85f };
		info.endColor = { 0.55f, 0.96f, 1.0f, 0.0f };
	}

	// オーラを本体より広く放出し、遠距離でも目標物の占有範囲を読み取れるようにする。
	emitter->SetPosition({ position_.x, position_.y + std::fabs(scale_.y) * 0.15f, position_.z });
	const uint32_t emitCount = guideHighlightAlpha_ > 0.0f ? 9u : ((state_ == State::Critical || state_ == State::Breaking) ? 7u : 5u);
	emitter->RequestEmit(emitCount);
}

void EnemySpawnCrystal::UpdateApproachSound(const CrystalReactionSettings& reactionSettings)
{
	if (!IsAlive())
	{
		playerInsideApproachRange_ = false;
		return;
	}

	const IPlayerRuntime* player = IPlayerRuntime::GetActiveRuntimeConst();
	if (!player)
	{
		playerInsideApproachRange_ = false;
		return;
	}

	const float enterDistance = std::max(0.0f, reactionSettings.approachSoundDistance);
	const float resetDistance = std::max(enterDistance + 1.0f, reactionSettings.approachSoundResetDistance);
	const float distance = Length(player->GetWorldPosition() - position_);
	if (!playerInsideApproachRange_ && distance <= enterDistance)
	{
		Ken4lowEngine::AudioManager::GetInstance()->PlaySE(GetApproachSoundName(), 0.28f, 1.0f);
		playerInsideApproachRange_ = true;
	}
	else if (playerInsideApproachRange_ && distance >= resetDistance)
	{
		// 接近距離と解除距離を分け、境界付近でSEが連続再生されないようにする。
		playerInsideApproachRange_ = false;
	}
}

K4E::Vector4 EnemySpawnCrystal::BuildVisualColor(const CrystalReactionSettings& reactionSettings) const
{
	const float pulse = 0.5f + 0.5f * std::sin(visibilityTimer_ * std::max(0.1f, reactionSettings.visibilityPulseSpeed) * kTwoPi);
	Vector4 color = LerpColor({ 0.12f, 0.72f, 1.0f, 1.0f }, { 0.42f, 0.96f, 1.0f, 1.0f }, 0.25f + pulse * 0.30f);
	switch (state_)
	{
	case State::Damaged:
		color = LerpColor({ 0.18f, 0.42f, 1.0f, 1.0f }, { 0.48f, 0.72f, 1.0f, 1.0f }, pulse);
		break;
	case State::Critical:
		color = LerpColor({ 0.90f, 0.08f, 0.04f, 1.0f }, { 1.0f, 0.50f, 0.08f, 1.0f }, pulse);
		break;
	case State::Breaking:
		{
			const float t = Clamp01(breakingTimer_ / ResolveBreakingDuration(reactionSettings));
			color = LerpColor({ 1.0f, 0.15f, 0.08f, 1.0f }, { 0.05f, 0.05f, 0.05f, 0.0f }, t);
			break;
		}
	case State::Broken:
		color = { 0.0f, 0.0f, 0.0f, 0.0f };
		break;
	case State::Normal:
	default:
		break;
	}

	if (hitFlashTimer_ > 0.0f)
	{
		color = LerpColor(color, { 1.0f, 1.0f, 0.65f, color.w }, 0.88f);
	}
	if (guideHighlightAlpha_ > 0.0f && state_ != State::Breaking && state_ != State::Broken)
	{
		const float highlight = guideHighlightAlpha_ * (0.55f + pulse * 0.35f);
		color = LerpColor(color, { 1.0f, 0.92f, 0.18f, color.w }, highlight);
	}

	return color;
}

K4E::Vector4 EnemySpawnCrystal::BuildEmissiveColor(const CrystalReactionSettings& reactionSettings) const
{
	const float pulse = 0.5f + 0.5f * std::sin(visibilityTimer_ * std::max(0.1f, reactionSettings.visibilityPulseSpeed) * kTwoPi);
	Vector4 emissive{ 0.12f + pulse * 0.16f, 1.25f + pulse * 0.75f, 2.15f + pulse * 1.15f, 1.0f };
	if (state_ == State::Damaged)
	{
		emissive = { 0.16f, 0.72f + pulse * 0.35f, 2.65f + pulse * 0.85f, 1.0f };
	}
	else if (state_ == State::Critical)
	{
		emissive = { 2.65f + pulse * 1.15f, 0.12f + pulse * 0.18f, 0.04f, 1.0f };
	}
	else if (state_ == State::Breaking)
	{
		const float fade = 1.0f - Clamp01(breakingTimer_ / ResolveBreakingDuration(reactionSettings));
		emissive = { (3.2f + pulse) * fade, 0.12f * fade, 0.04f * fade, 1.0f };
	}
	else if (state_ == State::Broken)
	{
		emissive = { 0.0f, 0.0f, 0.0f, 1.0f };
	}

	if (guideHighlightAlpha_ > 0.0f && state_ != State::Breaking && state_ != State::Broken)
	{
		emissive = LerpColor(emissive, { 3.2f, 2.4f, 0.12f, 1.0f }, guideHighlightAlpha_ * (0.45f + pulse * 0.35f));
	}
	if (hitFlashTimer_ > 0.0f)
	{
		emissive = LerpColor(emissive, { 4.0f, 4.0f, 2.6f, 1.0f }, 0.85f);
	}
	return emissive;
}

K4E::Vector3 EnemySpawnCrystal::BuildVisualPosition(const CrystalReactionSettings& reactionSettings) const
{
	Vector3 visualPosition = position_;
	if (hitShakeTimer_ > 0.0f || state_ == State::Breaking)
	{
		const float power = (state_ == State::Breaking) ? reactionSettings.hitShakePower * 1.6f : reactionSettings.hitShakePower;
		visualPosition.x += RandomRange(-power, power);
		visualPosition.y += RandomRange(-power * 0.5f, power * 0.5f);
		visualPosition.z += RandomRange(-power, power);
	}
	return visualPosition;
}

K4E::Vector3 EnemySpawnCrystal::BuildVisualScale(const CrystalReactionSettings& reactionSettings) const
{
	Vector3 visualScale = scale_;
	if (state_ == State::Breaking)
	{
		const float t = Clamp01(breakingTimer_ / ResolveBreakingDuration(reactionSettings));
		const float scaleBoost = 1.0f + (reactionSettings.breakEffectScale - 1.0f) * (1.0f - std::abs(t * 2.0f - 1.0f));
		visualScale = visualScale * scaleBoost;
	}
	else
	{
		const float pulse = 0.5f + 0.5f * std::sin(visibilityTimer_ * std::max(0.1f, reactionSettings.visibilityPulseSpeed) * kTwoPi);
		float scaleBoost = 1.0f + std::max(0.0f, reactionSettings.visibilityPulseScale) * (0.35f + pulse * 0.65f);
		if (guideHighlightAlpha_ > 0.0f)
		{
			// ステージ1開始案内中は通常脈動へ強調分を重ね、最初の破壊対象を明確にする。
			scaleBoost += guideHighlightAlpha_ * (0.08f + pulse * 0.10f);
		}
		visualScale = visualScale * scaleBoost;
	}
	return visualScale;
}

std::string EnemySpawnCrystal::BuildVisibilityEmitterName() const
{
	return "CrystalVisibility_" + crystalName_;
}

const char* EnemySpawnCrystal::GetHitSoundName() const
{
	return "enemy_hit.mp3";
}

const char* EnemySpawnCrystal::GetBreakSoundName() const
{
	return "enemy_death.mp3";
}

const char* EnemySpawnCrystal::GetApproachSoundName() const
{
	return "crystal_near.mp3";
}
