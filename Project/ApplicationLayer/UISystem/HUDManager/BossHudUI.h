#pragma once
#include "Vector2.h"
#include "Vector3.h"

#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{
	class Sprite;
}

/// -------------------------------------------------------------
/// ボス戦中に表示するHPバーと方向ガイドを管理するHUD部品。
///
/// HUDManagerからボス専用の表示状態・ParameterManager連携を切り離し、
/// HUDManager本体が共通HUDの統括に集中できるようにする。
/// -------------------------------------------------------------
class BossHudUI
{
public:
	BossHudUI() = default;
	~BossHudUI();

	// ボスHPバー・方向ガイド用のスプライトと調整パラメータを初期化する。
	void Initialize();

	// ボスHPバーの遅延ゲージと方向ガイドの表示時間を更新する。
	void Update(float deltaTime);

	// ボスHPバーと方向ガイドを現在の表示状態に応じて描画する。
	void Draw();

	void SetBossHP(float hp, float maxHp, bool bossBattleActive);
	void SetBossGuide(const K4E::Vector3& playerPos,
		const K4E::Vector3& bossPos,
		const K4E::Vector3& cameraForward,
		bool bossBattleActive);
	void NotifyBossIntroCompleted(const K4E::Vector3& bossPos);

	bool IsHpBarDrawEnabled() const { return hpBarRuntimeVisible_; }
	bool ShouldHideWaveUI() const { return hpBarSettings_.hideWaveUI && bossBattleActive_; }

private:
	struct HpBarSettings
	{
		bool visible = true;
		K4E::Vector3 position{ 960.0f, 54.0f, 0.0f };
		float width = 760.0f;
		float height = 22.0f;
		K4E::Vector3 nameOffset{ 0.0f, -28.0f, 0.0f };
		std::string displayName = "GUARDIAN";
		bool showAfterIntro = true;
		bool hideWaveUI = true;
	};

	struct GuideSettings
	{
		bool visible = true;
		K4E::Vector2 center{ 960.0f, 540.0f };
		float radius = 155.0f;
		float holdTime = 8.0f;
		float lineThickness = 6.0f;
		float dotSize = 24.0f;
	};

	void RegisterHpBarParameters();
	void ApplyHpBarParameters();
	void InitializeHpBarSprites();
	void InitializeGuideSprites();
	void UpdateHpBarSprites();
	void UpdateGuideSprites(float deltaTime);
	void DrawHpBar();
	void DrawGuide();

	HpBarSettings hpBarSettings_{};
	GuideSettings guideSettings_{};

	std::unique_ptr<K4E::Sprite> hpFrameSprite_;
	std::unique_ptr<K4E::Sprite> hpBackSprite_;
	std::unique_ptr<K4E::Sprite> hpDelaySprite_;
	std::unique_ptr<K4E::Sprite> hpFillSprite_;

	std::unique_ptr<K4E::Sprite> guideLineSprite_;
	std::unique_ptr<K4E::Sprite> guideDotBackSprite_;
	std::unique_ptr<K4E::Sprite> guideDotSprite_;

	bool bossBattleActive_ = false;
	bool hpBarRuntimeVisible_ = false;
	float bossHp_ = 0.0f;
	float bossMaxHp_ = 0.0f;
	float hpRate_ = 0.0f;
	float delayedHpRate_ = 0.0f;

	bool guideActive_ = false;
	float guideTimer_ = 0.0f;
	float guideAngle_ = 0.0f;
	float guideLineLength_ = 0.0f;
	K4E::Vector2 guideLineCenter_{};
	K4E::Vector2 guideDotPosition_{};
	K4E::Vector3 guideBossPosition_{};
};
