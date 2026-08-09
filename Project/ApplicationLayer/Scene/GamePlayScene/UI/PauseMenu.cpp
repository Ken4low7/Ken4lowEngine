#define NOMINMAX
#include "PauseMenu.h"

#include <DirectXCommon.h>
#include <FontAtlasLoader.h>
#include <Input.h>
#include <TextSpriteDrawer.h>
#include <UiSpriteFactory.h>
#include "GameViewportConstants.h"

#include <algorithm>
#include <Windows.h>

namespace K4E = ::Ken4lowEngine;

PauseMenu::~PauseMenu()
{
	if (textDrawer_)
	{
		textDrawer_->Finalize();
	}
}

void PauseMenu::Initialize()
{
	// Pause menuはWinAppサイズに追従せず固定内部解像度1920x1080で構築する。
	screenWidth_ = static_cast<float>(K4E::GameViewportConstants::Width);
	screenHeight_ = static_cast<float>(K4E::GameViewportConstants::Height);

	// UI生成だけをUiSpriteFactoryへ寄せ、入力・選択状態・遷移は既存のPauseMenu側に残す。
	overlay_ = K4E::UiSpriteFactory::CreateWhiteRectSprite();
	panel_ = K4E::UiSpriteFactory::CreateWhiteRectSprite();
	panelBorder_ = K4E::UiSpriteFactory::CreateWhiteRectSprite();
	titleLine_ = K4E::UiSpriteFactory::CreateWhiteRectSprite();

	buttons_.clear();
	buttons_.reserve(items_.size());

	for (int i = 0; i < static_cast<int>(items_.size()); ++i)
	{
		ButtonSprites b{};
		b.bg = K4E::UiSpriteFactory::CreateButtonBackgroundSprite();
		b.border = K4E::UiSpriteFactory::CreateButtonBorderSprite();
		b.accent = K4E::UiSpriteFactory::CreateAccentSprite();
		buttons_.push_back(std::move(b));
	}

	textDrawer_ = std::make_unique<K4E::TextSpriteDrawer>();
	textReady_ = false;
	try
	{
		auto fontDefJP = K4E::FontAtlasLoader::LoadFromJson(
			"UI/Font/JP/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?'
		);
		textDrawer_->Initialize(fontDefJP);
		textReady_ = true;
	}
	catch (...)
	{
		textReady_ = false;
	}

	selectedIndex_ = 0;
	isOpen_ = false;

	RebuildLayout();
	ApplyVisualState();
}

void PauseMenu::Open()
{
	isOpen_ = true;
	selectedIndex_ = 0;
	ApplyVisualState();
}

void PauseMenu::Close()
{
	isOpen_ = false;
}

PauseMenuCommand PauseMenu::Update(Ken4lowEngine::Input* input)
{
	if (!isOpen_ || !input)
	{
		return PauseMenuCommand::None;
	}

	RefreshScreenSizeIfNeeded();

	const int itemCount = static_cast<int>(items_.size());
	if (itemCount <= 0)
	{
		return PauseMenuCommand::None;
	}

	auto moveCursor = [&](int delta)
		{
			selectedIndex_ += delta;
			if (selectedIndex_ < 0) { selectedIndex_ = itemCount - 1; }
			if (selectedIndex_ >= itemCount) { selectedIndex_ = 0; }
		};

	// Pause中はEditorがゲーム入力を解放するため、UI操作はRawキーで受け取る。
	if (input->TriggerRawKey(DIK_W) || input->TriggerRawKey(DIK_UP))
	{
		moveCursor(-1);
	}
	if (input->TriggerRawKey(DIK_S) || input->TriggerRawKey(DIK_DOWN))
	{
		moveCursor(+1);
	}

	// マウスホバーで選択変更
	const K4E::Vector2 mousePos = input->GetMousePosition();
	const int hoverIndex = HitTestButtonIndex(mousePos);
	if (hoverIndex >= 0)
	{
		selectedIndex_ = hoverIndex;
	}

	ApplyVisualState();

	// 決定入力もEditorのgameInputEnabledに依存させず、UI自身でRaw状態を読む。
	const bool decideByKey =
		input->TriggerRawKey(DIK_RETURN) ||
		input->TriggerRawKey(DIK_NUMPADENTER) ||
		input->TriggerRawKey(DIK_SPACE);

	static bool wasLeftMouseDown = false;
	const bool leftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	const bool leftMouseTriggered = leftMouseDown && !wasLeftMouseDown;
	wasLeftMouseDown = leftMouseDown;
	const bool decideByMouse = (hoverIndex >= 0) && leftMouseTriggered;

	if (!(decideByKey || decideByMouse))
	{
		return PauseMenuCommand::None;
	}

	switch (selectedIndex_)
	{
	case 0: return PauseMenuCommand::Resume;
	case 1: return PauseMenuCommand::ToStageSelect;
	case 2: return PauseMenuCommand::ToTitle;
	default: break;
	}

	return PauseMenuCommand::None;
}

void PauseMenu::Draw()
{
	if (!isOpen_)
	{
		return;
	}

	RefreshScreenSizeIfNeeded();
	ApplyVisualState();

	if (overlay_) overlay_->Draw();
	if (panelBorder_) panelBorder_->Draw();
	if (panel_) panel_->Draw();
	if (titleLine_) titleLine_->Draw();

	for (auto& b : buttons_)
	{
		if (b.border) b.border->Draw();
		if (b.bg) b.bg->Draw();
		if (b.accent) b.accent->Draw();
	}

	DrawTexts();
}

void PauseMenu::RebuildLayout()
{
	const float cx = screenWidth_ * 0.5f;
	const float cy = screenHeight_ * 0.5f;

	panelW_ = std::min(screenWidth_ * 0.62f, 760.0f);
	panelH_ = std::min(screenHeight_ * 0.58f, 460.0f);
	panelX_ = cx - panelW_ * 0.5f;
	panelY_ = cy - panelH_ * 0.5f;

	// 全画面オーバーレイ
	if (overlay_)
	{
		K4E::UiSpriteFactory::ApplyRect(overlay_.get(), { 0.0f, 0.0f }, { screenWidth_, screenHeight_ }, { 0.0f, 0.0f });
	}

	// パネル本体
	if (panel_)
	{
		K4E::UiSpriteFactory::ApplyRect(panel_.get(), { panelX_, panelY_ }, { panelW_, panelH_ }, { 0.0f, 0.0f });
	}

	// パネル枠
	if (panelBorder_)
	{
		K4E::UiSpriteFactory::ApplyRect(panelBorder_.get(), { panelX_ - 3.0f, panelY_ - 3.0f }, { panelW_ + 6.0f, panelH_ + 6.0f }, { 0.0f, 0.0f });
	}

	// タイトル下のライン
	if (titleLine_)
	{
		K4E::UiSpriteFactory::ApplyRect(titleLine_.get(), { cx, panelY_ + 86.0f }, { panelW_ - 120.0f, 4.0f }, { 0.5f, 0.0f });
	}

	// ボタン
	const float buttonW = panelW_ - 96.0f;
	const float buttonH = 64.0f;
	const float gap = 16.0f;
	const float startY = panelY_ + 118.0f;

	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		auto& b = buttons_[i];

		const float x = cx - buttonW * 0.5f;
		const float y = startY + i * (buttonH + gap);

		b.rect.left = x;
		b.rect.top = y;
		b.rect.right = x + buttonW;
		b.rect.bottom = y + buttonH;

		if (b.border)
		{
			K4E::UiSpriteFactory::ApplyRect(b.border.get(), { x - 2.0f, y - 2.0f }, { buttonW + 4.0f, buttonH + 4.0f }, { 0.0f, 0.0f });
		}

		if (b.bg)
		{
			K4E::UiSpriteFactory::ApplyRect(b.bg.get(), { x, y }, { buttonW, buttonH }, { 0.0f, 0.0f });
		}

		if (b.accent)
		{
			K4E::UiSpriteFactory::ApplyRect(b.accent.get(), { x + 14.0f, y + buttonH * 0.5f }, { 7.0f, buttonH - 18.0f }, { 0.0f, 0.5f });
		}
	}
}

void PauseMenu::RefreshScreenSizeIfNeeded()
{
	// 固定内部解像度は正値定数なので、定数条件チェックでC4127を出さない。
	const float w = static_cast<float>(K4E::GameViewportConstants::Width);
	const float h = static_cast<float>(K4E::GameViewportConstants::Height);

	if (w != screenWidth_ || h != screenHeight_)
	{
		screenWidth_ = w;
		screenHeight_ = h;
		RebuildLayout();
	}
}

int PauseMenu::HitTestButtonIndex(const K4E::Vector2& mousePos) const
{
	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		if (mousePos.x >= buttons_[i].rect.left && mousePos.x <= buttons_[i].rect.right && mousePos.y >= buttons_[i].rect.top && mousePos.y <= buttons_[i].rect.bottom)
		{
			return i;
		}
	}
	return -1;
}

void PauseMenu::ApplyVisualState()
{
	// 背景・パネル
	if (overlay_) overlay_->SetColor({ 0.0f, 0.0f, 0.0f, 0.62f });
	if (panel_) panel_->SetColor({ 0.045f, 0.040f, 0.035f, 0.94f });
	if (panelBorder_) panelBorder_->SetColor({ 0.95f, 0.48f, 0.14f, 0.95f });
	if (titleLine_) titleLine_->SetColor({ 0.95f, 0.56f, 0.18f, 0.92f });

	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		const bool selected = (i == selectedIndex_);
		auto& b = buttons_[i];

		if (b.bg)
		{
			if (selected) { b.bg->SetColor({ 0.32f, 0.18f, 0.06f, 0.98f }); }
			else { b.bg->SetColor({ 0.12f, 0.11f, 0.10f, 0.95f }); }
		}

		if (b.border)
		{
			if (selected) { b.border->SetColor({ 1.0f, 0.64f, 0.18f, 1.0f }); }
			else { b.border->SetColor({ 0.35f, 0.30f, 0.22f, 1.0f }); }
		}

		if (b.accent)
		{
			if (selected) { b.accent->SetColor({ 1.0f, 0.40f, 0.18f, 0.96f }); }
			else { b.accent->SetColor({ 1.0f, 0.40f, 0.18f, 0.00f }); }
		}
	}
}

void PauseMenu::DrawTexts()
{
	if (!textReady_ || !textDrawer_)
	{
		return;
	}

	const float cx = screenWidth_ * 0.5f;
	textDrawer_->Reset();
	textDrawer_->SetLetterSpacing(2.0f);
	textDrawer_->SetLineSpacing(6.0f);

	textDrawer_->SetScale(1.35f);
	textDrawer_->SetColor({ 1.0f, 0.90f, 0.52f, 1.0f });
	textDrawer_->DrawTextCentered("一時停止", { cx, panelY_ + 34.0f });

	textDrawer_->SetScale(0.92f);
	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		const auto& b = buttons_[i];
		const bool selected = (i == selectedIndex_);
		const float centerX = (b.rect.left + b.rect.right) * 0.5f;
		const float centerY = (b.rect.top + b.rect.bottom) * 0.5f;

		textDrawer_->SetColor(selected ? K4E::Vector4{ 1.0f, 0.96f, 0.78f, 1.0f } : K4E::Vector4{ 0.78f, 0.76f, 0.70f, 1.0f });
		textDrawer_->DrawTextCentered(items_[i], { centerX, centerY - 7.0f });
	}

	textDrawer_->SetScale(0.50f);
	textDrawer_->SetLetterSpacing(1.0f);
	textDrawer_->SetColor({ 0.78f, 0.80f, 0.82f, 0.92f });
	textDrawer_->DrawTextCentered("W / S / ↑ / ↓ 選択    Enter / Click 決定", { cx, panelY_ + panelH_ - 34.0f });
}