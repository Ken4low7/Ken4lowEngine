#pragma once

#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "BossEnemyVfx.h"
#include "Camera.h"
#include "CameraManager.h"
#include "Vector3.h"

#include <algorithm>
#include <cstdint>
#include <numbers>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// Objective完了後のBossActor登場演出を管理する。
class BossIntroController
{
public:
	enum class State
	{
		None,
		WaitingAfterCrystalsBroken,
		StartCutscene,
		CameraMoveToBoss,
		BossSpawnImpact,
		BossRising,
		FinishCutscene,
		Completed,
	};

	struct Settings
	{
		float bossAppearDelay = 2.0f;
		K4E::Vector3 bossAppearPosition{ 0.0f, 2.25f, 30.0f };
		float bossStartOffsetY = -20.0f;
		float bossRiseTime = 3.0f;
		float cameraMoveTime = 1.5f;
		float cameraReturnTime = 1.0f;
		bool bossIntroPauseGame = true;
		bool enableBossIntroCamera = true;
		bool enableBossRiseEffect = true;
		bool enableBossIntroDust = true;
		bool enableBossIntroCameraShake = true;
		float dustGroundOffsetY = 2.2f;
		float dustEmitInterval = 0.08f;
		uint32_t dustStartBurstCount = 96;
		uint32_t dustLoopEmitCount = 10;
		uint32_t dustEndBurstCount = 128;
		float spawnShakeDuration = 0.75f;
		float spawnShakeAmplitude = 0.45f;
		float spawnShakeFrequency = 22.0f;
	};

	void Initialize(const K4E::Vector3& defaultBossPosition);
	void Finalize();
	void RequestStart(const K4E::Vector3& bossPosition);
	void Reset();
	void SetRequestedBossPositionOverride(const K4E::Vector3& position)
	{
		requestedBossPosition_ = position;
		hasRequestedBossPosition_ = true;
		settings_.bossAppearPosition = position; // Stage固有の登場位置をParameter設定より優先する。
	}
	void ClearRequestedBossPositionOverride() { hasRequestedBossPosition_ = false; }

	void Update(float deltaTime, K4E::BossActor* boss, K4E::Camera* camera)
	{
		ApplyParameters();
		if (hasRequestedBossPosition_) settings_.bossAppearPosition = requestedBossPosition_; // Parameter再適用後もStage固有の登場座標を演出終了まで優先する。
		SetDebugSnapshot(boss, camera);
		switch (state_)
		{
		case State::WaitingAfterCrystalsBroken:
			stateTimer_ += deltaTime;
			if (stateTimer_ >= std::max(0.0f, settings_.bossAppearDelay)) ChangeState(State::StartCutscene);
			break;
		case State::StartCutscene: BeginCutscene(camera); break;
		case State::CameraMoveToBoss: UpdateCameraMove(deltaTime, camera); break;
		case State::BossSpawnImpact: BeginBossSpawnImpact(camera); break;
		case State::BossRising: UpdateBossActorRising(deltaTime, boss, camera); break;
		case State::FinishCutscene: UpdateBossActorCameraReturn(deltaTime, boss, camera); break;
		default: break;
		}
	}

	void SetDebugSnapshot(const K4E::BossActor* boss, const K4E::Camera* camera)
	{
		debugHasBoss_ = boss != nullptr;
		debugHasCamera_ = camera != nullptr;
		debugBossPosition_ = boss ? boss->GetPosition() : K4E::Vector3{};
		debugBossLocalPosition_ = boss ? boss->GetRootLocalPosition() : K4E::Vector3{};
		debugBossWorldPosition_ = boss ? boss->GetRootWorldPosition() : K4E::Vector3{};
		debugBossHasParent_ = boss ? boss->HasRootParent() : false;
		debugCameraPosition_ = camera ? camera->GetTranslate() : K4E::Vector3{};
		debugBossCameraDistance_ = boss && camera ? K4E::Vector3::Length(debugBossWorldPosition_ - debugCameraPosition_) : 0.0f;
		debugViewProjectionKind_ = camera && K4E::CameraManager::GetInstance()->GetMainCamera() == camera
			? "Gameplay/MainCamera"
			: "CameraManager Other/Debug";
	}

	void DrawImGui();
	bool IsRunning() const { return state_ != State::None && state_ != State::Completed; }
	bool IsWaitingDelay() const { return state_ == State::WaitingAfterCrystalsBroken; }
	bool IsGameplayPaused() const;
	bool HasPlayed() const { return hasPlayedBossIntro_; }
	bool IsCompleted() const { return state_ == State::Completed; }
	bool ShouldRegisterBossCollider() const { return bossColliderEnableRequested_; }
	bool ConsumeBossSpawnRequest();
	bool ConsumeBossColliderEnableRequest();
	bool ConsumeDebugStartRequest();
	bool ConsumeDebugResetRequest();
	bool ConsumeDebugForceBossToAppearRequest();
	bool ConsumeDebugClearBossParentRequest();
	bool ConsumeDebugUseGameplayViewProjectionRequest();
	State GetState() const { return state_; }
	const Settings& GetSettings() const { return settings_; }
	const K4E::Vector3& GetBossAppearPosition() const { return settings_.bossAppearPosition; }
	K4E::Vector3 GetBossStartPosition() const;
	K4E::Vector3 GetBossLookTarget() const;

private:
	void RegisterParameters();
	void UnregisterParameters();
	void ApplyParameters();
	void ChangeState(State state);
	void BeginCutscene(K4E::Camera* camera);
	void UpdateCameraMove(float deltaTime, K4E::Camera* camera);
	void BeginBossSpawnImpact(K4E::Camera* camera);

	void UpdateBossActorRising(float deltaTime, K4E::BossActor* boss, K4E::Camera* camera)
	{
		stateTimer_ += deltaTime;
		UpdateCameraShake(deltaTime);
		const float t = settings_.bossRiseTime <= 0.0f ? 1.0f : Clamp01(stateTimer_ / settings_.bossRiseTime);
		UpdateBossIntroDust(deltaTime, t);
		if (boss)
		{
			boss->SetPosition(Lerp(GetBossStartPosition(), settings_.bossAppearPosition, t));
			boss->SetYaw(std::numbers::pi_v<float>);
			boss->ForceSyncWorldTransform();
		}
		if (settings_.enableBossIntroCamera && camera) ApplyCameraLookAtBoss(camera, introCameraPosition_, introCameraTarget_);
		if (t >= 1.0f)
		{
			ChangeState(settings_.enableBossIntroCamera ? State::FinishCutscene : State::Completed);
			if (state_ == State::Completed) CompleteBossActorIntro(boss, camera);
		}
	}

	void UpdateBossActorCameraReturn(float deltaTime, K4E::BossActor* boss, K4E::Camera* camera)
	{
		stateTimer_ += deltaTime;
		UpdateCameraShake(deltaTime);
		const float t = settings_.cameraReturnTime <= 0.0f ? 1.0f : Clamp01(stateTimer_ / settings_.cameraReturnTime);
		if (camera) ApplyCameraLookAtBoss(camera, Lerp(introCameraPosition_, savedCameraPosition_, t), introCameraTarget_);
		if (t >= 1.0f) CompleteBossActorIntro(boss, camera);
	}

	void CompleteBossActorIntro(K4E::BossActor* boss, K4E::Camera* camera)
	{
		if (boss)
		{
			boss->ClearRootParentKeepingWorldPosition();
			boss->SetPosition(settings_.bossAppearPosition);
			boss->SetYaw(std::numbers::pi_v<float>);
			boss->ForceSyncWorldTransform();
		}
		if (!bossIntroEndDustDone_)
		{
			EmitBossIntroDustBurst(settings_.dustEndBurstCount);
			bossIntroEndDustDone_ = true;
		}
		SetDebugSnapshot(boss, camera);
		bossColliderEnableRequested_ = true;
		hasPlayedBossIntro_ = true;
		ChangeState(State::Completed);
	}

	void UpdateBossIntroDust(float deltaTime, float riseT);
	void EmitBossIntroDustBurst(uint32_t emitCount);
	void EmitBossIntroImpactBurst(uint32_t shockCount, uint32_t dustCount);
	K4E::Vector3 MakeBossIntroDustPosition() const;
	void ResetBossIntroDustState();
	void StartCameraShake(float duration, float amplitude, float frequency);
	void UpdateCameraShake(float deltaTime);
	K4E::Vector3 GetCameraShakeOffset() const;
	void ApplyCameraLookAtBoss(K4E::Camera* camera, const K4E::Vector3& cameraPosition, const K4E::Vector3& targetPosition) const;
	static K4E::Vector3 Lerp(const K4E::Vector3& a, const K4E::Vector3& b, float t);
	static float Clamp01(float value);
	static const char* ToStateLabel(State state);

private:
	static constexpr const char* kParameterGroupName = "BossIntro";
	Settings settings_{};
	State state_ = State::None;
	float stateTimer_ = 0.0f;
	bool hasPlayedBossIntro_ = false;
	bool bossSpawnRequested_ = false;
	bool bossColliderEnableRequested_ = false;
	bool debugStartRequested_ = false;
	bool debugResetRequested_ = false;
	bool debugForceBossToAppearRequested_ = false;
	bool debugClearBossParentRequested_ = false;
	bool debugUseGameplayViewProjectionRequested_ = false;
	K4E::Vector3 requestedBossPosition_{};
	bool hasRequestedBossPosition_ = false;
	K4E::Vector3 savedCameraPosition_{};
	K4E::Vector3 savedCameraRotation_{};
	K4E::Vector3 introCameraPosition_{ 0.0f, 9.0f, 18.0f };
	K4E::Vector3 introCameraTarget_{};
	K4E::Vector3 debugBossPosition_{};
	K4E::Vector3 debugBossLocalPosition_{};
	K4E::Vector3 debugBossWorldPosition_{};
	K4E::Vector3 debugCameraPosition_{};
	float debugBossCameraDistance_ = 0.0f;
	bool debugHasBoss_ = false;
	bool debugHasCamera_ = false;
	bool debugBossHasParent_ = false;
	const char* debugViewProjectionKind_ = "Unknown";
	BossEnemyVfx bossEnemyVfx_{};
	float bossIntroDustEmitTimer_ = 0.0f;
	bool bossIntroStartDustDone_ = false;
	bool bossIntroEndDustDone_ = false;
	bool bossSpawnImpactTriggered_ = false;
	float cameraShakeTimer_ = 0.0f;
	float cameraShakeDuration_ = 0.0f;
	float cameraShakeAmplitude_ = 0.0f;
	float cameraShakeFrequency_ = 0.0f;
	float cameraShakeSeed_ = 0.0f;
};
