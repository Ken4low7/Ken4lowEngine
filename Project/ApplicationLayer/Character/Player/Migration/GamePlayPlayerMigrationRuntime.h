#pragma once

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "ApplicationLayer/Character/Player/Migration/PlayerTutorialRestrictionBridge.h"
#include "ApplicationLayer/Scene/DebugScene/DebugActorRegistration.h"
#include "BulletManager.h"
#include "CollisionManager.h"
#include "Stage.h"

#include <ActorWorld.h>
#include <AudioManager.h>
#include <Camera.h>
#include <CameraManager.h>
#include <Collider.h>
#include <Input.h>
#include <InputSnapshot.h>
#include <PhysicsWorld.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

/// PlayerActorをGamePlay入力・Ladder・Bullet Bridgeへ接続し、World更新前後のPlayer固有処理だけを担当するRuntime Controller。
class GamePlayPlayerMigrationRuntime
{
public:
	bool Initialize(BulletManager* bulletManager, K4E::Stage* stage, K4E::ActorWorld* actorWorld, K4E::PhysicsWorld* physicsWorld, const K4E::Vector3& spawnPosition)
	{
		Finalize();
		bulletManager_ = bulletManager;
		legacyCollisionManager_ = bulletManager_ ? bulletManager_->GetCollisionManager() : nullptr;
		stage_ = stage;
		actorWorld_ = actorWorld;
		physicsWorld_ = physicsWorld;
		if (!stage_ || !actorWorld_ || !physicsWorld_) return false;

		RegisterApplicationActorTypes();
		K4E::Actor* spawnedActor = actorWorld_->SpawnActorFromJson(kPlayerPrefabPath);
		player_ = dynamic_cast<K4E::PlayerActor*>(spawnedActor);
		if (!player_)
		{
			if (spawnedActor) actorWorld_->DestroyActor(spawnedActor);
			Finalize();
			return false;
		}

		player_->Initialize(); // Prefab復元後に不足ComponentとPlayer固有Callbackを同じ初期化入口で再接続する。
		player_->SetName("Player");
		player_->SetLayer("Player");
		player_->AddTag("Player");
		stageColliders_ = stage_->GetWorldColliderPointers();
		ApplyP0MovementTuning(player_->GetPlayerMovementComponent());
		player_->ResetForValidation(ResolveSpawnRootPosition(spawnPosition));
		if (K4E::WeaponComponent* weapon = player_->GetWeaponComponent()) weapon->ConfigureAmmoState(30, 30, 90, 120);
		RefreshPlayerRuntimeBindings();
		Bullet::SetDamageableHitCallback([this](bool killed)
			{
				if (player_) player_->NotifyHitFeedback(killed); // Bulletの寿命よりPlayer Runtimeが先に破棄されてもnull確認してHUD通知を止める。
			});
		UpdateLadderState();
		RefreshNearbyStageColliders(true);
		player_->SetGameplayHudVisible(true);
		if (auto* visual = player_->GetHumanoidVisualComponent()) visual->SetActive(false);

		if (K4E::Camera* camera = GetCamera())
		{
			camera->SetFarClip(1600.0f);
			K4E::CameraManager::GetInstance()->SetMainCamera(camera);
		}
		lastShotRevision_ = player_->GetWeaponComponent() ? player_->GetWeaponComponent()->GetShotRevision() : 0u;
		active_ = true;
		return true;
	}

	void Finalize()
	{
		active_ = false;
		Bullet::SetDamageableHitCallback({});
		if (player_) player_->SetLadderState(false);
		ClearNearbyStageColliders();
		stageColliders_.clear();
		player_ = nullptr;
		lastTunedMovement_ = nullptr;
		bulletManager_ = nullptr;
		legacyCollisionManager_ = nullptr;
		stage_ = nullptr;
		actorWorld_ = nullptr;
		physicsWorld_ = nullptr;
		lastShotRevision_ = 0u;
		lastStageRefreshPosition_ = {};
		hasStageRefreshPosition_ = false;
	}

	bool BeginWorldUpdate(float deltaTime, bool allowInput = true)
	{
		(void)deltaTime;
		if (!active_ || !player_ || !actorWorld_ || !physicsWorld_) return false;
		RefreshPlayerRuntimeBindings();
		K4E::Input* input = K4E::Input::GetInstance();
		K4E::PlayerInputComponent* playerInput = player_->GetPlayerInputComponent();
		if (playerInput)
		{
			const PlayerTutorialRestrictionBridge::State restrictions = PlayerTutorialRestrictionBridge::GetState();
			playerInput->SetInputRestrictions(restrictions.enabled, restrictions.allowMove, restrictions.allowShoot, restrictions.allowReload);
			playerInput->SetWeaponSwitchEnabled(restrictions.allowWeaponSwitch); // Stage1の1武器制限を数字キーとホイール入力へ常時反映する。
		}

		const bool canControl = allowInput && input && playerInput && input->IsGameInputEnabled() && !K4E::CameraManager::GetInstance()->IsUsingDebugCamera();
		if (canControl) playerInput->ApplyInputSnapshot(K4E::BuildInputSnapshot(*input), kMouseLookSensitivity);
		else if (playerInput) playerInput->ResetInputState();
		UpdateLadderState();
		return true; // ActorWorldとPhysicsWorldの更新は所有者であるCharacterWorldへ委譲する。
	}

	void PreparePhysicsUpdate()
	{
		if (!active_ || !player_ || !physicsWorld_) return;
		RefreshNearbyStageColliders(false); // Player移動後、Physics判定前に周辺Stage Collider集合を更新する。
	}

	void EndWorldUpdate()
	{
		if (!active_ || !player_) return;
		SpawnBridgedShots();
		if (!K4E::CameraManager::GetInstance()->IsUsingDebugCamera())
		{
			if (K4E::Camera* camera = GetCamera()) K4E::CameraManager::GetInstance()->SetMainCamera(camera);
		}
	}

	/// Debug・互換呼び出しでは従来順を維持し、本番CharacterWorldは分割APIを使用する。
	void Update(float deltaTime, bool allowInput = true)
	{
		if (!BeginWorldUpdate(deltaTime, allowInput)) return;
		actorWorld_->Update(deltaTime);
		PreparePhysicsUpdate();
		physicsWorld_->Update(deltaTime);
		actorWorld_->PostPhysicsUpdate(deltaTime);
		EndWorldUpdate();
	}

	void PrepareRenderState() { if (active_ && actorWorld_) actorWorld_->PrepareRenderState(); }
	void Draw() { if (active_ && actorWorld_) actorWorld_->Draw(); }
	void DrawShadow() { if (active_ && actorWorld_) actorWorld_->DrawShadow(); }
	void SetDebugCameraEnabled(bool enabled)
	{
		if (!active_ || !player_) return;
		if (K4E::PlayerInputComponent* input = player_->GetPlayerInputComponent(); input && enabled) input->ResetInputState();
	}

	bool IsActive() const { return active_ && player_ != nullptr; }
	K4E::PlayerActor* GetPlayer() { return player_; }
	const K4E::PlayerActor* GetPlayer() const { return player_; }
	IPlayerRuntime* GetPlayerRuntime() { return player_; }
	const IPlayerRuntime* GetPlayerRuntime() const { return player_; }
	size_t GetRegisteredStageColliderCount() const { return activeStageColliders_.size(); }
	size_t GetTotalStageColliderCount() const { return stageColliders_.size(); }
	K4E::Camera* GetCamera() const { return player_ ? player_->GetCamera() : nullptr; }
	K4E::Vector3 GetPlayerPosition() const { return player_ ? player_->GetWorldPosition() : K4E::Vector3{}; }

private:
	void ApplyP0MovementTuning(K4E::PlayerMovementComponent* movement)
	{
		if (!movement || movement == lastTunedMovement_) return;
		nlohmann::json tuning;
		movement->ToJson(tuning);
		tuning["MoveSpeed"] = kGroundedMoveSpeed;
		tuning["SprintSpeedMultiplier"] = kGroundedSprintMultiplier;
		tuning["JumpSpeed"] = kGroundedJumpSpeed;
		movement->FromJson(tuning); // Prefabの他設定を維持したままP0の重い操作感だけを一度適用する。
		lastTunedMovement_ = movement;
	}

	void RefreshPlayerRuntimeBindings()
	{
		if (!player_) return;
		ApplyP0MovementTuning(player_->GetPlayerMovementComponent());
		if (K4E::PlayerMeleeAttackComponent* melee = player_->GetPlayerMeleeAttackComponent())
		{
			melee->SetCollisionManager(legacyCollisionManager_);
			melee->SetHitFeedbackCallback([this](bool killed) { if (player_) player_->NotifyHitFeedback(killed); }); // JSON再読込後の新ComponentへRuntime依存を張り直す。
		}
		if (const K4E::WeaponComponent* weapon = player_->GetWeaponComponent(); weapon && weapon->GetShotRevision() < lastShotRevision_)
		{
			lastShotRevision_ = weapon->GetShotRevision(); // 再読込でRevisionが初期化された場合に次の1発から再同期する。
		}
	}

	K4E::Vector3 ResolveSpawnRootPosition(const K4E::Vector3& requestedPosition) const
	{
		K4E::Vector3 resolved = requestedPosition;
		if (!player_ || !stage_) return resolved;

		float floorTop = -std::numeric_limits<float>::infinity();
		float nearestFloorDistance = std::numeric_limits<float>::infinity();
		for (const K4E::AABB& floor : stage_->GetFloorAABBs())
		{
			constexpr float kSpawnBoundsMargin = 0.15f;
			const bool containsXZ = requestedPosition.x >= floor.min.x - kSpawnBoundsMargin && requestedPosition.x <= floor.max.x + kSpawnBoundsMargin &&
				requestedPosition.z >= floor.min.z - kSpawnBoundsMargin && requestedPosition.z <= floor.max.z + kSpawnBoundsMargin;
			if (!containsXZ) continue;
			const float floorDistance = std::fabs(floor.max.y - requestedPosition.y);
			if (floorDistance < nearestFloorDistance)
			{
				nearestFloorDistance = floorDistance;
				floorTop = floor.max.y; // 上下に床が重なる場所ではSpawnPointのYに最も近い床面を選ぶ。
			}
		}
		if (!std::isfinite(floorTop)) return resolved;

		const K4E::CharacterColliderComponent* collider = player_->GetColliderComponent();
		const K4E::SceneComponent* root = player_->GetRootComponent();
		if (!collider || !root) return resolved;
		const float rootScaleY = std::fabs(root->GetWorldScale().y);
		const float colliderScaleY = std::fabs(collider->GetLocalScale().y);
		const float halfHeight = collider->GetHalfSize().y * rootScaleY * colliderScaleY;
		const float localCenterY = collider->GetLocalPosition().y * rootScaleY;
		resolved.y = floorTop + halfHeight - localCenterY + kSpawnGroundClearance; // SpawnPointを足元基準としてCollider下面を床上へ揃える。
		return resolved;
	}

	static float DistanceSquaredPointToAabbXZ(const K4E::Vector3& point, const K4E::AABB& bounds)
	{
		const float dx = point.x < bounds.min.x ? bounds.min.x - point.x : (point.x > bounds.max.x ? point.x - bounds.max.x : 0.0f);
		const float dz = point.z < bounds.min.z ? bounds.min.z - point.z : (point.z > bounds.max.z ? point.z - bounds.max.z : 0.0f);
		return dx * dx + dz * dz;
	}

	void UpdateLadderState()
	{
		if (!player_ || !stage_) return;
		K4E::Collider* collider = player_->GetCollisionPrimitive();
		player_->SetLadderState(collider && stage_->CheckLadderOverlap(collider->GetAABB())); // StageのLadder Triggerを新Movementへ同期する。
	}

	void RefreshNearbyStageColliders(bool force)
	{
		if (!player_ || !physicsWorld_) return;
		const K4E::Vector3 playerPosition = GetPlayerPosition();
		if (!force && hasStageRefreshPosition_)
		{
			const float dx = playerPosition.x - lastStageRefreshPosition_.x;
			const float dz = playerPosition.z - lastStageRefreshPosition_.z;
			if (dx * dx + dz * dz < kStageRefreshDistance * kStageRefreshDistance) return;
		}

		std::vector<std::pair<float, K4E::Collider*>> candidates;
		candidates.reserve(stageColliders_.size());
		for (K4E::Collider* collider : stageColliders_)
		{
			if (!collider || !collider->IsCollisionEnabledForQuery()) continue;
			const float distanceSq = DistanceSquaredPointToAabbXZ(playerPosition, collider->GetAABB());
			if (distanceSq <= kStageActivationRadius * kStageActivationRadius) candidates.emplace_back(distanceSq, collider);
		}
		if (candidates.size() > kMaxActiveStageColliders)
		{
			std::nth_element(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(kMaxActiveStageColliders), candidates.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
			candidates.resize(kMaxActiveStageColliders);
		}

		std::unordered_set<K4E::Collider*> desired;
		desired.reserve(candidates.size());
		for (const auto& candidate : candidates) desired.insert(candidate.second);
		for (auto it = activeStageColliders_.begin(); it != activeStageColliders_.end();)
		{
			if (!desired.contains(*it))
			{
				physicsWorld_->UnregisterCollider(*it);
				if (legacyCollisionManager_) legacyCollisionManager_->RemoveCollider(*it);
				it = activeStageColliders_.erase(it);
			}
			else ++it;
		}
		for (K4E::Collider* collider : desired)
		{
			if (!activeStageColliders_.insert(collider).second) continue;
			physicsWorld_->RegisterCollider(collider);
			if (legacyCollisionManager_) legacyCollisionManager_->AddCollider(collider); // 弾・Enemy対WorldもPlayer周辺だけLegacy判定へ登録する。
		}
		lastStageRefreshPosition_ = playerPosition;
		hasStageRefreshPosition_ = true;
	}

	void ClearNearbyStageColliders()
	{
		for (K4E::Collider* collider : activeStageColliders_)
		{
			if (physicsWorld_) physicsWorld_->UnregisterCollider(collider);
			if (legacyCollisionManager_) legacyCollisionManager_->RemoveCollider(collider);
		}
		activeStageColliders_.clear();
	}

	void SpawnBridgedShots()
	{
		if (!player_ || !bulletManager_) return;
		K4E::WeaponComponent* weapon = player_->GetWeaponComponent();
		K4E::Camera* camera = GetCamera();
		if (!weapon || !camera || weapon->IsMeleeWeapon()) return;

		const unsigned int currentRevision = weapon->GetShotRevision();
		while (lastShotRevision_ < currentRevision)
		{
			const WeaponParams params = weapon->BuildProjectileParams();
			const K4E::Vector3 forward = K4E::Vector3::Normalize(camera->GetForward());
			const K4E::Vector3 start = camera->GetTranslate() + forward * (std::max)(0.05f, params.muzzleForwardOffset);
			const float speed = (std::max)(1.0f, params.projectileSpeed);
			const float lifeTime = params.projectileLifeTime > 0.0f ? params.projectileLifeTime : (std::max)(0.1f, params.maxRange / speed);
			const int damage = (std::max)(1, static_cast<int>(std::lround(params.damage)));
			bulletManager_->Spawn(start, forward, speed, damage, lifeTime, GetPlayerPosition(), 0u, static_cast<uint32_t>(CollisionTypeIdDef::kBullet), params);
			K4E::AudioManager::GetInstance()->PlaySE("player_fire.mp3", 0.1f);
			++lastShotRevision_;
		}
	}

private:
	static constexpr const char* kPlayerPrefabPath = "Resources/ActorPrefabs/Player.json";
	static constexpr float kMouseLookSensitivity = 0.0025f;
	static constexpr float kSpawnGroundClearance = 0.02f;
	static constexpr float kGroundedMoveSpeed = 5.5f;
	static constexpr float kGroundedSprintMultiplier = 1.4f;
	static constexpr float kGroundedJumpSpeed = 6.0f;
	static constexpr float kStageActivationRadius = 72.0f;
	static constexpr float kStageRefreshDistance = 5.0f;
	static constexpr size_t kMaxActiveStageColliders = 256;
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::PhysicsWorld* physicsWorld_ = nullptr;
	K4E::PlayerActor* player_ = nullptr;
	K4E::PlayerMovementComponent* lastTunedMovement_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	CollisionManager* legacyCollisionManager_ = nullptr;
	K4E::Stage* stage_ = nullptr;
	std::vector<K4E::Collider*> stageColliders_{};
	std::unordered_set<K4E::Collider*> activeStageColliders_{};
	K4E::Vector3 lastStageRefreshPosition_{};
	unsigned int lastShotRevision_ = 0u;
	bool hasStageRefreshPosition_ = false;
	bool active_ = false;
};
