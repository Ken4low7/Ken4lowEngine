#pragma once
#include "CollisionManager.h"
#include "BulletManager.h"
#include "CharacterWorld.h"
#include "HUDManager.h"
#include "WaveManager.h"
#include "Stage.h"
#include "StageObjectiveManager.h"
#include "GamePlayStageContext.h"
#include <SkyBox.h>
#include "DataAssetPresets.h"
#include "EnemyHPBarManager.h"
#include "ItemManager.h"
#include "CrystalManager.h"
#include "BossBattleController.h"
#include "AimTargetDetector.h"
#include "Stage1TutorialController.h"
#include "Stage2CharacterDeviceManager.h"
#include "GameplayPhysicsDebugController.h"
#include "AmmoRecoveryItemSpawner.h"
#include "WorldDebugView.h"
#include "BulletEnemyCollisionSoA.h"

#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

class Player;
class GamePlayWorld
{
private:

public:
	void Initialize(GamePlayStageContext& stageContext, bool skipStage1Tutorial = false);
	void Finalize();
	void Update(float deltaTime);
	void UpdateIntroVisuals();
	void UpdateEquipIntro(float deltaTime);
	void WarmupStartGameplayForIntro();
	void SetStartGameplayVisualsVisible(bool visible);
	void Draw3D(bool hideCharactersDuringIntro);
	void DrawBossIntro3D();
	void DrawShadow(bool hideCharactersDuringIntro);
	void DrawBossIntroShadow();
	void DrawHUD(bool hideDuringIntro);
	void DrawImGui();
	void DrawGameDebugImGui();
	void DrawCollisionDebugImGui();
	void DrawEnemyDebugImGui();

	void SyncAfterPlayerSpawn();
	void StartWaves();
	bool IsPlayerDead();
	bool IsAllWavesCleared() const;
	bool IsStageObjectiveCleared() const;
	bool IsStageObjectiveFailed() const;
	bool IsGameClearRequested() const { return bossBattleController_.IsGameClearRequested(); }
	void SetDebugCameraEnabled(bool enabled);
	void SetDefenseTargetDestroyed(bool destroyed);

	CharacterWorld& GetCharacters() { return characters_; }
	const CharacterWorld& GetCharacters() const { return characters_; }
	HUDManager* GetHUDManager() const { return hudManager_.get(); }
	BulletManager* GetBulletManager() const { return bulletManager_.get(); }
	K4E::BossActor* GetBoss() const { return bossBattleController_.GetBoss(); }
	WaveManager* GetWaveManager() const { return waveManager_.get(); }
	K4E::Stage* GetStage() const { return stage_.get(); }
	K4E::SkyBox* GetSkyBox() const { return skyBox_.get(); }
	CollisionManager* GetCollisionManager() const { return collisionManager_.get(); }
	const StageObjectiveManager::Snapshot* GetStageObjectiveSnapshot() const
	{
		return stageObjectiveManager_ ? &stageObjectiveManager_->GetSnapshot() : nullptr;
	}
	void EnsureBossStageStarted()
	{
		if (!stageObjectiveManager_ || bossBattleController_.IsSpawned() || bossBattleController_.IsIntroActive() || bossBattleController_.HasIntroPlayed()) return;
		const StageObjectiveManager::Snapshot& snapshot = stageObjectiveManager_->GetSnapshot();
		if (snapshot.type != GamePlayStageContext::StageObjectiveType::DefeatBoss || snapshot.status != StageObjectiveManager::Status::Active) return;
		bossBattleController_.RequestBossBattle(bossBattleController_.GetBossSpawnPosition()); // 中枢制御塔は前提ギミックを挟まず、Arena表示後すぐBoss登場演出へ移る。
	}

	const K4E::Matrix4x4& GetShadowLightViewProjection() const { return shadowLightViewProjection_; }
	bool NotifyStageDeviceActivated(const std::string& deviceId)
	{
		if (!stageObjectiveManager_) return false;
		if (deviceId == Stage2CharacterDeviceManager::GetDefenseTargetDestroyedEventId())
		{
			stageObjectiveManager_->SetDefenseTargetDestroyed(true);
			return true; // Stage 3防衛コアの死亡イベントを共通Objective失敗へ変換する。
		}
		if (deviceId == Stage2CharacterDeviceManager::GetBossArenaEnteredEventId())
		{
			stageObjectiveManager_->SetBossArenaReached(true);
			return bossBattleController_.RequestBossBattle(stage2DeviceManager_.GetBossArenaPosition()); // 広間入場時だけ既存カメラ演出と坑道Boss生成を開始する。
		}
		if (stage2DeviceManager_.IsActive()) stageObjectiveManager_->SetRequiresBossAfterDevices(true);
		return stageObjectiveManager_->NotifyDeviceActivated(deviceId);
	}
	void NotifyStageGoalReached() { SetReachedGoal(true); }
	void NotifyStageDefenseTargetDestroyed() { SetDefenseTargetDestroyed(true); }
	void NotifyStageBossDefeated() { SetBossDefeated(true); }
	void AddActivatedDeviceCount(int amount = 1);
	void SetReachedGoal(bool reached);
	void SetBossDefeated(bool defeated);
	bool HasCrystalBroken() const { return crystalManager_.HasCrystalBroken(); }
	bool IsFinalPhaseReady() const { return crystalManager_.IsFinalPhaseReady(); }
	bool IsBossAppearRequested() const { return crystalManager_.IsBossAppearRequested(); }
	bool IsWorldColorChangeComplete() const { return crystalManager_.IsWorldColorChangeComplete(); }
	bool IsBossIntroActive() const { return bossBattleController_.IsIntroActive(); }
	bool IsBossIntroGameplayPaused() const { return bossBattleController_.IsIntroGameplayPaused(); }
	bool IsBossIntroPresentationActive() const { return bossBattleController_.IsIntroPresentationActive(); }
	bool HasStage1TutorialCompleted() const { return stage1TutorialController_.HasCompletedTutorial(); }

private:
	void InitializeLighting();
	void InitializeSkyBox();
	void InitializeCollisionSystems();
	void InitializeCharacterSystems();
	void InitializeHUD();
	void InitializeStageAndPhysics(const GamePlayStageContext::StageAssetPaths& stageAssets);
	void InitializePlayerSpawn(GamePlayStageContext& stageContext);
	void InitializeWaveSystem(GamePlayStageContext& stageContext);
	void InitializeBossState(GamePlayStageContext& stageContext);
	void InitializeStage1Crystals();
	void InitializeStage2Devices(GamePlayStageContext& stageContext);
	void InitializeRuntimeHelpers();
	void UpdateStageRuntime();
	bool UpdateBlockingStage1Intro(float deltaTime);
	bool UpdateBlockingBossIntro(float deltaTime);
	void UpdateGameplayActors(float deltaTime);
	void UpdatePlayerLadderOverlap();
	void UpdateBossRuntime(float deltaTime);
	void UpdateItemRuntime(float deltaTime);
	void UpdateShadowRuntime();
	void UpdateBulletAndCollisionRuntime(float deltaTime);
	void UpdateBulletEnemySoAProbe();
	void UpdateSkyBoxRuntime(float deltaTime);
	void UpdateAimTargetRuntime();
	void UpdateHpBarRuntime(float deltaTime);
	void UpdateHudRuntime(float deltaTime);
	void UpdateWaveRuntime(float deltaTime);
	void UpdateStageObjectiveRuntime(float deltaTime);
	void CollisionUpdate();
	GameplayPhysicsDebugController::Dependencies BuildGameplayPhysicsDebugDependencies();
	void UpdateShadowLightViewProjection();
	bool TryGetDirectionalLightFromManager(K4E::Vector3& outDirection) const;
	bool IsSightBlocked(const K4E::Segment& seg) const;
	Stage1TutorialController::Dependencies BuildStage1TutorialDependencies();
	BossBattleController::Dependencies BuildBossBattleDependencies();
	WorldDebugView::Dependencies BuildWorldDebugDependencies();

private:
	std::unique_ptr<CollisionManager> collisionManager_;
	std::unique_ptr<BulletManager> bulletManager_;
	CharacterWorld characters_;
	std::unique_ptr<K4E::SkyBox> skyBox_ = nullptr;
	K4E::SkyBoxPresetCollection skyBoxPresets_{};
	std::unique_ptr<K4E::Stage> stage_ = nullptr;
	std::unique_ptr<WaveManager> waveManager_ = nullptr;
	std::unique_ptr<HUDManager> hudManager_ = nullptr;
	EnemyHPBarManager enemyHpBarManager_;
	ItemManager itemManager_;
	AmmoRecoveryItemSpawner ammoRecoveryItemSpawner_;
	CrystalManager crystalManager_;
	Stage2CharacterDeviceManager stage2DeviceManager_;
	AimTargetDetector aimTargetDetector_;
	GameplayPhysicsDebugController gameplayPhysicsDebugController_{};
	WorldDebugView worldDebugView_{};
	K4E::BulletEnemyCollisionSoA bulletEnemyCollisionSoA_{};
	int prevWaveNumber_ = 0;
	bool prevWaveInProgress_ = false;
	bool prevAllWavesCleared_ = false;
	K4E::Matrix4x4 shadowLightViewProjection_{};
	K4E::Vector3 shadowLightDirection_ = { 0.3f, -1.0f, 0.2f };
	float shadowDistance_ = 50.0f;
	float shadowOrthoHalfWidth_ = 25.0f;
	float shadowOrthoHalfHeight_ = 25.0f;
	float shadowNearZ_ = 0.1f;
	float shadowFarZ_ = 120.0f;
	std::unique_ptr<StageObjectiveManager> stageObjectiveManager_ = nullptr;
	float lastBulletUpdateMs_ = 0.0f;
	float lastCollisionUpdateMs_ = 0.0f;
	float lastBulletEnemySoAMs_ = 0.0f;
	K4E::BulletEnemyCollisionSoA::FrameStats lastBulletEnemySoAStats_{};
	bool enableBulletEnemySoAProbe_ = true;
	float bulletEnemySoACellSize_ = 2.0f;
	bool stage1BeginnerBalanceEnabled_ = false;
	bool skipStage1Tutorial_ = false;
	Stage1TutorialController stage1TutorialController_{};
	BossBattleController bossBattleController_{};
};
