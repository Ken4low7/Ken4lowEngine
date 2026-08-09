#pragma once

#include "BossHudUI.h"
#include "Crosshair.h"
#include "DamageIndicatorManager.h"
#include "HPWidget.h"
#include "ReloadCircle.h"
#include "Stage1ObjectiveGuideUI.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "WaveUI.h"
#include "WeaponSlot.h"

#include <memory>
#include <string>

class IPlayerRuntime;

namespace Ken4lowEngine
{
	class Sprite;
	class TextSpriteDrawer;
}

/// World/Tutorial/Boss表示とPlayerActorの武器HUDを管理する。
class HUDManager
{
public:
	struct StageObjectiveDisplayState
	{
		bool visible = false;
		bool showProgress = false;
		bool cleared = false;
		bool failed = false;
		std::string title;
		std::string detail;
		float normalizedProgress = 0.0f;
	};

	struct InteractionPromptDisplayState
	{
		bool visible = false;
		bool completed = false;
		std::string text;
		float normalizedProgress = 0.0f;
	};

public:
	~HUDManager();

	void Initialize();
	void Update(float deltaTime);
	void Draw();

	void SetPlayerRuntime(IPlayerRuntime* playerRuntime) { playerRuntime_ = playerRuntime; }
	void SetRuntimeWeaponHudVisible(bool visible);
	bool IsRuntimeWeaponHudVisible() const { return runtimeWeaponHudVisible_; }

	void SetBossHP(float hp, float maxHp, bool bossBattleActive);
	void SetStage1ObjectiveGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated, bool tutorialActive);
	void SetStage1ObjectiveTutorialAlpha(float alpha);
	void SetStage1ObjectiveTutorialPage(int page);
	void SetStage1ObjectiveTutorialProgress(float progress);
	void SetStage1TutorialItemMarker(int markerIndex, bool visible, const Ken4lowEngine::Vector2& screenPosition, int itemType);
	void NotifyStage1ObjectiveGuideStarted();
	void NotifyStage1BossAppeared();
	void SetStageObjectiveDisplayState(const StageObjectiveDisplayState& state);
	void SetInteractionPromptDisplayState(const InteractionPromptDisplayState& state);
	void SetBossGuide(const Ken4lowEngine::Vector3& playerPos, const Ken4lowEngine::Vector3& bossPos, const Ken4lowEngine::Vector3& cameraForward, bool bossBattleActive);
	void NotifyBossIntroCompleted(const Ken4lowEngine::Vector3& bossPos);
	void SetWeaponSlotVisibleSlotCount(int count);

	void SetWaveDisplayState(const WaveUI::DisplayState& state);
	void NotifyWaveStarted(int waveNumber, bool isFinalWave);
	void NotifyAllWavesCleared();
	void SetWaveUIVisible(bool visible);
	bool IsWaveUIDrawEnabled() const;

	void AddDamageIndicator(const Ken4lowEngine::Vector3& playerPos,
		const Ken4lowEngine::Vector3& attackerPos,
		const Ken4lowEngine::Vector3& cameraForward,
		const Ken4lowEngine::Vector3& cameraRight);

	void SetReloadCircleVisible(bool visible) { if (reloadCircle_) reloadCircle_->SetVisible(visible); }
	ReloadCircle* GetReloadCircle() const { return reloadCircle_.get(); }
	WeaponSlot* GetWeaponSlot() const { return weaponSlot_.get(); }
	bool IsBossHPBarDrawEnabled() const { return bossHudUI_.IsHpBarDrawEnabled(); }

	// Editorの旧HUD状態表示はAPI互換だけ維持し、PlayerActor UIとの二重確保は行わない。
	HPWidget* GetHPWidget() const { return nullptr; }
	Crosshair* GetCrosshair() const { return nullptr; }
	void SetHP(float, float) {}
	void NotifyPlayerHit(float = 1.0f) {}
	void NotifyEnemyHit(bool = false) {}
	void NotifyEnemyKill(bool = false) {}
	void SetCrosshairMovementState(bool, bool, bool) {}
	void NotifyCrosshairLanded() {}
	void SetCrosshairTargetingEnemy(bool) {}
	void SetCrosshairTargetColors(const Ken4lowEngine::Vector4&, const Ken4lowEngine::Vector4&) {}
	void SetCrosshairVisible(bool) {}
	void SetHPVisible(bool) {}
	void SetControlGuideVisible(bool) {}

private:
	bool UpdateReloadCircleFromRuntime();
	void UpdateWeaponSlotFromRuntime();
	IPlayerRuntime* ResolvePlayerRuntime() const;
	void InitializeStageObjectiveUI();
	void UpdateStageObjectiveUI(float deltaTime);
	void DrawStageObjectiveUI();
	void InitializeInteractionPromptUI();
	void UpdateInteractionPromptUI(float deltaTime);
	void DrawInteractionPromptUI();

	IPlayerRuntime* playerRuntime_ = nullptr;
	bool runtimeWeaponHudVisible_ = true;
	bool prevReloading_ = false;

	std::unique_ptr<ReloadCircle> reloadCircle_;
	std::unique_ptr<WeaponSlot> weaponSlot_;
	std::unique_ptr<WaveUI> waveUI_;
	std::unique_ptr<DamageIndicatorManager> damageIndicatorManager_;
	BossHudUI bossHudUI_;
	Stage1ObjectiveGuideUI stage1ObjectiveGuideUI_;

	StageObjectiveDisplayState stageObjectiveDisplayState_{};
	std::unique_ptr<Ken4lowEngine::Sprite> stageObjectiveBackSprite_;
	std::unique_ptr<Ken4lowEngine::Sprite> stageObjectiveAccentSprite_;
	std::unique_ptr<Ken4lowEngine::Sprite> stageObjectiveProgressBackSprite_;
	std::unique_ptr<Ken4lowEngine::Sprite> stageObjectiveProgressFillSprite_;
	std::unique_ptr<Ken4lowEngine::TextSpriteDrawer> stageObjectiveTextDrawer_;
	bool stageObjectiveTextReady_ = false;
	float stageObjectiveAlpha_ = 0.0f;

	InteractionPromptDisplayState interactionPromptDisplayState_{};
	std::unique_ptr<Ken4lowEngine::Sprite> interactionPromptBackSprite_;
	std::unique_ptr<Ken4lowEngine::Sprite> interactionPromptAccentSprite_;
	std::unique_ptr<Ken4lowEngine::Sprite> interactionPromptProgressBackSprite_;
	std::unique_ptr<Ken4lowEngine::Sprite> interactionPromptProgressFillSprite_;
	float interactionPromptAlpha_ = 0.0f;
};
