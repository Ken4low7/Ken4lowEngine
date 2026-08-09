#pragma once
#include "Vector2.h"
#include "Vector4.h"

#include <array>
#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{
	class Sprite;
	class TextSpriteDrawer;
}

/// -------------------------------------------------------------
/// ステージ1の目的表示とチュートリアル説明を描画するHUD部品。
///
/// HUDManagerからステージ1専用の表示状態・ページ描画を切り離し、
/// HUDManager本体が共通HUDの管理に集中できるようにする。
/// -------------------------------------------------------------
class Stage1ObjectiveGuideUI
{
public:
	Stage1ObjectiveGuideUI() = default;
	~Stage1ObjectiveGuideUI();

	// 背景スプライトと日本語フォントを初期化し、Stage1目的表示を描画できる状態にする。
	void Initialize();

	// 目的表示のフェード、チュートリアル説明、ボス出現通知のタイマーを更新する。
	void Update(float deltaTime);

	// 現在のチュートリアル状態またはステージ目標を画面に描画する。
	void Draw();

	void SetGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated, bool tutorialActive);
	void SetTutorialAlpha(float alpha);
	void SetTutorialPage(int page);
	void SetTutorialProgress(float progress);
	void SetTutorialItemMarker(int markerIndex, bool visible, const K4E::Vector2& screenPosition, int itemType);
	void NotifyGuideStarted();
	void NotifyBossAppeared();

private:
	struct Settings
	{
		bool visible = true;
		K4E::Vector2 center{ 250.0f, 94.0f };
		K4E::Vector2 panelSize{ 380.0f, 54.0f };
		K4E::Vector2 tutorialCenter{ 960.0f, 270.0f };
		K4E::Vector2 tutorialPanelSize{ 920.0f, 260.0f };
		K4E::Vector2 noticeCenter{ 960.0f, 156.0f };
		float titleScale = 0.92f;
		float progressScale = 0.58f;
		float smallScale = 0.54f;
		float noticeScale = 0.78f;
		float introHoldTime = 7.0f;
		float bossNoticeTime = 3.2f;
	};

	struct TutorialItemMarker
	{
		bool visible = false;
		K4E::Vector2 screenPosition{};
		int itemType = 0;
	};

	bool PrepareText();
	void DrawTutorialPage();
	void DrawTutorialCrystalPage();
	void DrawTutorialMovePage();
	void DrawTutorialMouseLookPage();
	void DrawTutorialShootPage();
	void DrawTutorialReloadPage();
	void DrawTutorialEnemyPage();
	void DrawTutorialItemPage();
	void DrawTutorialCompletedPage();
	void DrawTutorialItemMarkers();
	void DrawObjectiveProgress();
	void DrawBossNotice();

	Settings settings_{};
	std::unique_ptr<K4E::Sprite> backSprite_;
	std::unique_ptr<K4E::Sprite> accentSprite_;
	std::unique_ptr<K4E::TextSpriteDrawer> textDrawer_;
	bool textReady_ = false;
	bool enabled_ = false;
	int destroyedCrystals_ = 0;
	int totalCrystals_ = 0;
	bool bossBattleActive_ = false;
	bool bossDefeated_ = false;
	bool tutorialActive_ = false;
	int tutorialPage_ = 0;
	float tutorialAlpha_ = 0.0f;
	float tutorialProgress_ = 0.0f;
	float introTimer_ = 0.0f;
	float alpha_ = 0.0f;
	float bossNoticeTimer_ = 0.0f;
	std::array<TutorialItemMarker, 2> itemMarkers_{};
};
