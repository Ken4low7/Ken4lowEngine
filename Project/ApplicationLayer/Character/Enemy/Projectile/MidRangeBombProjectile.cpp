#define NOMINMAX
#include "MidRangeBombProjectile.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Character/Player/Player.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"
#include "GpuParticleType.h"
#include "Stage.h"
#include "Wireframe.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace Ken4lowEngine;

namespace
{
    void EmitBombParticle(
        const char* emitterName,
        GpuParticleKind kind,
        GpuParticleType particleType,
        const Vector3& position,
        uint32_t count,
        float radius,
        float lifeScale,
        float speedScale)
    {
        if (!emitterName || count == 0) return;
        GpuParticleManager* manager = GpuParticleManager::GetInstance();
        if (!manager) return;

        GpuParticleEmitter::EmitterInfo info{};
        info.textureFilePath = "Effects/white.dds";
        info.radius = std::max(0.0f, radius);
        info.kind = kind;
        info.spriteType = particleType;
        info.billboardFlags = BillboardMode::Camera;
        info.lifeScale = std::max(0.01f, lifeScale);
        info.speedScale = std::max(0.0f, speedScale);

        GpuParticleEmitter* emitter = manager->GetEmitter(emitterName);
        if (!emitter) emitter = manager->CreateEmitter(emitterName, info);
        if (!emitter) return;
        emitter->GetInfoMutable() = info;
        emitter->SetPosition(position);
        emitter->RequestEmit(count);
    }
}

IPlayerRuntime* MidRangeBombProjectile::s_targetPlayerRuntime_ = nullptr;
bool MidRangeBombProjectile::s_debugCubeVisible_ = true;
float MidRangeBombProjectile::s_debugCubeSize_ = 0.4f;

void MidRangeBombProjectile::Initialize()
{
    position_ = {};
    velocity_ = {};
    explosionPosition_ = {};
    telegraphPosition_ = {};
    lifeTimer_ = 0.0f;
    explosionDrawTimer_ = 0.0f;
    telegraphTime_ = 0.0f;
    telegraphEmitTimer_ = 0.0f;
    exploded_ = false;
    alive_ = false;
    directDamageApplied_ = false;
    explosionDamageApplied_ = false;

    debugCube_ = std::make_unique<Object3D>();
    debugCube_->Initialize("Sample/cube.gltf");
    debugCube_->SetColor({ 0.48f, 0.08f, 0.02f, 1.0f });
    UpdateDebugCube();
}

void MidRangeBombProjectile::Launch(const Vector3& start, const Vector3& target, const BombProjectileSettings& settings)
{
    settings_ = settings;
    position_ = start;
    explosionPosition_ = start;
    telegraphPosition_ = target;
    telegraphPosition_.y = ResolveFloorY(target) + 0.06f;
    lifeTimer_ = 0.0f;
    explosionDrawTimer_ = 0.0f;
    telegraphTime_ = 0.0f;
    telegraphEmitTimer_ = 0.0f;
    exploded_ = false;
    alive_ = true;
    directDamageApplied_ = false;
    explosionDamageApplied_ = false;

    Vector3 directionXZ = target - start;
    directionXZ.y = 0.0f;
    const float lengthXZ = Vector3::LengthXZ(directionXZ);
    if (lengthXZ > 0.0001f) directionXZ = directionXZ / lengthXZ;
    else directionXZ = { 0.0f, 0.0f, 1.0f };

    float horizontalSpeed = settings_.initialSpeed;
    if (settings_.useDistanceBasedSpeed)
    {
        horizontalSpeed = settings_.minInitialSpeed +
            std::max(0.0f, lengthXZ - settings_.speedBaseDistance) * settings_.speedPerDistance;
    }
    horizontalSpeed = std::clamp(horizontalSpeed, std::max(0.0f, settings_.minInitialSpeed), std::max(settings_.minInitialSpeed, settings_.maxInitialSpeed));
    velocity_ = directionXZ * horizontalSpeed;
    velocity_.y = std::clamp(settings_.upwardVelocity, 0.0f, 12.0f);

    EmitBombParticle("MidRangeBombLaunchSpark", GpuParticleKind::Sprite, GpuParticleType::Spark, start, 12, 0.35f, 0.45f, 1.2f);
    UpdateDebugCube();
}

void MidRangeBombProjectile::Update(float deltaTime)
{
    constexpr float kMaxDeltaTime = 1.0f / 30.0f;
    deltaTime = std::clamp(deltaTime, 0.0f, kMaxDeltaTime);

    if (alive_)
    {
        lifeTimer_ += deltaTime;
        UpdateTelegraph(deltaTime);
        velocity_.y -= std::clamp(settings_.gravity, 0.0f, 30.0f) * deltaTime;
        position_ += velocity_ * deltaTime;
        UpdateDebugCube();

        bool shouldExplode = lifeTimer_ >= settings_.lifeTime;
        const float floorY = ResolveFloorY(position_);
        if (position_.y <= floorY + s_debugCubeSize_ * 0.5f)
        {
            position_.y = floorY + s_debugCubeSize_ * 0.5f;
            UpdateDebugCube();
            shouldExplode = true;
        }
        if (shouldExplode) Explode();
    }
    else if (exploded_)
    {
        explosionDrawTimer_ = std::max(0.0f, explosionDrawTimer_ - deltaTime);
    }

    if (s_targetPlayerRuntime_) TryApplyPlayerDamage(*s_targetPlayerRuntime_);
}

void MidRangeBombProjectile::Draw() const
{
    if (alive_)
    {
        if (s_debugCubeVisible_ && debugCube_) debugCube_->Draw();
        const float pulse = 0.93f + std::sin(telegraphTime_ * 8.0f) * 0.07f;
        const float radius = settings_.explosionRadius * pulse;
        Wireframe::GetInstance()->DrawCircle(telegraphPosition_, radius, 48, { 1.0f, 0.16f, 0.02f, 0.95f });
        Wireframe::GetInstance()->DrawCircle(telegraphPosition_ + Vector3{ 0.0f, 0.02f, 0.0f }, radius * 0.58f, 32, { 1.0f, 0.62f, 0.08f, 0.82f });
    }
    if (exploded_ && explosionDrawTimer_ > 0.0f)
    {
        Wireframe::GetInstance()->DrawSphere(explosionPosition_, settings_.explosionRadius, { 1.0f, 0.12f, 0.02f, 0.75f });
        Wireframe::GetInstance()->DrawCircle(explosionPosition_, settings_.explosionRadius, 48, { 1.0f, 0.55f, 0.08f, 0.95f });
    }
}

void MidRangeBombProjectile::Explode()
{
    if (exploded_) return;
    explosionPosition_ = position_;
    const float floorY = ResolveFloorY(explosionPosition_);
    if (explosionPosition_.y < floorY + 0.2f) explosionPosition_.y = floorY + 0.2f;
    exploded_ = true;
    alive_ = false;
    explosionDrawTimer_ = 0.45f;
    EmitExplosionEffect(explosionPosition_, settings_.explosionRadius); // Damage判定と同じ中心・半径を使い、見た目と当たり判定を一致させる。
}

void MidRangeBombProjectile::EmitExplosionEffect(const Vector3& position, float radius)
{
    const float safeRadius = std::max(0.5f, radius);
    EmitBombParticle("MidRangeBombShockwave", GpuParticleKind::Sprite, GpuParticleType::Shockwave, position, 44, safeRadius * 0.18f, 0.70f, 1.7f);
    EmitBombParticle("MidRangeBombDust", GpuParticleKind::Sprite, GpuParticleType::Dust, position, 52, safeRadius * 0.55f, 1.15f, 1.25f);
    EmitBombParticle("MidRangeBombSpark", GpuParticleKind::Sprite, GpuParticleType::Spark, position + Vector3{ 0.0f, 0.35f, 0.0f }, 30, safeRadius * 0.30f, 0.65f, 2.2f);
    EmitBombParticle("MidRangeBombSmoke", GpuParticleKind::Sprite, GpuParticleType::Smoke, position + Vector3{ 0.0f, 0.45f, 0.0f }, 26, safeRadius * 0.40f, 1.35f, 0.65f);
    EmitBombParticle("MidRangeBombDebris", GpuParticleKind::Mesh, GpuParticleType::Debris, position, 20, safeRadius * 0.42f, 1.10f, 1.8f);
}

void MidRangeBombProjectile::SetDebugCubeSize(float size)
{
    s_debugCubeSize_ = std::clamp(size, 0.1f, 2.0f);
}

void MidRangeBombProjectile::UpdateDebugCube()
{
    if (!debugCube_) return;
    debugCube_->SetTranslate(position_);
    debugCube_->SetRotate({ lifeTimer_ * 2.0f, lifeTimer_ * 3.5f, lifeTimer_ * 1.5f });
    debugCube_->SetScale({ s_debugCubeSize_, s_debugCubeSize_, s_debugCubeSize_ });
    debugCube_->Update();
}

void MidRangeBombProjectile::UpdateTelegraph(float deltaTime)
{
    telegraphTime_ += deltaTime;
    telegraphEmitTimer_ += deltaTime;
    if (telegraphEmitTimer_ < 0.08f) return;
    telegraphEmitTimer_ = std::fmod(telegraphEmitTimer_, 0.08f);
    EmitBombParticle(
        "MidRangeBombTelegraph",
        GpuParticleKind::Sprite,
        GpuParticleType::Spark,
        telegraphPosition_,
        8,
        settings_.explosionRadius * 0.90f,
        0.42f,
        0.20f);
}

float MidRangeBombProjectile::ResolveFloorY(const Vector3& samplePosition) const
{
    const Stage* stage = Stage::GetActiveRuntimeStage();
    if (!stage) return 0.0f;

    float bestFloorY = -std::numeric_limits<float>::infinity();
    for (const AABB& floor : stage->GetFloorAABBs())
    {
        if (samplePosition.x < floor.min.x || samplePosition.x > floor.max.x ||
            samplePosition.z < floor.min.z || samplePosition.z > floor.max.z)
        {
            continue;
        }
        if (floor.max.y <= samplePosition.y + 1.0f) bestFloorY = std::max(bestFloorY, floor.max.y);
    }
    return std::isfinite(bestFloorY) ? bestFloorY : 0.0f;
}

MidRangeBombProjectile::PlayerHitResult MidRangeBombProjectile::TryApplyPlayerDamage(IPlayerRuntime& player)
{
    PlayerHitResult result = PlayerHitResult::None;
    const Vector3 playerPosition = player.GetWorldPosition();

    if (alive_ && !directDamageApplied_)
    {
        const float directDistance = Vector3::Length(playerPosition - position_);
        if (directDistance <= settings_.hitRadius)
        {
            player.ApplyDamage(static_cast<float>(settings_.directHitDamage), &position_);
            directDamageApplied_ = true;
            result = PlayerHitResult::Direct;
            Explode();
        }
    }

    if (exploded_ && !explosionDamageApplied_)
    {
        const float explosionDistance = Vector3::Length(playerPosition - explosionPosition_);
        if (explosionDistance <= settings_.explosionRadius)
        {
            if (result != PlayerHitResult::Direct || settings_.directHitAlsoExplosionDamage)
            {
                player.ApplyDamage(static_cast<float>(settings_.explosionDamage), &explosionPosition_);
                explosionDamageApplied_ = true;
                result = (result == PlayerHitResult::Direct) ? PlayerHitResult::DirectAndExplosion : PlayerHitResult::Explosion;
            }
            else
            {
                explosionDamageApplied_ = true;
            }
        }
    }
    return result;
}

MidRangeBombProjectile::PlayerHitResult MidRangeBombProjectile::TryApplyPlayerDamage(Player& player)
{
    return TryApplyPlayerDamage(static_cast<IPlayerRuntime&>(player));
}

bool MidRangeBombProjectile::IsAlive() const
{
    return alive_ || (exploded_ && explosionDrawTimer_ > 0.0f);
}
