#define NOMINMAX
#include "Stage1ObjectiveGuideUI.h"
#include "FontAtlasLoader.h"
#include "TextSpriteDrawer.h"
#include <Sprite.h>

#include <algorithm>
#include <cmath>
#include <string>

using namespace Ken4lowEngine;

namespace
{
	std::string ToPercentText(int percent)
	{
		return std::to_string(std::clamp(percent, 0, 100)) + "%";
	}

	std::string BuildProgressBlocks(float progress)
	{
		const int filled = std::clamp(static_cast<int>(std::round(progress * 10.0f)), 0, 10);
		std::string out = "［";
		for (int i = 0; i < 10; ++i)
		{
			out += (i < filled) ? "■" : "□";
		}
		out += "］";
		return out;
	}
}

Stage1ObjectiveGuideUI::~Stage1ObjectiveGuideUI() = default;

void Stage1ObjectiveGuideUI::Initialize()
{
	backSprite_ = std::make_unique<Sprite>();
	backSprite_->Initialize("Effects/white.dds");
	backSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	accentSprite_ = std::make_unique<Sprite>();
	accentSprite_->Initialize("Effects/white.dds");
	accentSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	textDrawer_ = std::make_unique<TextSpriteDrawer>();
	textReady_ = false;
	try
	{
		auto fontDefJP = FontAtlasLoader::LoadFromJson(
			"UI/Font/JP/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?'
		);
		textDrawer_->Initialize(fontDefJP);
		textReady_ = true;
	} catch (...)
	{
		textReady_ = false;
	}
}

void Stage1ObjectiveGuideUI::Update(float deltaTime)
{
	if (introTimer_ > 0.0f)
	{
		introTimer_ = std::max(0.0f, introTimer_ - deltaTime);
	}
	if (bossNoticeTimer_ > 0.0f)
	{
		bossNoticeTimer_ = std::max(0.0f, bossNoticeTimer_ - deltaTime);
	}

	const bool shouldShow = settings_.visible && enabled_ && !bossDefeated_;
	if (tutorialActive_)
	{
		alpha_ = shouldShow ? 0.92f * tutorialAlpha_ : 0.0f;
	}
	else
	{
		const float targetAlpha = shouldShow ? 0.70f : 0.0f;
		const float approach = std::clamp(deltaTime * 8.0f, 0.0f, 1.0f);
		alpha_ += (targetAlpha - alpha_) * approach;
	}

	const Vector2 center = tutorialActive_ ? settings_.tutorialCenter : settings_.center;
	const Vector2 size = tutorialActive_ ? settings_.tutorialPanelSize : settings_.panelSize;
	if (backSprite_)
	{
		backSprite_->SetPosition(center);
		backSprite_->SetSize(size);
		backSprite_->SetColor({ 0.03f, 0.035f, 0.045f, alpha_ * 0.82f });
		backSprite_->Update();
	}
	if (accentSprite_)
	{
		accentSprite_->SetPosition({ center.x, center.y + size.y * 0.5f - 5.0f });
		accentSprite_->SetSize({ size.x * 0.92f, 5.0f });
		accentSprite_->SetColor({ 0.35f, 0.86f, 1.0f, alpha_ });
		accentSprite_->Update();
	}
}

void Stage1ObjectiveGuideUI::Draw()
{
	if (alpha_ <= 0.01f || !enabled_)
	{
		return;
	}

	if (backSprite_) backSprite_->Draw();
	if (accentSprite_) accentSprite_->Draw();

	if (!PrepareText())
	{
		return;
	}

	if (tutorialActive_)
	{
		DrawTutorialPage();
		DrawTutorialItemMarkers();
		return;
	}

	DrawObjectiveProgress();
	DrawBossNotice();
}

void Stage1ObjectiveGuideUI::SetGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated, bool tutorialActive)
{
	enabled_ = enabled;
	destroyedCrystals_ = std::max(0, destroyedCrystals);
	totalCrystals_ = std::max(0, totalCrystals);
	bossBattleActive_ = bossBattleActive;
	bossDefeated_ = bossDefeated;
	tutorialActive_ = tutorialActive;
}

void Stage1ObjectiveGuideUI::SetTutorialAlpha(float alpha)
{
	tutorialAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

void Stage1ObjectiveGuideUI::SetTutorialPage(int page)
{
	tutorialPage_ = page;
}

void Stage1ObjectiveGuideUI::SetTutorialProgress(float progress)
{
	tutorialProgress_ = std::clamp(progress, 0.0f, 1.0f);
}

void Stage1ObjectiveGuideUI::SetTutorialItemMarker(int markerIndex, bool visible, const K4E::Vector2& screenPosition, int itemType)
{
	if (markerIndex < 0 || markerIndex >= static_cast<int>(itemMarkers_.size()))
	{
		return;
	}

	itemMarkers_[markerIndex].visible = visible;
	itemMarkers_[markerIndex].screenPosition = screenPosition;
	itemMarkers_[markerIndex].itemType = itemType;
}

void Stage1ObjectiveGuideUI::NotifyGuideStarted()
{
	introTimer_ = std::max(0.0f, settings_.introHoldTime);
	alpha_ = 0.0f;
}

void Stage1ObjectiveGuideUI::NotifyBossAppeared()
{
	bossNoticeTimer_ = std::max(0.0f, settings_.bossNoticeTime);
}

bool Stage1ObjectiveGuideUI::PrepareText()
{
	if (!textReady_ || !textDrawer_)
	{
		return false;
	}

	textDrawer_->Reset();
	textDrawer_->SetLetterSpacing(1.0f);
	textDrawer_->SetLineSpacing(6.0f);
	return true;
}

void Stage1ObjectiveGuideUI::DrawTutorialPage()
{
	switch (tutorialPage_)
	{
	case 0:
		DrawTutorialCrystalPage();
		break;
	case 1:
		DrawTutorialMovePage();
		break;
	case 2:
		DrawTutorialMouseLookPage();
		break;
	case 3:
		DrawTutorialShootPage();
		break;
	case 4:
		DrawTutorialReloadPage();
		break;
	case 5:
		DrawTutorialEnemyPage();
		break;
	case 6:
		DrawTutorialItemPage();
		break;
	default:
		DrawTutorialCompletedPage();
		break;
	}
}

void Stage1ObjectiveGuideUI::DrawTutorialCrystalPage()
{
	textDrawer_->SetScale(settings_.titleScale);
	textDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("クリスタルを3つ破壊しろ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y - 34.0f
		});
	textDrawer_->SetScale(settings_.progressScale);
	textDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("光っている青い結晶が破壊対象だ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 12.0f
		});
	textDrawer_->DrawTextCentered("すべて破壊するとボスが出現する", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 54.0f
		});
	textDrawer_->SetScale(settings_.smallScale);
	textDrawer_->SetColor({ 1.0f, 0.82f, 0.30f, alpha_ });
	textDrawer_->DrawTextCentered("左クリックで次へ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 96.0f
		});
}

void Stage1ObjectiveGuideUI::DrawTutorialMovePage()
{
	const int percent = static_cast<int>(std::round(tutorialProgress_ * 100.0f));
	textDrawer_->SetScale(settings_.titleScale);
	textDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, alpha_ });
	// 日本語と英数字を分けず、UTF-8文字列としてまとめてTextSpriteDrawerへ渡す。
	textDrawer_->DrawTextCentered("WASDで移動しろ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y - 54.0f
		});
	textDrawer_->SetScale(settings_.progressScale);
	textDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("移動練習：" + ToPercentText(percent), {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 18.0f
		});
	textDrawer_->DrawTextCentered(BuildProgressBlocks(tutorialProgress_), {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 64.0f
		});
}

void Stage1ObjectiveGuideUI::DrawTutorialMouseLookPage()
{
	const int percent = static_cast<int>(std::round(tutorialProgress_ * 100.0f));
	textDrawer_->SetScale(settings_.titleScale);
	textDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("マウスで視点を動かせ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y - 54.0f
		});
	textDrawer_->SetScale(settings_.progressScale);
	textDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("視点移動：" + ToPercentText(percent), {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 18.0f
		});
	textDrawer_->DrawTextCentered(BuildProgressBlocks(tutorialProgress_), {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 64.0f
		});
}

void Stage1ObjectiveGuideUI::DrawTutorialShootPage()
{
	textDrawer_->SetScale(settings_.titleScale);
	textDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("左クリックで射撃しろ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y - 18.0f
		});
	textDrawer_->SetScale(settings_.smallScale);
	textDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("何発か撃って、射撃感覚を確認しろ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 44.0f
		});
}

void Stage1ObjectiveGuideUI::DrawTutorialReloadPage()
{
	textDrawer_->SetScale(settings_.titleScale);
	textDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("Rキーでリロードしろ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y - 18.0f
		});
	textDrawer_->SetScale(settings_.smallScale);
	textDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("リロード完了で次へ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 44.0f
		});
}

void Stage1ObjectiveGuideUI::DrawTutorialEnemyPage()
{
	textDrawer_->SetScale(settings_.titleScale);
	textDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("敵を倒してみろ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y - 20.0f
		});
	textDrawer_->SetScale(settings_.smallScale);
	textDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("出てきた弱い敵を1体倒そう", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 42.0f
		});
}

void Stage1ObjectiveGuideUI::DrawTutorialItemPage()
{
	const int pickedCount = std::clamp(static_cast<int>(std::round(tutorialProgress_ * 2.0f)), 0, 2);
	textDrawer_->SetScale(settings_.titleScale);
	textDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("アイテムを2つ拾え", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y - 48.0f
		});
	textDrawer_->SetScale(settings_.smallScale);
	textDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("アイテムに近づくと自動で拾える", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 8.0f
		});
	textDrawer_->DrawTextCentered("取得：" + std::to_string(pickedCount) + " / 2", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 58.0f
		});
}

void Stage1ObjectiveGuideUI::DrawTutorialCompletedPage()
{
	textDrawer_->SetScale(settings_.titleScale);
	textDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("チュートリアル完了", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y - 20.0f
		});
	textDrawer_->SetScale(settings_.smallScale);
	textDrawer_->SetColor({ 0.42f, 0.90f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered("クリスタルを3つ破壊しろ", {
		settings_.tutorialCenter.x,
		settings_.tutorialCenter.y + 42.0f
		});
}

void Stage1ObjectiveGuideUI::DrawTutorialItemMarkers()
{
	for (const TutorialItemMarker& marker : itemMarkers_)
	{
		if (!marker.visible)
		{
			continue;
		}

		const bool isAmmo = marker.itemType == 1;
		textDrawer_->SetScale(settings_.smallScale);
		textDrawer_->SetColor({ 1.0f, 0.92f, 0.28f, alpha_ });
		// 初心者が拾う対象を見失わないよう、チュートリアル中はアイテム上に説明マーカーを表示する。
		textDrawer_->DrawTextCentered("▼", {
			marker.screenPosition.x,
			marker.screenPosition.y - 54.0f
			});
		textDrawer_->DrawTextCentered(isAmmo ? "弾薬箱" : "回復薬", {
			marker.screenPosition.x,
			marker.screenPosition.y - 18.0f
			});
		textDrawer_->DrawTextCentered(isAmmo ? "弾を補充" : "HPを回復", {
			marker.screenPosition.x,
			marker.screenPosition.y + 16.0f
			});
	}
}

void Stage1ObjectiveGuideUI::DrawObjectiveProgress()
{
	const bool crystalsDone = totalCrystals_ > 0 && destroyedCrystals_ >= totalCrystals_;
	const std::string objective = crystalsDone
		? "目標：ボスを倒せ"
		: ("目標：クリスタル " + std::to_string(destroyedCrystals_) + " / " + std::to_string(totalCrystals_));

	textDrawer_->SetScale(settings_.smallScale);
	textDrawer_->SetColor({ 0.90f, 0.96f, 1.0f, alpha_ });
	textDrawer_->DrawTextCentered(objective, {
		settings_.center.x,
		settings_.center.y - 2.0f
		});
}

void Stage1ObjectiveGuideUI::DrawBossNotice()
{
	if (bossNoticeTimer_ <= 0.0f)
	{
		return;
	}

	const float noticeAlpha = std::clamp(bossNoticeTimer_ / std::max(0.01f, settings_.bossNoticeTime), 0.0f, 1.0f);
	textDrawer_->SetScale(settings_.noticeScale);
	textDrawer_->SetColor({ 1.0f, 0.78f, 0.22f, noticeAlpha });
	textDrawer_->DrawTextCentered("ボスが出現した!", settings_.noticeCenter);
}
