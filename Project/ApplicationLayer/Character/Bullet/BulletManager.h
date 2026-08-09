#pragma once
#include "Bullet.h"
#include "WeaponParams.h"

#include <memory>
#include <vector>
#include <functional>
#include <utility>

/// ---------- 前方宣言 ---------- ///
class CollisionManager;
namespace Ken4lowEngine
{
	class PhysicsWorld;
	class BulletEnemyCollisionSoA;
}

/// -------------------------------------------------------------
///                     弾管理クラス
/// -------------------------------------------------------------
class BulletManager
{
public: /// ---------- 型定義 ---------- ///

	using ShotEffectTransformResolver = std::function<bool(Ken4lowEngine::Vector3&, Ken4lowEngine::Vector3&)>;

public: /// ---------- メンバ関数 ---------- ///

	void Initialize(CollisionManager* collisionManager);

	Bullet* Spawn(const Ken4lowEngine::Vector3& startPos,
		const Ken4lowEngine::Vector3& dir,
		float speed,
		int damage = 1,
		float lifeTimeSec = 3.0f,
		const Ken4lowEngine::Vector3& shooterPosition = { 0.0f, 0.0f, 0.0f },
		uint32_t shooterColliderId = 0u,
		uint32_t typeId = static_cast<uint32_t>(CollisionTypeIdDef::kBullet),
		const WeaponParams& weaponParams = WeaponParams()
	);

	void Update(float dt);
	void Draw();
	void DrawImGui();
	void Clear();

	void SetWorldImpactCallback(std::function<void(const Ken4lowEngine::Vector3&, const Ken4lowEngine::Vector3&)> callback);
	void SetPhysicsTriggerWorld(Ken4lowEngine::PhysicsWorld* physicsWorld, uint32_t playerBulletLayer);
	void SetUsePhysicsTriggerForNormalBullets(bool enabled);
	void RefreshPhysicsTriggerRegistrations();
	void AppendCollisionSoABullets(Ken4lowEngine::BulletEnemyCollisionSoA& collisionSoA) const;

	void SetShotEffectTransformResolver(ShotEffectTransformResolver resolver)
	{
		shotEffectTransformResolver_ = std::move(resolver); // 発射VFXの位置だけをPlayer ViewModel側から注入し、弾道生成とは分離する。
	}

public: /// ---------- アクセサ ---------- ///

	size_t GetCount() const { return bullets_.size(); }
	size_t GetActiveCount() const;
	size_t GetPhysicsTriggerBulletCount() const;
	int GetPhysicsTriggerHitCount() const { return physicsTriggerHitCount_; }
	CollisionManager* GetCollisionManager() const { return collisionManager_; } // 新Player近接ComponentもGamePlayの同じCollision正本を参照する。

private: /// ---------- メンバ変数 ---------- ///

	CollisionManager* collisionManager_ = nullptr;
	Ken4lowEngine::PhysicsWorld* physicsWorld_ = nullptr;
	uint32_t playerBulletLayer_ = 0u;
	bool usePhysicsTriggerForNormalBullets_ = false;
	int physicsTriggerHitCount_ = 0;
	std::function<void(const Ken4lowEngine::Vector3&, const Ken4lowEngine::Vector3&)> worldImpactCallback_{};
	ShotEffectTransformResolver shotEffectTransformResolver_{};
	std::vector<std::unique_ptr<Bullet>> bullets_;
};
