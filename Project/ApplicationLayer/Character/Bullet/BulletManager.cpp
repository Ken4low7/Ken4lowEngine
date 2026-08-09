#define NOMINMAX
#include "BulletManager.h"
#include "Bullet.h"
#include "CollisionManager.h"
#include "Engine/Physics/Core/PhysicsWorld.h"
#include "BulletEnemyCollisionSoA.h"
#include "GpuParticleManager.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace Ken4lowEngine;

namespace
{
	constexpr uint32_t kMuzzleSparkMeshId = 1000u;
	constexpr uint32_t kMuzzleSparkBurstCount = 8u;
	constexpr const char* kMuzzleSparkEmitterName = "MuzzleSparkMesh";
	constexpr const char* kMuzzleSparkMeshModelPath = "Sample/cube.gltf";

	constexpr uint32_t kBulletTracerEmitterBankCount = 32u;
	constexpr uint32_t kBulletTracerMaxParticlesPerEmitter = 4u;
	constexpr float kBulletTracerStartOffset = 0.08f;
	constexpr float kBulletTracerWidth = 0.035f;
	constexpr float kBulletTracerLifeTime = 0.075f;
	constexpr float kBulletTracerDirectionSpeed = 0.001f;
	constexpr const char* kBulletTracerEmitterBaseName = "BulletTracerRibbon_";
	constexpr const char* kBulletTracerTexturePath = "Effects/white.dds";

	float Length(const Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	}

	std::string MakeMeshTexturePath(uint32_t meshId)
	{
		return "Mesh:" + std::to_string(meshId);
	}

	bool EnsureMeshAssetRegistered(GpuParticleManager* particle, uint32_t meshId, const char* modelPath)
	{
		if (!particle) return false;
		if (particle->FindMeshAsset(meshId)) return true;
		return particle->LoadMeshAssetsFromAssimp(meshId, modelPath, true);
	}

	GpuParticleEmitter* GetOrCreateMeshEmitter(
		GpuParticleManager* particle,
		const char* name,
		uint32_t meshId,
		GpuParticleType particleType,
		const char* meshModelPath)
	{
		if (!EnsureMeshAssetRegistered(particle, meshId, meshModelPath)) return nullptr;
		if (GpuParticleEmitter* emitter = particle->GetEmitter(name)) return emitter;

		GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = MakeMeshTexturePath(meshId);
		info.radius = 0.0f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.drawType = 0;
		info.kind = GpuParticleKind::Mesh;
		info.spriteType = particleType;
		info.billboardFlags = BillboardMode::None;
		return particle->CreateEmitter(name, info);
	}

	GpuParticleEmitter* GetOrCreateBulletTracerRibbonEmitter(GpuParticleManager* particle, const char* name)
	{
		if (!particle) return nullptr;
		if (GpuParticleEmitter* emitter = particle->GetEmitter(name)) return emitter;

		GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = kBulletTracerTexturePath;
		info.radius = 0.0f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.drawType = 0;
		info.kind = GpuParticleKind::Ribbon;
		info.ribbonType = GpuRibbonType::Trail;
		info.billboardFlags = BillboardMode::Camera;
		info.useDescSpawnOverride = true;
		info.maxParticles = kBulletTracerMaxParticlesPerEmitter;
		info.positionRandom = {};
		info.velocity = { 0.0f, kBulletTracerDirectionSpeed, 0.0f };
		info.velocityRandom = {};
		info.startSize = { kBulletTracerWidth, 1.0f };
		info.endSize = { kBulletTracerWidth * 0.35f, 1.0f };
		info.startColor = { 0.82f, 0.92f, 1.0f, 0.95f };
		info.endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
		info.lifeTime = kBulletTracerLifeTime;
		info.lifeTimeRandom = 0.0f;
		info.gravity = {};
		info.damping = 0.0f;
		info.speed = 0.0f;
		info.speedRandom = 0.0f;
		info.sizeRandom = 0.0f;
		info.alphaFade = true;
		return particle->CreateEmitter(name, info);
	}

	void EmitMuzzleSparkMesh(GpuParticleManager* particle, const Vector3& muzzlePosition)
	{
		GpuParticleEmitter* emitter = GetOrCreateMeshEmitter(
			particle,
			kMuzzleSparkEmitterName,
			kMuzzleSparkMeshId,
			GpuParticleType::Spark,
			kMuzzleSparkMeshModelPath);
		if (!emitter) return;

		emitter->SetPosition(muzzlePosition);
		emitter->RequestEmit(kMuzzleSparkBurstCount);
	}

	void EmitBulletTracerRibbon(GpuParticleManager* particle, const Vector3& muzzlePosition, const Vector3& tracerEndPosition)
	{
		if (!particle) return;

		const Vector3 fullTracerVector = tracerEndPosition - muzzlePosition;
		const float fullTracerLength = Length(fullTracerVector);
		if (fullTracerLength <= 1.0e-4f) return;

		const Vector3 forward = fullTracerVector * (1.0f / fullTracerLength);
		const float startOffset = std::min(kBulletTracerStartOffset, fullTracerLength * 0.5f);
		const Vector3 tracerStartPosition = muzzlePosition + forward * startOffset;
		const Vector3 drawableVector = tracerEndPosition - tracerStartPosition;
		const float drawableLength = Length(drawableVector);
		if (drawableLength <= 1.0e-4f) return;

		static uint32_t tracerEmitterBank = 0u;
		const uint32_t currentBank = tracerEmitterBank;
		tracerEmitterBank = (tracerEmitterBank + 1u) % kBulletTracerEmitterBankCount;
		const std::string emitterName = std::string(kBulletTracerEmitterBaseName) + std::to_string(currentBank);

		GpuParticleEmitter* emitter = GetOrCreateBulletTracerRibbonEmitter(particle, emitterName.c_str());
		if (!emitter) return;

		auto& info = emitter->GetInfoMutable();
		info.velocity = forward * kBulletTracerDirectionSpeed;
		info.startSize = { kBulletTracerWidth, drawableLength };
		info.endSize = { kBulletTracerWidth * 0.35f, drawableLength };

		// 1枚の擬似Ribbonを始点と着弾点の中点へ置き、長手方向を射線へ合わせる。
		emitter->SetPosition((tracerStartPosition + tracerEndPosition) * 0.5f);
		emitter->RequestEmit(1u);
	}

	void EmitPlayerWeaponFireEffects(const Vector3& muzzlePosition, const Vector3& tracerEndPosition)
	{
		EffectSystem::GetInstance()->Play("MuzzleFlash", muzzlePosition);

		GpuParticleManager* particle = GpuParticleManager::GetInstance();
		if (!particle) return;
		EmitMuzzleSparkMesh(particle, muzzlePosition);
		EmitBulletTracerRibbon(particle, muzzlePosition, tracerEndPosition);
	}
}

void BulletManager::Initialize(CollisionManager* collisionManager)
{
	collisionManager_ = collisionManager;
	shotEffectTransformResolver_ = {};
	bullets_.clear();
}

Bullet* BulletManager::Spawn(const Vector3& startPos,
	const Vector3& dirNormalized,
	float speed,
	int damage,
	float lifeTimeSec,
	const Ken4lowEngine::Vector3& shooterPosition,
	uint32_t shooterColliderId,
	uint32_t typeId,
	const WeaponParams& weaponParams
)
{
	auto b = std::make_unique<Bullet>();
	b->Initialize(startPos, dirNormalized * speed, damage, lifeTimeSec, shooterPosition, shooterColliderId, typeId);
	b->SetModelDrawEnabled(weaponParams.drawProjectileModel);
	b->SetCollisionManager(collisionManager_);
	b->SetWorldImpactCallback(worldImpactCallback_);
	b->ConfigureSplashDamage(weaponParams);
	b->SetWeaponMetadata(weaponParams);
	b->SetUsePhysicsTrigger(usePhysicsTriggerForNormalBullets_);
	if (b->UsesPhysicsTrigger())
	{
		// 通常弾のColliderをPhysicsWorld Trigger判定へ登録する。Legacy側には残すが命中処理はBullet側でスキップする。
		b->SetCollisionLayer(playerBulletLayer_);
		if (physicsWorld_)
		{
			physicsWorld_->RegisterCollider(b.get());
		}
	}

	if (collisionManager_) collisionManager_->AddCollider(b.get());

	Bullet* raw = b.get();
	bullets_.push_back(std::move(b));

	if (typeId == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		const Vector3 fallbackForward = Vector3::Normalize(dirNormalized);
		const float maxTracerDistance = std::max(0.0f, speed) * std::max(0.0f, lifeTimeSec);
		Vector3 tracerEndPosition = startPos + fallbackForward * maxTracerDistance;

		if (collisionManager_ && maxTracerDistance > 0.0f)
		{
			RaycastQuery tracerQuery{};
			tracerQuery.origin = startPos;
			tracerQuery.direction = fallbackForward;
			tracerQuery.maxDistance = maxTracerDistance;
			tracerQuery.traceChannel = ETraceChannel::Weapon;

			RaycastHit tracerHit{};
			if (collisionManager_->RaycastSingle(tracerQuery, tracerHit) && tracerHit.hit)
			{
				tracerEndPosition = tracerHit.point;
			}
		}

		Vector3 effectPosition = startPos + fallbackForward * 0.35f;
		Vector3 effectDirection = fallbackForward;
		if (shotEffectTransformResolver_)
		{
			Vector3 resolvedPosition{};
			Vector3 resolvedDirection{};
			if (shotEffectTransformResolver_(resolvedPosition, resolvedDirection))
			{
				effectPosition = resolvedPosition;
				effectDirection = Vector3::Normalize(resolvedDirection); // ViewModelの銃口座標を使い、カメラ中心の顔位置からVFXを出さない。
			}
		}
		if (maxTracerDistance <= 0.0f)
		{
			tracerEndPosition = effectPosition + effectDirection;
		}
		EmitPlayerWeaponFireEffects(effectPosition, tracerEndPosition);
	}
	return raw;
}

void BulletManager::SetWorldImpactCallback(std::function<void(const Vector3&, const Vector3&)> callback)
{
	worldImpactCallback_ = std::move(callback);
	// すでに飛翔中の弾にも新しい通知先を反映し、Scene再接続時の古い参照を残さない。
	for (auto& bullet : bullets_)
	{
		if (bullet)
		{
			bullet->SetWorldImpactCallback(worldImpactCallback_);
		}
	}
}

void BulletManager::Update(float dt)
{
	for (auto& b : bullets_) b->Update(dt);

	// 寿命切れ/衝突済みの弾はCollider解除後に管理対象から外し、Update/Draw負荷を残さない。
	const auto removeBegin = std::remove_if(bullets_.begin(), bullets_.end(), [this](const std::unique_ptr<Bullet>& bullet)
		{
			if (!bullet || !bullet->IsRemovable())
			{
				return false;
			}

			if (bullet->UsesPhysicsTrigger() && bullet->HasPhysicsHit())
			{
				++physicsTriggerHitCount_;
			}
			if (physicsWorld_ && bullet->UsesPhysicsTrigger())
			{
				// 破棄済みBullet Collider参照を防ぐため、管理対象から外す前にPhysicsWorld登録を解除する。
				physicsWorld_->UnregisterCollider(bullet.get());
			}
			if (collisionManager_) collisionManager_->RemoveCollider(bullet.get());
			return true;
		});

	bullets_.erase(removeBegin, bullets_.end());
}

size_t BulletManager::GetActiveCount() const
{
	return static_cast<size_t>(std::count_if(bullets_.begin(), bullets_.end(), [](const std::unique_ptr<Bullet>& bullet)
		{
			return bullet && !bullet->IsDead() && !bullet->IsRemovable();
		}));
}

size_t BulletManager::GetPhysicsTriggerBulletCount() const
{
	return static_cast<size_t>(std::count_if(bullets_.begin(), bullets_.end(), [](const std::unique_ptr<Bullet>& bullet)
		{
			return bullet && bullet->UsesPhysicsTrigger() && !bullet->IsRemovable();
		}));
}

void BulletManager::AppendCollisionSoABullets(BulletEnemyCollisionSoA& collisionSoA) const
{
	for (const auto& bullet : bullets_)
	{
		if (!bullet || bullet->IsDead() || bullet->IsRemovable())
		{
			continue;
		}

		// 描画や寿命管理はBullet側に残し、判定に必要な位置・半径・ダメージだけをSoAへ転送する。
		collisionSoA.AddBullet(
			bullet->GetCenterPosition(),
			bullet->GetCollisionRadius(),
			bullet->GetDamage(),
			true);
	}
}

void BulletManager::SetPhysicsTriggerWorld(PhysicsWorld* physicsWorld, uint32_t playerBulletLayer)
{
	physicsWorld_ = physicsWorld;
	playerBulletLayer_ = playerBulletLayer;
	RefreshPhysicsTriggerRegistrations();
}

void BulletManager::SetUsePhysicsTriggerForNormalBullets(bool enabled)
{
	usePhysicsTriggerForNormalBullets_ = enabled;
	RefreshPhysicsTriggerRegistrations();
}

void BulletManager::RefreshPhysicsTriggerRegistrations()
{
	for (auto& bullet : bullets_)
	{
		if (!bullet)
		{
			continue;
		}

		const bool wasUsingPhysics = bullet->UsesPhysicsTrigger();
		if (wasUsingPhysics && physicsWorld_)
		{
			physicsWorld_->UnregisterCollider(bullet.get());
		}

		bullet->SetUsePhysicsTrigger(usePhysicsTriggerForNormalBullets_);
		if (bullet->UsesPhysicsTrigger())
		{
			bullet->SetCollisionLayer(playerBulletLayer_);
			if (physicsWorld_ && !bullet->IsRemovable())
			{
				physicsWorld_->RegisterCollider(bullet.get());
			}
		}
	}
}

void BulletManager::Draw()
{
	for (auto& b : bullets_) b->Draw();
}

void BulletManager::DrawImGui()
{
	for (auto& b : bullets_) b->DrawImGui();
}

void BulletManager::Clear()
{
	if (collisionManager_)
	{
		for (auto& b : bullets_) collisionManager_->RemoveCollider(b.get());
	}
	if (physicsWorld_)
	{
		for (auto& b : bullets_)
		{
			if (b && b->UsesPhysicsTrigger())
			{
				// 破棄済みBullet Collider参照を防ぐため、Scene終了/一括Clear時にPhysicsWorld登録を解除する。
				physicsWorld_->UnregisterCollider(b.get());
			}
		}
	}
	bullets_.clear();
}
