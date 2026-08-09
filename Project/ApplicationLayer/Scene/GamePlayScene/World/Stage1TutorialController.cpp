#define NOMINMAX
#include "Stage1TutorialController.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "BulletManager.h"
#include "CollisionManager.h"
#include "CrystalManager.h"
#include "EnemyBase.h"
#include "EnemyHPBarProjector.h"
#include "GameViewportConstants.h"
#include "HUDManager.h"
#include "Input.h"
#include "ItemManager.h"
#include "Stage.h"
#include <SkyBox.h>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kMovePracticeRequiredDistance = 7.0f;
	constexpr float kMovePracticeMinInputTime = 1.7f;
	constexpr float kMouseLookPracticeRequiredAmount = 1400.0f;
	constexpr float kMouseLookPracticeMinInputTime = 1.7f;
	constexpr int kShootPracticeRequiredShots = 6;
	constexpr float kDefaultStepAdvanceDelay = 0.65f;

	bool CalcLookAnglesToTargetForStage1(const K4E::Vector3& from, const K4E::Vector3& target, float& outPitch, float& outYaw)
	{
		K4E::Vector3 direction = target - from;
		if (K4E::Vector3::LengthSquared(direction) <= 0.000001f) return false;
		direction = K4E::Vector3::Normalize(direction);
		outYaw = std::atan2(-direction.x, direction.z);
		const float xzLen = std::sqrt(direction.x * direction.x + direction.z * direction.z);
		outPitch = std::atan2(-direction.y, xzLen);
		return true;
	}
}

void Stage1TutorialController::Start(const Dependencies& deps, bool beginnerBalanceEnabled, bool bossDefeated, bool skipTutorial)
{
	if (!beginnerBalanceEnabled || !deps.hudManager || !deps.crystalManager || !deps.itemManager) return;
	beginnerBalanceEnabled_ = beginnerBalanceEnabled;
	latestBossDefeated_ = bossDefeated;

	if (skipTutorial)
	{
		tutorialStep_ = TutorialStep::Completed;
		objectiveIntroActive_ = false;
		tutorialSeen_ = true;
		deps.hudManager->SetStage1ObjectiveTutorialAlpha(0.0f);
		deps.hudManager->SetStage1ObjectiveTutorialPage(0);
		deps.hudManager->SetStage1ObjectiveTutorialProgress(0.0f);
		deps.hudManager->SetStage1TutorialItemMarker(0, false, {}, 0);
		deps.hudManager->SetStage1TutorialItemMarker(1, false, {}, 0);
		UpdateObjectiveGuideHud(deps, beginnerBalanceEnabled_, false, false, false, bossDefeated);
		return;
	}

	objectiveIntroActive_ = true;
	objectiveIntroTimer_ = 0.0f;
	tutorialStep_ = TutorialStep::CrystalExplanation;
	moveProgress_ = 0.0f;
	movePracticeTimer_ = 0.0f;
	movePracticeDistance_ = 0.0f;
	mouseLookProgress_ = 0.0f;
	mouseLookPracticeTimer_ = 0.0f;
	mouseLookAmount_ = 0.0f;
	shootProgress_ = 0.0f;
	shootCount_ = 0;
	pendingStepAdvance_ = false;
	stepAdvanceDelayTimer_ = 0.0f;
	stepAdvanceDelay_ = 0.0f;
	tutorialEnemy_ = nullptr;
	tutorialEnemySpawned_ = false;
	tutorialItemSpawned_ = false;
	tutorialItemsCollected_ = 0;
	savedEnemyDeathDropEnabled_ = deps.itemManager->IsEnemyDeathDropEnabled();
	deps.itemManager->SetEnemyDeathDropEnabled(false);
	reloadStarted_ = false;
	reloadWasReloading_ = false;
	tutorialCompletionNotified_ = false;
	tutorialCompleteTimer_ = 0.0f;
	deps.hudManager->SetStage1ObjectiveGuide(true, deps.crystalManager->GetDestroyedCrystalCount(), deps.crystalManager->GetCrystalCount(), false, bossDefeated, true);
	deps.hudManager->SetStage1ObjectiveTutorialAlpha(1.0f);
	deps.hudManager->SetStage1ObjectiveTutorialPage(0);
	deps.hudManager->SetStage1ObjectiveTutorialProgress(0.0f);
	deps.hudManager->NotifyStage1ObjectiveGuideStarted();

	if (deps.characters)
	{
		if (IPlayerRuntime* player = deps.characters->GetPlayerRuntime())
		{
			if (K4E::Camera* camera = player->GetCamera()) savedCameraRotation_ = camera->GetRotate();
			AlignPlayerViewToFirstCrystal(deps, *player);
			movePreviousPlayerPosition_ = player->GetWorldPosition();
		}
	}
}

void Stage1TutorialController::Update(const Dependencies& deps, float deltaTime)
{
	if (!objectiveIntroActive_ || !deps.characters || !deps.crystalManager) return;
	objectiveIntroTimer_ += deltaTime;
	float tutorialAlpha = 1.0f;

	deps.crystalManager->UpdatePresentationOnly(*deps.characters, deltaTime);
	deps.crystalManager->SetFirstAliveCrystalGuideHighlight(tutorialStep_ == TutorialStep::CrystalExplanation ? tutorialAlpha : 0.0f);

	auto* input = K4E::Input::GetInstance();
	const bool clickedNext = input && (input->TriggerMouse(0) || input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) || input->TriggerButton(K4E::XButtons.A));
	if (tutorialStep_ == TutorialStep::CrystalExplanation && clickedNext) AdvanceStep(deps);

	ApplyPlayerRestrictions(deps);
	IPlayerRuntime* player = deps.characters->GetPlayerRuntime();
	if (player)
	{
		if (tutorialStep_ == TutorialStep::MovePractice)
		{
			const K4E::Vector3 before = player->GetWorldPosition();
			deps.characters->UpdatePlayerOnly(deltaTime);
			const K4E::Vector3 after = player->GetWorldPosition();
			const K4E::Vector3 delta = after - before;
			const float movedDistance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
			if (movedDistance > 0.001f && !pendingStepAdvance_)
			{
				movePracticeTimer_ += deltaTime;
				movePracticeDistance_ += movedDistance;
			}
			moveProgress_ = std::min(std::clamp(movePracticeDistance_ / kMovePracticeRequiredDistance, 0.0f, 1.0f), std::clamp(movePracticeTimer_ / kMovePracticeMinInputTime, 0.0f, 1.0f));
			movePreviousPlayerPosition_ = after;
			if (moveProgress_ >= 1.0f && !pendingStepAdvance_) RequestAdvanceStep(kDefaultStepAdvanceDelay);
		}
		else if (tutorialStep_ == TutorialStep::MouseLookPractice)
		{
			deps.characters->UpdatePlayerOnly(deltaTime);
			const float mouseLookAmount = input ? (std::fabs(static_cast<float>(input->GetMouseMoveX())) + std::fabs(static_cast<float>(input->GetMouseMoveY()))) : 0.0f;
			if (mouseLookAmount > 0.0f && !pendingStepAdvance_)
			{
				mouseLookPracticeTimer_ += deltaTime;
				mouseLookAmount_ += mouseLookAmount;
			}
			mouseLookProgress_ = std::min(std::clamp(mouseLookAmount_ / kMouseLookPracticeRequiredAmount, 0.0f, 1.0f), std::clamp(mouseLookPracticeTimer_ / kMouseLookPracticeMinInputTime, 0.0f, 1.0f));
			if (mouseLookProgress_ >= 1.0f && !pendingStepAdvance_) RequestAdvanceStep(kDefaultStepAdvanceDelay);
		}
		else if (tutorialStep_ == TutorialStep::ShootPractice)
		{
			const int magazineBefore = player->GetMagazineAmmo();
			deps.characters->UpdatePlayerOnly(deltaTime);
			const int magazineAfter = player->GetMagazineAmmo();
			if (magazineAfter < magazineBefore)
			{
				++shootCount_;
				shootProgress_ = std::clamp(static_cast<float>(shootCount_) / static_cast<float>(kShootPracticeRequiredShots), 0.0f, 1.0f);
				player->AddReserveAmmo(3);
			}
			if (deps.bulletManager) deps.bulletManager->Update(deltaTime);
			if (shootProgress_ >= 1.0f && !pendingStepAdvance_) RequestAdvanceStep(kDefaultStepAdvanceDelay);
		}
		else if (tutorialStep_ == TutorialStep::ReloadPractice)
		{
			deps.characters->UpdatePlayerOnly(deltaTime);
			const bool isReloading = player->IsReloading();
			if (isReloading) reloadStarted_ = true;
			if (reloadStarted_ && reloadWasReloading_ && !isReloading && !pendingStepAdvance_) RequestAdvanceStep(kDefaultStepAdvanceDelay);
			reloadWasReloading_ = isReloading;
		}
		else if (tutorialStep_ == TutorialStep::EnemyPractice)
		{
			SpawnTutorialEnemy(deps);
			deps.characters->Update(deltaTime);
			if (deps.bulletManager) deps.bulletManager->Update(deltaTime);
			if (deps.collisionManager && deps.collisionUpdate) deps.collisionUpdate();
			if (tutorialEnemy_ && tutorialEnemy_->IsDead() && !pendingStepAdvance_) RequestAdvanceStep(kDefaultStepAdvanceDelay);
		}
		else if (tutorialStep_ == TutorialStep::ItemPickupPractice)
		{
			SpawnTutorialItems(deps);
			deps.characters->UpdatePlayerOnly(deltaTime);
			if (deps.itemManager)
			{
				deps.itemManager->Update(player);
				if (deps.itemManager->ConsumeCollected(ItemType::AmmoSmall)) ++tutorialItemsCollected_;
				if (deps.itemManager->ConsumeCollected(ItemType::HealSmall)) ++tutorialItemsCollected_;
			}
			if (tutorialItemsCollected_ >= 2 && !pendingStepAdvance_) RequestAdvanceStep(0.0f);
		}
		else if (tutorialStep_ == TutorialStep::Completed)
		{
			tutorialCompleteTimer_ += deltaTime;
			tutorialAlpha = 1.0f - std::clamp(tutorialCompleteTimer_ / std::max(0.01f, tutorialCompleteHoldTime_), 0.0f, 1.0f);
			if (tutorialCompleteTimer_ >= tutorialCompleteHoldTime_) { Finish(deps); return; }
		}
	}

	UpdatePendingStepAdvance(deps, deltaTime);
	UpdatePresentation(deps, deltaTime, tutorialAlpha);
}

void Stage1TutorialController::Finish(const Dependencies& deps)
{
	objectiveIntroActive_ = false;
	tutorialStep_ = TutorialStep::Completed;
	pendingStepAdvance_ = false;
	tutorialSeen_ = true;
	if (deps.crystalManager) deps.crystalManager->SetFirstAliveCrystalGuideHighlight(0.0f);
	if (deps.itemManager)
	{
		deps.itemManager->SetEnemyDeathDropEnabled(savedEnemyDeathDropEnabled_);
		deps.itemManager->SetConsumeItemWhenFull(false);
	}
	if (deps.hudManager)
	{
		deps.hudManager->SetStage1ObjectiveTutorialAlpha(0.0f);
		deps.hudManager->SetStage1ObjectiveTutorialPage(0);
		deps.hudManager->SetStage1ObjectiveTutorialProgress(0.0f);
		deps.hudManager->SetStage1TutorialItemMarker(0, false, {}, 0);
		deps.hudManager->SetStage1TutorialItemMarker(1, false, {}, 0);
	}
	UpdateObjectiveGuideHud(deps, beginnerBalanceEnabled_, false, false, false, latestBossDefeated_);
}

void Stage1TutorialController::UpdateObjectiveGuideHud(const Dependencies& deps, bool beginnerBalanceEnabled, bool bossBattleActive, bool bossSpawned, bool bossIntroPlayed, bool bossDefeated)
{
	latestBossDefeated_ = bossDefeated;
	if (!deps.hudManager || !deps.crystalManager) return;
	deps.hudManager->SetStage1ObjectiveGuide(beginnerBalanceEnabled, deps.crystalManager->GetDestroyedCrystalCount(), deps.crystalManager->GetCrystalCount(), bossBattleActive || bossSpawned || bossIntroPlayed, bossDefeated, objectiveIntroActive_);
}

bool Stage1TutorialController::IsTutorialPlaying() const { return objectiveIntroActive_ && tutorialStep_ != TutorialStep::None && tutorialStep_ != TutorialStep::Completed; }
bool Stage1TutorialController::IsGameplayBlocked() const { return objectiveIntroActive_ && tutorialStep_ != TutorialStep::Completed; }

void Stage1TutorialController::AdvanceStep(const Dependencies& deps)
{
	switch (tutorialStep_)
	{
	case TutorialStep::CrystalExplanation:
		tutorialStep_ = TutorialStep::MouseLookPractice;
		moveProgress_ = 0.0f;
		movePracticeTimer_ = 0.0f;
		movePracticeDistance_ = 0.0f;
		if (deps.characters && deps.characters->GetPlayerRuntime()) movePreviousPlayerPosition_ = deps.characters->GetPlayerRuntime()->GetWorldPosition();
		break;
	case TutorialStep::MouseLookPractice:
		tutorialStep_ = TutorialStep::MovePractice;
		shootProgress_ = 0.0f;
		shootCount_ = 0;
		break;
	case TutorialStep::MovePractice:
		tutorialStep_ = TutorialStep::ShootPractice;
		mouseLookProgress_ = 0.0f;
		mouseLookPracticeTimer_ = 0.0f;
		mouseLookAmount_ = 0.0f;
		break;
	case TutorialStep::ShootPractice:
		tutorialStep_ = TutorialStep::ReloadPractice;
		reloadStarted_ = false;
		reloadWasReloading_ = false;
		if (deps.characters && deps.characters->GetPlayerRuntime()) PrepareReloadPractice(*deps.characters->GetPlayerRuntime());
		break;
	case TutorialStep::ReloadPractice: tutorialStep_ = TutorialStep::EnemyPractice; break;
	case TutorialStep::EnemyPractice:
		ClearTutorialEnemy(deps);
		tutorialStep_ = TutorialStep::ItemPickupPractice;
		tutorialItemsCollected_ = 0;
		tutorialItemSpawned_ = false;
		break;
	case TutorialStep::ItemPickupPractice:
		tutorialStep_ = TutorialStep::Completed;
		tutorialCompleteTimer_ = 0.0f;
		tutorialCompletionNotified_ = true;
		tutorialSeen_ = true;
		break;
	default: break;
	}
	pendingStepAdvance_ = false;
	stepAdvanceDelayTimer_ = 0.0f;
	stepAdvanceDelay_ = 0.0f;
	objectiveIntroTimer_ = 0.0f;
}

void Stage1TutorialController::RequestAdvanceStep(float delay)
{
	if (pendingStepAdvance_) return;
	pendingStepAdvance_ = true;
	stepAdvanceDelayTimer_ = 0.0f;
	stepAdvanceDelay_ = std::max(0.0f, delay);
}

void Stage1TutorialController::UpdatePendingStepAdvance(const Dependencies& deps, float deltaTime)
{
	if (!pendingStepAdvance_) return;
	stepAdvanceDelayTimer_ += deltaTime;
	if (stepAdvanceDelayTimer_ >= stepAdvanceDelay_) AdvanceStep(deps);
}

bool Stage1TutorialController::AllowsPlayerMove() const
{
	return tutorialStep_ == TutorialStep::ItemPickupPractice || tutorialStep_ == TutorialStep::MovePractice || tutorialStep_ == TutorialStep::MouseLookPractice || tutorialStep_ == TutorialStep::ShootPractice || tutorialStep_ == TutorialStep::ReloadPractice || tutorialStep_ == TutorialStep::EnemyPractice;
}

bool Stage1TutorialController::AllowsPlayerShoot() const { return tutorialStep_ == TutorialStep::ShootPractice || tutorialStep_ == TutorialStep::EnemyPractice; }
bool Stage1TutorialController::AllowsReload() const { return tutorialStep_ == TutorialStep::ReloadPractice || tutorialStep_ == TutorialStep::EnemyPractice; }

void Stage1TutorialController::PrepareReloadPractice(IPlayerRuntime& player)
{
	if (player.GetMagazineCapacity() > 0 && player.GetReserveAmmo() <= 0) player.AddReserveAmmo(player.GetMagazineCapacity());
}

void Stage1TutorialController::ApplyPlayerRestrictions(const Dependencies& deps)
{
	(void)deps; // PlayerTutorialRestrictionBridgeが現在StepをPlayerInputComponentへ直接渡すため、旧Player API呼び出しは不要。
}

void Stage1TutorialController::SpawnTutorialEnemy(const Dependencies& deps)
{
	if (tutorialEnemySpawned_ || !deps.characters || !deps.characters->GetPlayerRuntime()) return;
	const K4E::Vector3 playerPos = deps.characters->GetPlayerRuntime()->GetWorldPosition();
	EnemyBase& enemy = deps.characters->SpawnEnemyAt({ playerPos.x, playerPos.y, playerPos.z + 8.0f }, EnemyType::Melee);
	enemy.SetMaxHp(60);
	enemy.SetCurrentHp(60);
	tutorialEnemy_ = &enemy;
	tutorialEnemySpawned_ = true;
}

void Stage1TutorialController::ClearTutorialEnemy(const Dependencies& deps)
{
	if (tutorialEnemy_ && deps.characters) deps.characters->RemoveEnemy(tutorialEnemy_);
	tutorialEnemy_ = nullptr;
	tutorialEnemySpawned_ = false;
}

void Stage1TutorialController::SpawnTutorialItems(const Dependencies& deps)
{
	if (tutorialItemSpawned_ || !deps.characters || !deps.itemManager || !deps.characters->GetPlayerRuntime()) return;
	const K4E::Vector3 playerPos = deps.characters->GetPlayerRuntime()->GetWorldPosition();
	deps.itemManager->SetConsumeItemWhenFull(true);
	deps.itemManager->SpawnAmmoSmall({ playerPos.x + 2.2f, playerPos.y, playerPos.z + 4.0f });
	deps.itemManager->SpawnHealSmall({ playerPos.x - 2.2f, playerPos.y, playerPos.z + 4.0f });
	deps.itemManager->RegisterColliders(deps.collisionManager);
	tutorialItemSpawned_ = true;
}

void Stage1TutorialController::UpdateTutorialHud(const Dependencies& deps)
{
	if (!deps.hudManager || !deps.itemManager || !deps.characters) return;
	int page = 0;
	float progress = 0.0f;
	switch (tutorialStep_)
	{
	case TutorialStep::CrystalExplanation: page = 0; break;
	case TutorialStep::MovePractice: page = 1; progress = moveProgress_; break;
	case TutorialStep::MouseLookPractice: page = 2; progress = mouseLookProgress_; break;
	case TutorialStep::ShootPractice: page = 3; progress = shootProgress_; break;
	case TutorialStep::ReloadPractice: page = 4; break;
	case TutorialStep::EnemyPractice: page = 5; break;
	case TutorialStep::ItemPickupPractice: page = 6; progress = std::clamp(static_cast<float>(tutorialItemsCollected_) / 2.0f, 0.0f, 1.0f); break;
	case TutorialStep::Completed: page = 7; break;
	default: break;
	}
	deps.hudManager->SetStage1ObjectiveTutorialPage(page);
	deps.hudManager->SetStage1ObjectiveTutorialProgress(progress);
	deps.hudManager->SetStage1TutorialItemMarker(0, false, {}, 0);
	deps.hudManager->SetStage1TutorialItemMarker(1, false, {}, 0);
	if (tutorialStep_ != TutorialStep::ItemPickupPractice) return;

	auto projectMarker = [&deps](int markerIndex, ItemType itemType, int markerType)
		{
			K4E::Vector3 itemPosition{};
			if (!deps.itemManager->TryGetFirstActiveItemPosition(itemType, itemPosition)) return;
			IPlayerRuntime* player = deps.characters->GetPlayerRuntime();
			K4E::Camera* camera = player ? player->GetCamera() : nullptr;
			if (!camera) return;
			const float width = static_cast<float>(K4E::GameViewportConstants::Width);
			const float height = static_cast<float>(K4E::GameViewportConstants::Height);
			const K4E::Vector3 markerWorld{ itemPosition.x, itemPosition.y + 0.5f, itemPosition.z };
			const HpBarProjectResult projected = ProjectWorldToScreen(markerWorld, camera->GetViewMatrix(), camera->GetProjectionMatrix(), width, height);
			deps.hudManager->SetStage1TutorialItemMarker(markerIndex, projected.inFront && projected.inScreen, projected.screenPos, markerType);
		};
	projectMarker(0, ItemType::AmmoSmall, 1);
	projectMarker(1, ItemType::HealSmall, 0);
}

void Stage1TutorialController::AlignPlayerViewToFirstCrystal(const Dependencies& deps, IPlayerRuntime& player)
{
	K4E::Camera* camera = player.GetCamera();
	if (!camera || !deps.crystalManager) return;
	K4E::Vector3 crystalPosition{};
	if (!deps.crystalManager->TryGetFirstAliveCrystalPosition(crystalPosition)) return;
	crystalPosition.y += 1.8f;
	float pitch = 0.0f;
	float yaw = 0.0f;
	if (CalcLookAnglesToTargetForStage1(camera->GetTranslate(), crystalPosition, pitch, yaw))
	{
		player.SetViewLookAngles(pitch, yaw);
		camera->Update();
	}
}

void Stage1TutorialController::UpdatePresentation(const Dependencies& deps, float deltaTime, float tutorialAlpha)
{
	if (deps.skyBox)
	{
		deps.skyBox->Update();
		deps.skyBox->AdvanceCloudLayer(deltaTime);
	}
	if (deps.updateShadowLightViewProjection) deps.updateShadowLightViewProjection();
	if (deps.stage && deps.shadowLightViewProjection) deps.stage->UpdateShadowMatrix(*deps.shadowLightViewProjection);
	if (deps.characters && deps.shadowLightViewProjection) deps.characters->UpdateShadowMatrix(*deps.shadowLightViewProjection);

	if (deps.hudManager && deps.characters && deps.crystalManager)
	{
		IPlayerRuntime* player = deps.characters->GetPlayerRuntime();
		K4E::Camera* camera = player ? player->GetCamera() : nullptr;
		if (camera)
		{
			const float width = static_cast<float>(K4E::GameViewportConstants::Width);
			const float height = static_cast<float>(K4E::GameViewportConstants::Height);
			deps.crystalManager->UpdateHpBars(camera->GetViewMatrix(), camera->GetProjectionMatrix(), width, height, deltaTime, tutorialStep_ == TutorialStep::CrystalExplanation ? deps.crystalManager->GetFirstAliveCrystal() : nullptr, true, 0.3f);
		}
		if (player) deps.hudManager->SetHP(player->GetHP(), player->GetMaxHP());
		deps.hudManager->SetBossHP(0.0f, 0.0f, false);
		deps.hudManager->SetStage1ObjectiveTutorialAlpha(tutorialAlpha);
		UpdateTutorialHud(deps);
		deps.hudManager->Update(deltaTime);
	}
}
