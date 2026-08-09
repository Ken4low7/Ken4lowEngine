#pragma once

#include "Vector3.h"
#include "Object3D.h"

#include <memory>

class IPlayerRuntime;
class Player;

struct BombProjectileSettings
{
    float initialSpeed = 10.0f;
    bool useDistanceBasedSpeed = true;
    float minInitialSpeed = 7.0f;
    float maxInitialSpeed = 16.0f;
    float speedPerDistance = 0.55f;
    float speedBaseDistance = 6.0f;
    float upwardVelocity = 7.0f;
    float gravity = 18.0f;
    float lifeTime = 4.0f;
    float hitRadius = 0.45f;
    float explosionRadius = 3.0f;
    int directHitDamage = 25;
    int explosionDamage = 12;
    bool directHitAlsoExplosionDamage = false;
};

class MidRangeBombProjectile
{
public:
    enum class PlayerHitResult
    {
        None,
        Direct,
        Explosion,
        DirectAndExplosion,
    };

    void Initialize();
    void Launch(const Ken4lowEngine::Vector3& start, const Ken4lowEngine::Vector3& target, const BombProjectileSettings& settings);
    void Update(float deltaTime);
    void Draw() const;
    void Explode();
    PlayerHitResult TryApplyPlayerDamage(IPlayerRuntime& player);
    PlayerHitResult TryApplyPlayerDamage(Player& player);
    bool IsAlive() const;

    static void SetTargetPlayerRuntime(IPlayerRuntime* player) { s_targetPlayerRuntime_ = player; }
    const Ken4lowEngine::Vector3& GetPosition() const { return position_; }
    static bool IsDebugCubeVisible() { return s_debugCubeVisible_; }
    static void SetDebugCubeVisible(bool visible) { s_debugCubeVisible_ = visible; }
    static float GetDebugCubeSize() { return s_debugCubeSize_; }
    static void SetDebugCubeSize(float size);
    static void EmitExplosionEffect(const Ken4lowEngine::Vector3& position, float radius);

private:
    void UpdateDebugCube();
    void UpdateTelegraph(float deltaTime);
    float ResolveFloorY(const Ken4lowEngine::Vector3& samplePosition) const;

private:
    Ken4lowEngine::Vector3 position_{};
    Ken4lowEngine::Vector3 velocity_{};
    Ken4lowEngine::Vector3 explosionPosition_{};
    Ken4lowEngine::Vector3 telegraphPosition_{};
    BombProjectileSettings settings_{};
    float lifeTimer_ = 0.0f;
    float explosionDrawTimer_ = 0.0f;
    float telegraphTime_ = 0.0f;
    float telegraphEmitTimer_ = 0.0f;
    bool exploded_ = false;
    bool alive_ = false;
    bool directDamageApplied_ = false;
    bool explosionDamageApplied_ = false;
    std::unique_ptr<Ken4lowEngine::Object3D> debugCube_;

    static IPlayerRuntime* s_targetPlayerRuntime_;
    static bool s_debugCubeVisible_;
    static float s_debugCubeSize_;
};
