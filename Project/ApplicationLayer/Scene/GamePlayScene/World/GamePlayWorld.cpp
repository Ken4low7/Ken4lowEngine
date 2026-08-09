#define NOMINMAX
#include "DirectXCommon.h"
#include "GamePlayWorld.h"
#include "GamePlayStageContext.h"
#include "GameViewportConstants.h"

#include "LightManager.h"
#include "CameraManager.h"
#include "SkyBoxManager.h"
#include "JsonDataManager.h"
#include "Wireframe.h"
#include "EnemyBase.h"
#include "EnemyHPBarProjector.h"
#include "GameplayPhysicsEventHandler.h"
#include "GpuParticleManager.h"
#include "ParticleManager.h"
#include "PhysicsTestBullet.h"
#include "CollisionTypeIdDef.h"
#include "Input.h"
#include <LogString.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#ifdef USE_IMGUI
#include <Editor/EditorModeController.h>
#include <imgui.h>
#include <GameTimer.h>
#endif

using namespace Ken4lowEngine;

void GamePlayWorld::Initialize(GamePlayStageContext& stageContext, bool skipStage1Tutorial)
{
	const auto stageAssets = stageContext.GetCurrentStageAssets();
	stage1BeginnerBalanceEnabled_ = stageContext.IsBeginningPlainStage();
	skipStage1Tutorial_ = skipStage1Tutorial;
	InitializeLighting();
	InitializeSkyBox();
	InitializeCollisionSystems();
	InitializeCharacterSystems();
	InitializeHUD();
	InitializeStageAndPhysics(stageAssets);
	InitializePlayerSpawn(stageContext);
	InitializeWaveSystem(stageContext);
	InitializeBossState(stageContext);
	InitializeStage1Crystals();
	InitializeStage2Devices(stageContext);
	InitializeRuntimeHelpers();
}

void GamePlayWorld::InitializeLighting()
{
	auto* lightManager = LightManager::GetInstance();
	lightManager->ResetToDefaultLighting();
	lightManager->SetShadowCasterLightIndex(-1);
	lightManager->SetManualShadowFocusPosition({ 0.0f, 0.0f, 0.0f });
}

void GamePlayWorld::InitializeSkyBox()
{
	skyBox_ = std::make_unique<K4E::SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");
	K4E::JsonAssetEntry skyBoxPresetEntry;
	if (K4E::JsonDataManager::SafeLoad("Resources/DataAssets/SkyBoxPresets/debug_skybox.json", skyBoxPresetEntry))
	{
		skyBoxPresets_.FromJson(skyBoxPresetEntry.data);
		if (const K4E::SkyBoxPreset* skyBoxPreset = skyBoxPresets_.FindActivePreset()) K4E::ApplySkyBoxPreset(*skyBox_, *skyBoxPreset);
	}
}

void GamePlayWorld::InitializeCollisionSystems()
{
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();
	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize(collisionManager_.get());
}

void GamePlayWorld::InitializeCharacterSystems()
{
	GameContext ctx{};
	ctx.collisionManager_ = collisionManager_.get();
	ctx.bulletManager_ = bulletManager_.get();
	characters_.Initialize(ctx);
	itemManager_.Initialize();
	itemManager_.RegisterColliders(collisionManager_.get());
	characters_.SetEnemyKilledCallback([this](const K4E::Vector3& deathPosition) { itemManager_.TryDropFromEnemyDeath(deathPosition); });
}

void GamePlayWorld::InitializeHUD()
{
	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->Initialize();
	hudManager_->SetWeaponSlotVisibleSlotCount(stage1BeginnerBalanceEnabled_ ? 1 : WeaponSlot::kSlotCount);
	// P13以降のPlayer固有HP/Ammo/CrosshairはPlayerHudPresenterComponentが描画し、HUDManagerはWorld/Tutorial/Boss表示へ集中する。
}

void GamePlayWorld::InitializeStageAndPhysics(const GamePlayStageContext::StageAssetPaths& stageAssets)
{
	stage_ = std::make_unique<K4E::Stage>();
	stage_->Initialize(stageAssets.jsonPath, stageAssets.modelPath);
	stage_->RegisterColliders(collisionManager_.get());
	stage_->Update();
	gameplayPhysicsDebugController_.Initialize(BuildGameplayPhysicsDebugDependencies());
	EnemyBase::SetGlobalStageWorldAABBs(&stage_->GetWorldAABBs());
	EnemyBase::SetGlobalStageFloorAABBs(&stage_->GetFloorAABBs());
	EnemyBase::SetGlobalStageNavigationObstacleAABBs(&stage_->GetNavigationObstacleAABBs());
}

void GamePlayWorld::InitializePlayerSpawn(GamePlayStageContext& stageContext)
{
	stageContext.LoadSpawnPointsFromLevel(stageContext.GetCurrentStageAssets().jsonPath);
	stageObjectiveManager_ = std::make_unique<StageObjectiveManager>();
	stageObjectiveManager_->Initialize(stageContext);

	K4E::Vector3 spawn{ 0.0f, 2.25f, 0.0f };
	if (stageContext.HasPlayerSpawnPoint())
	{
		spawn = stageContext.GetPlayerSpawnPoint();
		spawn.y += 1.0f;
	}
	characters_.SetPlayerSpawnPosition(spawn);
	characters_.UpdatePlayerOnly(0.0f); // Stage生成後にPlayerActorをSpawnし、Runtime Camera/Physicsをこのフレームで確定する。

	if (IPlayerRuntime* player = characters_.GetPlayerRuntime())
	{
		if (K4E::Camera* camera = player->GetCamera())
		{
			K4E::CameraManager::GetInstance()->SetMainCamera(camera);
			camera->SetFarClip(1600.0f);
		}
	}
	if (stage_) stage_->Update();
	CollisionUpdate();
}

void GamePlayWorld::InitializeWaveSystem(GamePlayStageContext& stageContext)
{
	waveManager_ = std::make_unique<WaveManager>();
	if (stageObjectiveManager_ && stageObjectiveManager_->UsesWaveSystem()) stageContext.SetupWaves(waveManager_.get());
	else waveManager_->SetWaves({});
	prevWaveNumber_ = 0;
	prevWaveInProgress_ = false;
	prevAllWavesCleared_ = false;
}

void GamePlayWorld::InitializeBossState(GamePlayStageContext& stageContext)
{
	bossBattleController_.Initialize(stageContext, stage1BeginnerBalanceEnabled_);
}

void GamePlayWorld::InitializeStage1Crystals()
{
	std::vector<CrystalSpawnPoint> crystalSpawnPoints;
	CrystalSpawnPoint center{};
	center.crystalName = "Crystal_01";
	center.position = { 0.0f, 2.0f, 20.0f };
	center.scale = { 1.5f, 2.5f, 1.5f };
	center.hp = center.maxHp = 100;
	center.spawnEnemyType = EnemyType::Melee;
	center.spawnInterval = 2.0f;
	center.maxAliveEnemies = 10;
	center.spawnRadius = 4.0f;
	center.enableInfiniteSpawn = true;
	center.spawnBossTrigger = true;
	crystalSpawnPoints.push_back(center);

	CrystalSpawnPoint right = center;
	right.crystalName = "Crystal_02";
	right.position = { 10.0f, 2.0f, 30.0f };
	right.spawnEnemyType = EnemyType::MidRange;
	right.spawnInterval = 3.0f;
	right.maxAliveEnemies = 6;
	crystalSpawnPoints.push_back(right);

	CrystalSpawnPoint left = center;
	left.crystalName = "BossCrystal_01";
	left.position = { -10.0f, 2.0f, 30.0f };
	crystalSpawnPoints.push_back(left);

	crystalManager_.SetSkyBox(skyBox_.get());
	crystalManager_.Initialize(crystalSpawnPoints, collisionManager_.get(), &stage_->GetFloorAABBs(), &stage_->GetNavigationObstacleAABBs());
	crystalManager_.SetStage1BeginnerBalanceEnabled(stage1BeginnerBalanceEnabled_);
	crystalManager_.SetProgressDebugStatus(characters_.GetAliveNormalEnemyCount(), bossBattleController_.IsSpawnConditionMet(), bossBattleController_.IsSpawned(), bossBattleController_.GetBossSpawnPosition());
}

void GamePlayWorld::InitializeStage2Devices(GamePlayStageContext& stageContext)
{
	stage2DeviceManager_.Initialize(stageContext); // StageRuleが装置起動型の時だけ専用ActorWorldへ3基を生成する。
}

void GamePlayWorld::InitializeRuntimeHelpers()
{
	enemyHpBarManager_.Initialize();
	aimTargetDetector_.Initialize();
	ammoRecoveryItemSpawner_.Initialize();
	stage1TutorialController_.Start(BuildStage1TutorialDependencies(), stage1BeginnerBalanceEnabled_, bossBattleController_.IsDefeated(), skipStage1Tutorial_);
}

void GamePlayWorld::Finalize()
{
	bossBattleController_.Finalize(BuildBossBattleDependencies());
	stage2DeviceManager_.Finalize();
	crystalManager_.Finalize();
	stageObjectiveManager_.reset();
	waveManager_.reset();
	hudManager_.reset();
	EnemyBase::SetGlobalStageWorldAABBs(nullptr);
	EnemyBase::SetGlobalStageFloorAABBs(nullptr);
	EnemyBase::SetGlobalStageNavigationObstacleAABBs(nullptr);
	ammoRecoveryItemSpawner_.Reset();
	itemManager_.Clear();
	gameplayPhysicsDebugController_.Finalize();
	characters_.Finalize();
	stage_.reset();
	bulletManager_.reset();
	if (collisionManager_) collisionManager_->Reset();
	collisionManager_.reset();
	skyBox_.reset();
}

void GamePlayWorld::Update(float deltaTime)
{
	UpdateStageRuntime();
	if (UpdateBlockingStage1Intro(deltaTime)) return;
	if (UpdateBlockingBossIntro(deltaTime)) return;
	UpdateGameplayActors(deltaTime);
	UpdateBossRuntime(deltaTime);
	UpdateItemRuntime(deltaTime);
	UpdateShadowRuntime();
	UpdateBulletAndCollisionRuntime(deltaTime);
	UpdateSkyBoxRuntime(deltaTime);
	UpdateAimTargetRuntime();
	UpdateHpBarRuntime(deltaTime);
	UpdateHudRuntime(deltaTime);
	UpdateWaveRuntime(deltaTime);
	UpdateStageObjectiveRuntime(deltaTime);
}

void GamePlayWorld::UpdateStageRuntime() { if (stage_) stage_->Update(); }

bool GamePlayWorld::UpdateBlockingStage1Intro(float deltaTime)
{
	if (!stage1TutorialController_.IsActive()) return false;
	crystalManager_.SetDifficultyDirectorEnabled(false);
	stage1TutorialController_.Update(BuildStage1TutorialDependencies(), deltaTime);
	return true;
}

bool GamePlayWorld::UpdateBlockingBossIntro(float deltaTime)
{
	if (!bossBattleController_.IsIntroGameplayPaused()) return false;
	bossBattleController_.UpdatePausedWorld(BuildBossBattleDependencies(), deltaTime);
	if (skyBox_) { skyBox_->Update(); skyBox_->AdvanceCloudLayer(deltaTime); }
	UpdateShadowLightViewProjection();
	if (stage_) stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);
	stage2DeviceManager_.UpdateShadowMatrix(shadowLightViewProjection_);
	if (auto* boss = bossBattleController_.GetBoss()) boss->UpdateShadowMatrix(shadowLightViewProjection_);
	return true;
}

void GamePlayWorld::UpdateGameplayActors(float deltaTime)
{
	UpdatePlayerLadderOverlap();
	characters_.Update(deltaTime);
	stage2DeviceManager_.Update(
		characters_.GetPlayerRuntime(),
		K4E::Input::GetInstance(),
		characters_,
		deltaTime,
		[this](const std::string& deviceId)
		{
			NotifyStageDeviceActivated(deviceId); // Device Actorの一度だけの起動イベントを共通Objectiveへ渡す。
		});
	crystalManager_.SetDifficultyDirectorEnabled(!stage1TutorialController_.IsGameplayBlocked());
	crystalManager_.Update(characters_, deltaTime);
	bossBattleController_.UpdateSpawnProgress(BuildBossBattleDependencies());
	bossBattleController_.UpdateIntro(BuildBossBattleDependencies(), deltaTime);
}

void GamePlayWorld::UpdatePlayerLadderOverlap()
{
	// Ladderの新PlayerActor完全Parityは未移行項目としてPLAYER_MIGRATION.mdへ残し、P13では旧Player依存を再導入しない。
}

void GamePlayWorld::UpdateBossRuntime(float deltaTime) { bossBattleController_.UpdateRuntime(BuildBossBattleDependencies(), deltaTime); }

void GamePlayWorld::UpdateItemRuntime(float deltaTime)
{
	IPlayerRuntime* player = characters_.GetPlayerRuntime();
	const bool suppressAmmoRecoverySpawn = stage1TutorialController_.IsGameplayBlocked() || bossBattleController_.IsIntroGameplayPaused() || bossBattleController_.IsGameClearRequested();
	ammoRecoveryItemSpawner_.Update(deltaTime, player, itemManager_, stage_.get(), suppressAmmoRecoverySpawn);
	itemManager_.Update(player);
}

void GamePlayWorld::UpdateShadowRuntime()
{
	UpdateShadowLightViewProjection();
	if (stage_) stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);
	stage2DeviceManager_.UpdateShadowMatrix(shadowLightViewProjection_);
	if (auto* boss = bossBattleController_.GetBoss()) boss->UpdateShadowMatrix(shadowLightViewProjection_);
}

void GamePlayWorld::UpdateBulletAndCollisionRuntime(float deltaTime)
{
	if (bulletManager_)
	{
		const auto begin = std::chrono::steady_clock::now();
		bulletManager_->Update(deltaTime);
		lastBulletUpdateMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
	}
	gameplayPhysicsDebugController_.Update(BuildGameplayPhysicsDebugDependencies(), deltaTime);
	const auto begin = std::chrono::steady_clock::now();
	CollisionUpdate();
	lastCollisionUpdateMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
	UpdateBulletEnemySoAProbe();
}

void GamePlayWorld::UpdateBulletEnemySoAProbe()
{
	lastBulletEnemySoAMs_ = 0.0f;
	lastBulletEnemySoAStats_ = {}; // SoA経路は観測専用のため、Player移行中は本編判定へ影響させない。
}

void GamePlayWorld::UpdateSkyBoxRuntime(float deltaTime)
{
	if (skyBox_) { skyBox_->Update(); skyBox_->AdvanceCloudLayer(deltaTime); }
}

void GamePlayWorld::UpdateAimTargetRuntime()
{
	IPlayerRuntime* player = characters_.GetPlayerRuntime();
	if (!collisionManager_ || !player || !player->GetCamera()) return;
	aimTargetDetector_.Update(*player->GetCamera(), *collisionManager_);
	if (K4E::PlayerActor* actor = characters_.GetPlayer()) actor->SetCrosshairTargeted(aimTargetDetector_.HasDamageableTarget());
}

void GamePlayWorld::UpdateHpBarRuntime(float deltaTime)
{
	IPlayerRuntime* player = characters_.GetPlayerRuntime();
	K4E::Camera* camera = player ? player->GetCamera() : nullptr;
	if (!camera) return;
	const std::vector<EnemyBase*> enemyList = characters_.GetEnemyRawList();
	const float width = static_cast<float>(K4E::GameViewportConstants::Width);
	const float height = static_cast<float>(K4E::GameViewportConstants::Height);
	enemyHpBarManager_.Update(enemyList, camera->GetViewMatrix(), camera->GetProjectionMatrix(), width, height, deltaTime, aimTargetDetector_.GetTargetEnemy(), aimTargetDetector_.ShouldShowHpBarOnlyWhenAimed(), aimTargetDetector_.GetHpBarVisibleHoldTime());
	crystalManager_.UpdateHpBars(camera->GetViewMatrix(), camera->GetProjectionMatrix(), width, height, deltaTime, aimTargetDetector_.GetTargetCrystal(), aimTargetDetector_.ShouldShowHpBarOnlyWhenAimed(), aimTargetDetector_.GetHpBarVisibleHoldTime());
}

void GamePlayWorld::UpdateHudRuntime(float deltaTime)
{
	IPlayerRuntime* player = characters_.GetPlayerRuntime();
	if (!hudManager_ || !player) return;
	hudManager_->SetCrosshairTargetColors(aimTargetDetector_.GetCrosshairNormalColor(), aimTargetDetector_.GetCrosshairTargetColor());
	hudManager_->SetCrosshairTargetingEnemy(aimTargetDetector_.HasDamageableTarget());
	hudManager_->SetHP(player->GetHP(), player->GetMaxHP());
	const auto& devicePrompt = stage2DeviceManager_.GetPromptSnapshot();
	HUDManager::InteractionPromptDisplayState interactionState{};
	interactionState.visible = devicePrompt.visible;
	interactionState.text = devicePrompt.text;
	interactionState.normalizedProgress = devicePrompt.normalizedProgress;
	interactionState.completed = devicePrompt.normalizedProgress >= 1.0f;
	hudManager_->SetInteractionPromptDisplayState(interactionState); // Stage 2の接近操作と長押し進捗をWorld HUDへ同期する。
	const bool bossBattleActive = bossBattleController_.IsBossBattleActive();
	bossBattleController_.UpdateHud(BuildBossBattleDependencies(), deltaTime);
	stage1TutorialController_.UpdateObjectiveGuideHud(BuildStage1TutorialDependencies(), stage1BeginnerBalanceEnabled_, bossBattleActive, bossBattleController_.IsSpawned(), bossBattleController_.HasIntroPlayed(), bossBattleController_.IsDefeated());
	bossBattleController_.UpdateBossGuideHud(*player, *hudManager_);
	hudManager_->Update(deltaTime);
}

void GamePlayWorld::UpdateWaveRuntime(float deltaTime)
{
	if (!stageObjectiveManager_ || !stageObjectiveManager_->UsesWaveSystem() || !waveManager_) return;
	waveManager_->Update(characters_, deltaTime);
	const int currentWave = waveManager_->GetCurrentWaveNumber();
	const int totalWaves = waveManager_->GetTotalWaveCount();
	const bool isWaveInProgress = waveManager_->IsWaveInProgress();
	const bool isWaitingNextWave = waveManager_->IsWaitingNextWave();
	const bool isAllWavesCleared = waveManager_->IsAllWavesCleared();
	const bool isFinalWave = currentWave >= totalWaves;
	if (hudManager_)
	{
		WaveUI::DisplayState state{};
		state.currentWave = currentWave;
		state.totalWaves = totalWaves;
		state.isWaveInProgress = isWaveInProgress;
		state.isWaitingNextWave = isWaitingNextWave;
		state.isAllWavesCleared = isAllWavesCleared;
		hudManager_->SetWaveDisplayState(state);
		if (isWaveInProgress && (!prevWaveInProgress_ || currentWave != prevWaveNumber_)) hudManager_->NotifyWaveStarted(currentWave, isFinalWave);
		if (isAllWavesCleared && !prevAllWavesCleared_) hudManager_->NotifyAllWavesCleared();
	}
	prevWaveNumber_ = currentWave;
	prevWaveInProgress_ = isWaveInProgress;
	prevAllWavesCleared_ = isAllWavesCleared;
}

void GamePlayWorld::UpdateStageObjectiveRuntime(float deltaTime) { if (stageObjectiveManager_) stageObjectiveManager_->Update(deltaTime); }

void GamePlayWorld::UpdateIntroVisuals()
{
	if (stage_) stage_->Update();
	UpdateShadowLightViewProjection();
	if (stage_) stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);
	stage2DeviceManager_.UpdateShadowMatrix(shadowLightViewProjection_);
	if (skyBox_) skyBox_->Update();
}

void GamePlayWorld::UpdateEquipIntro(float deltaTime)
{
	if (stage_) stage_->Update();
	characters_.UpdatePlayerOnly(deltaTime);
	UpdateShadowLightViewProjection();
	if (stage_) stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);
	stage2DeviceManager_.UpdateShadowMatrix(shadowLightViewProjection_);
	if (skyBox_) { skyBox_->Update(); skyBox_->AdvanceCloudLayer(deltaTime); }
}

void GamePlayWorld::Draw3D(bool hideCharactersDuringIntro)
{
#ifdef _DEBUG
	const bool debugVisualsEnabled =
#ifdef USE_IMGUI
		K4E::EditorModeController::GetInstance()->ShouldDrawDebugVisuals();
#else
		true;
#endif
#endif
	K4E::SkyBoxManager::GetInstance()->SetRenderSetting();
	if (skyBox_) { skyBox_->Draw(); skyBox_->DrawCloudLayer(); }
	if (!hideCharactersDuringIntro) { characters_.Draw(); bossBattleController_.DrawBoss(); }
	if (stage_)
	{
		stage_->Draw();
#ifdef _DEBUG
		if (debugVisualsEnabled) stage_->DrawChunkDebug();
#endif
	}
	stage2DeviceManager_.Draw();
	crystalManager_.Draw();
	if (bulletManager_) bulletManager_->Draw();
	itemManager_.Draw();
	bossBattleController_.DrawClearItem();
	gameplayPhysicsDebugController_.Draw();
#ifdef _DEBUG
	if (debugVisualsEnabled && collisionManager_) collisionManager_->Draw();
	if (debugVisualsEnabled) K4E::Wireframe::GetInstance()->DrawGrid(200.0f, 50.0f, { 0.25f, 0.25f, 0.25f, 1.0f });
#endif
}

void GamePlayWorld::DrawBossIntro3D()
{
	K4E::SkyBoxManager::GetInstance()->SetRenderSetting();
	if (skyBox_) { skyBox_->Draw(); skyBox_->DrawCloudLayer(); }
	if (stage_) stage_->Draw();
	bossBattleController_.DrawBossIntro3D();
}

void GamePlayWorld::DrawShadow(bool hideCharactersDuringIntro)
{
	if (stage_) stage_->DrawShadow();
	stage2DeviceManager_.DrawShadow();
	if (!hideCharactersDuringIntro) { characters_.DrawShadow(); bossBattleController_.DrawShadow(); }
}

void GamePlayWorld::DrawBossIntroShadow()
{
	if (stage_) stage_->DrawShadow();
	bossBattleController_.DrawBossIntroShadow();
}

void GamePlayWorld::DrawHUD(bool hideDuringIntro)
{
	if (hideDuringIntro) return;
	crystalManager_.DrawHpBars();
	enemyHpBarManager_.Draw();
	characters_.GetActorWorld().DrawScreenSpaceUI(); // 新PlayerActorのHP/Ammo/CrosshairをGamePlay HUD Passで描画する。
	if (hudManager_) hudManager_->Draw();
}

void GamePlayWorld::DrawImGui() { itemManager_.DrawImGui(); }
void GamePlayWorld::DrawGameDebugImGui() { worldDebugView_.DrawGameDebugImGui(BuildWorldDebugDependencies()); }
void GamePlayWorld::DrawEnemyDebugImGui() { worldDebugView_.DrawEnemyDebugImGui(BuildWorldDebugDependencies()); }
void GamePlayWorld::DrawCollisionDebugImGui() { worldDebugView_.DrawCollisionDebugImGui(BuildWorldDebugDependencies()); }

void GamePlayWorld::SyncAfterPlayerSpawn()
{
	characters_.Update(0.0f);
	if (stage_) stage_->Update();
	CollisionUpdate();
}

void GamePlayWorld::StartWaves()
{
	if (stageObjectiveManager_ && stageObjectiveManager_->UsesWaveSystem() && waveManager_ && !waveManager_->HasStarted()) waveManager_->Start();
}

void GamePlayWorld::WarmupStartGameplayForIntro()
{
	SetStartGameplayVisualsVisible(false);
	StartWaves();
	if (stageObjectiveManager_ && stageObjectiveManager_->UsesWaveSystem() && waveManager_) waveManager_->Update(characters_, 0.0f);
	characters_.WarmupStartGameplayVisuals();
	CollisionUpdate();
}

void GamePlayWorld::SetStartGameplayVisualsVisible(bool visible) { characters_.SetStartGameplayVisualsVisible(visible); }

void GamePlayWorld::SetDebugCameraEnabled(bool enabled)
{
	if (enabled)
	{
		if (K4E::PlayerActor* player = characters_.GetPlayer())
		{
			if (auto* input = player->GetPlayerInputComponent()) input->ResetInputState();
		}
	}
	if (skyBox_) skyBox_->SetDebugCamera(enabled);
	K4E::Wireframe::GetInstance()->SetDebugCamera(enabled);
	if (auto* particleManager = K4E::ParticleManager::GetInstance()) particleManager->SetDebugCamera(enabled);
	if (auto* gpuParticleManager = K4E::GpuParticleManager::GetInstance()) gpuParticleManager->SetDebugCameraEnabled(enabled);
}

bool GamePlayWorld::IsPlayerDead()
{
	const IPlayerRuntime* player = characters_.GetPlayerRuntime();
	return player && player->GetHP() <= 0.0f;
}

bool GamePlayWorld::IsAllWavesCleared() const { return waveManager_ && waveManager_->IsAllWavesCleared(); }

bool GamePlayWorld::IsStageObjectiveCleared() const
{
	if (bossBattleController_.IsGameClearRequested()) return true;
	return stageObjectiveManager_ && stageObjectiveManager_->IsStageObjectiveCleared(IsAllWavesCleared());
}

bool GamePlayWorld::IsStageObjectiveFailed() const { return stageObjectiveManager_ && stageObjectiveManager_->IsStageObjectiveFailed(); }

Stage1TutorialController::Dependencies GamePlayWorld::BuildStage1TutorialDependencies()
{
	Stage1TutorialController::Dependencies deps{};
	deps.characters = &characters_;
	deps.hudManager = hudManager_.get();
	deps.crystalManager = &crystalManager_;
	deps.itemManager = &itemManager_;
	deps.bulletManager = bulletManager_.get();
	deps.collisionManager = collisionManager_.get();
	deps.skyBox = skyBox_.get();
	deps.stage = stage_.get();
	deps.shadowLightViewProjection = &shadowLightViewProjection_;
	deps.collisionUpdate = [this]() { CollisionUpdate(); };
	deps.updateShadowLightViewProjection = [this]() { UpdateShadowLightViewProjection(); };
	return deps;
}

BossBattleController::Dependencies GamePlayWorld::BuildBossBattleDependencies()
{
	BossBattleController::Dependencies deps{};
	deps.characters = &characters_;
	deps.hudManager = hudManager_.get();
	deps.crystalManager = &crystalManager_;
	deps.collisionManager = collisionManager_.get();
	deps.stage = stage_.get();
	deps.shadowLightViewProjection = &shadowLightViewProjection_;
	deps.setBossDefeated = [this](bool defeated) { SetBossDefeated(defeated); };
	deps.updateShadowLightViewProjection = [this]() { UpdateShadowLightViewProjection(); };
	return deps;
}

WorldDebugView::Dependencies GamePlayWorld::BuildWorldDebugDependencies()
{
	WorldDebugView::Dependencies deps{};
	deps.stageObjectiveManager = stageObjectiveManager_.get();
	deps.characters = &characters_;
	deps.aimTargetDetector = &aimTargetDetector_;
	deps.crystalManager = &crystalManager_;
	deps.ammoRecoveryItemSpawner = &ammoRecoveryItemSpawner_;
	deps.collisionManager = collisionManager_.get();
	deps.bulletManager = bulletManager_.get();
	deps.enemyHpBarManager = &enemyHpBarManager_;
	deps.lastBulletUpdateMs = lastBulletUpdateMs_;
	deps.lastCollisionUpdateMs = lastCollisionUpdateMs_;
	deps.isPlayerDead = [this]() { return IsPlayerDead(); };
	deps.drawGameplayPhysicsDebugImGui = [this]() { gameplayPhysicsDebugController_.DrawImGui(BuildGameplayPhysicsDebugDependencies()); };
	deps.drawBossBattleDebugImGui = [this]() { bossBattleController_.DrawImGui(BuildBossBattleDependencies(), IsBossIntroPresentationActive()); };
	return deps;
}

void GamePlayWorld::SetDefenseTargetDestroyed(bool destroyed) { if (stageObjectiveManager_) stageObjectiveManager_->SetDefenseTargetDestroyed(destroyed); }
void GamePlayWorld::AddActivatedDeviceCount(int amount) { if (stageObjectiveManager_) stageObjectiveManager_->AddActivatedDeviceCount(amount); }
void GamePlayWorld::SetReachedGoal(bool reached) { if (stageObjectiveManager_) stageObjectiveManager_->SetReachedGoal(reached); }
void GamePlayWorld::SetBossDefeated(bool defeated) { if (stageObjectiveManager_) stageObjectiveManager_->SetBossDefeated(defeated); }

void GamePlayWorld::CollisionUpdate()
{
	if (!collisionManager_) return;
	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();
}

GameplayPhysicsDebugController::Dependencies GamePlayWorld::BuildGameplayPhysicsDebugDependencies()
{
	GameplayPhysicsDebugController::Dependencies deps{};
	deps.characters = &characters_;
	deps.bulletManager = bulletManager_.get();
	deps.collisionManager = collisionManager_.get();
	deps.stage = stage_.get();
	deps.getBoss = [this]() { return bossBattleController_.GetBoss(); };
	deps.isBossColliderRegistered = [this]() { return bossBattleController_.IsColliderRegistered(); };
	return deps;
}

void GamePlayWorld::UpdateShadowLightViewProjection()
{
	K4E::Vector3 center{};
	if (const IPlayerRuntime* player = characters_.GetPlayerRuntime()) center = player->GetWorldPosition();
	shadowLightViewProjection_ = K4E::LightManager::GetInstance()->BuildShadowLightViewProjection(center);
}

bool GamePlayWorld::TryGetDirectionalLightFromManager(K4E::Vector3& outDirection) const
{
	const auto& lights = K4E::LightManager::GetInstance()->GetPunctualLights();
	for (const auto& light : lights)
	{
		if (light.lightType == 1)
		{
			outDirection = K4E::Vector3::Normalize(light.direction);
			return true;
		}
	}
	return false;
}

bool GamePlayWorld::IsSightBlocked(const K4E::Segment& seg) const
{
	(void)seg;
	return false;
}
