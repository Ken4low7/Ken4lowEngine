#pragma once

#include "CharacterWorld.h"
#include "ItemType.h"
#include "ApplicationLayer/Character/Player/Migration/PlayerTutorialRestrictionBridge.h"
#include <Object3D.h>

#include <functional>

namespace K4E = ::Ken4lowEngine;

class IPlayerRuntime;
class BulletManager;
class CollisionManager;
class CrystalManager;
class HUDManager;
class ItemManager;
class EnemyBase;
namespace Ken4lowEngine
{
	class SkyBox;
	class Stage;
}

/// ステージ1の初心者向けチュートリアル進行を管理する。
class Stage1TutorialController
{
public:
	Stage1TutorialController()
	{
		PlayerTutorialRestrictionBridge::SetProvider([this]()
			{
				PlayerTutorialRestrictionBridge::State state{};
				state.enabled = IsGameplayBlocked();
				state.allowMove = AllowsPlayerMove();
				state.allowShoot = AllowsPlayerShoot();
				state.allowReload = AllowsReload();
				state.allowWeaponSwitch = !beginnerBalanceEnabled_; // Stage1ではTutorial完了後もPrimary以外の選択を禁止する。
				return state; // P13でも入力制限の正本はTutorial状態とし、PlayerActorへ直接反映する。
			});
	}

	~Stage1TutorialController() { PlayerTutorialRestrictionBridge::ClearProvider(); }

	struct Dependencies
	{
		CharacterWorld* characters = nullptr;
		HUDManager* hudManager = nullptr;
		CrystalManager* crystalManager = nullptr;
		ItemManager* itemManager = nullptr;
		BulletManager* bulletManager = nullptr;
		CollisionManager* collisionManager = nullptr;
		K4E::SkyBox* skyBox = nullptr;
		K4E::Stage* stage = nullptr;
		K4E::Matrix4x4* shadowLightViewProjection = nullptr;
		std::function<void()> collisionUpdate;
		std::function<void()> updateShadowLightViewProjection;
	};

	void Start(const Dependencies& deps, bool beginnerBalanceEnabled, bool bossDefeated, bool skipTutorial = false);
	void Update(const Dependencies& deps, float deltaTime);
	void Finish(const Dependencies& deps);
	void UpdateObjectiveGuideHud(const Dependencies& deps, bool beginnerBalanceEnabled, bool bossBattleActive, bool bossSpawned, bool bossIntroPlayed, bool bossDefeated);

	bool IsActive() const { return objectiveIntroActive_; }
	bool IsTutorialPlaying() const;
	bool IsGameplayBlocked() const;
	bool HasCompletedTutorial() const { return tutorialSeen_; }

private:
	enum class TutorialStep
	{
		None,
		CrystalExplanation,
		MovePractice,
		MouseLookPractice,
		ShootPractice,
		ReloadPractice,
		EnemyPractice,
		ItemPickupPractice,
		Completed,
	};

	void AdvanceStep(const Dependencies& deps);
	void RequestAdvanceStep(float delay);
	void UpdatePendingStepAdvance(const Dependencies& deps, float deltaTime);
	bool AllowsPlayerMove() const;
	bool AllowsPlayerShoot() const;
	bool AllowsReload() const;
	void PrepareReloadPractice(IPlayerRuntime& player);
	void ApplyPlayerRestrictions(const Dependencies& deps);
	void SpawnTutorialEnemy(const Dependencies& deps);
	void ClearTutorialEnemy(const Dependencies& deps);
	void SpawnTutorialItems(const Dependencies& deps);
	void UpdateTutorialHud(const Dependencies& deps);
	void AlignPlayerViewToFirstCrystal(const Dependencies& deps, IPlayerRuntime& player);
	void UpdatePresentation(const Dependencies& deps, float deltaTime, float tutorialAlpha);

	bool beginnerBalanceEnabled_ = false;
	bool latestBossDefeated_ = false;
	bool objectiveIntroActive_ = false;
	float objectiveIntroTimer_ = 0.0f;
	TutorialStep tutorialStep_ = TutorialStep::None;
	float moveProgress_ = 0.0f;
	float movePracticeTimer_ = 0.0f;
	float movePracticeDistance_ = 0.0f;
	float mouseLookProgress_ = 0.0f;
	float mouseLookPracticeTimer_ = 0.0f;
	float mouseLookAmount_ = 0.0f;
	float shootProgress_ = 0.0f;
	int shootCount_ = 0;
	bool pendingStepAdvance_ = false;
	float stepAdvanceDelayTimer_ = 0.0f;
	float stepAdvanceDelay_ = 0.0f;
	K4E::Vector3 movePreviousPlayerPosition_{};
	K4E::Vector3 savedCameraRotation_{};
	EnemyBase* tutorialEnemy_ = nullptr;
	bool tutorialEnemySpawned_ = false;
	bool tutorialItemSpawned_ = false;
	int tutorialItemsCollected_ = 0;
	bool savedEnemyDeathDropEnabled_ = true;
	bool reloadStarted_ = false;
	bool reloadWasReloading_ = false;
	bool tutorialCompletionNotified_ = false;
	bool tutorialSeen_ = false;
	float tutorialCompleteTimer_ = 0.0f;
	float tutorialCompleteHoldTime_ = 1.4f;
};
