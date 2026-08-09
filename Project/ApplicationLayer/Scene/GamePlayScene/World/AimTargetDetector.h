#pragma once

#include "CollisionHitResult.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstdint>
#include <string>

class CollisionManager;
class EnemyBase;
class EnemySpawnCrystal;

namespace Ken4lowEngine
{
	class BossActor;
	class Camera;
	class Collider;
}

namespace K4E = ::Ken4lowEngine;

/// 照準Rayが最前面で捉えている対象を判定するクラス。
class AimTargetDetector
{
public:
	enum class ObjectType { None, Enemy, Crystal, Boss, Obstacle, Other };

	struct Result
	{
		bool hit = false;
		float hitDistance = 0.0f;
		K4E::Vector3 hitPosition{};
		ObjectType hitObjectType = ObjectType::None;
		K4E::Collider* hitObjectPointer = nullptr;
		bool isDamageableTarget = false;
		bool isBlockedByObstacle = false;
	};

	~AimTargetDetector();
	void Initialize();
	void Update(const K4E::Camera& camera, const CollisionManager& collisionManager);
	void DrawImGui();
	const Result& GetResult() const { return result_; }
	bool HasDamageableTarget() const { return result_.hit && result_.isDamageableTarget; }
	EnemyBase* GetTargetEnemy() const;
	EnemySpawnCrystal* GetTargetCrystal() const;
	K4E::BossActor* GetTargetBoss() const;
	float GetHpBarVisibleHoldTime() const { return hpBarVisibleHoldTime_; }
	bool ShouldShowHpBarOnlyWhenAimed() const { return showHpBarOnlyWhenAimed_; }
	const K4E::Vector4& GetCrosshairNormalColor() const { return crosshairNormalColor_; }
	const K4E::Vector4& GetCrosshairTargetColor() const { return crosshairTargetColor_; }
	static const char* ToString(ObjectType type);

private:
	void RegisterParameters();
	void UnregisterParameters();
	void ApplyParameters();
	void ResetResultForRay(const K4E::Vector3& origin, const K4E::Vector3& direction);
	void SelectTargetFromTrace(const CollisionManager& collisionManager, const K4E::Vector3& origin, const K4E::Vector3& direction);
	bool IsDamageable(ObjectType type) const;

private:
	Result result_{};
	K4E::Vector3 debugRayOrigin_{};
	K4E::Vector3 debugRayDirection_{ 0.0f, 0.0f, 1.0f };
	float debugRayLength_ = 0.0f;
	bool initialized_ = false;
	float aimRayLength_ = 1000.0f;
	bool aimRayDebugDraw_ = false;
	K4E::Vector4 crosshairTargetColor_{ 1.0f, 0.2f, 0.2f, 1.0f };
	K4E::Vector4 crosshairNormalColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
	float hpBarVisibleHoldTime_ = 0.3f;
	bool showHpBarOnlyWhenAimed_ = true;
	bool enableObstacleLineOfSightCheck_ = true;
	float targetDetectionRadius_ = 0.0f;
};
