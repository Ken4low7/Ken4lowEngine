#define NOMINMAX
#include "ResultMenu.h"
#include "DirectXCommon.h"
#include "GameViewportConstants.h"
#include "Input.h"
#include "UiSpriteFactory.h"

#include <GameTimer.h>
#include <FontAtlasLoader.h>
#include <TextSpriteDrawer.h>

#include <algorithm>
#include <cmath>
#include <Windows.h>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kHeaderY = -170.0f;
	constexpr float kButtonGap = 102.0f;
	constexpr float kSelectionAnimSpeed = 8.0f;
}

ResultMenu::~ResultMenu()
{
	if (textDrawer_)
	{
		textDrawer_->Finalize();
	}
}

void ResultMenu::Initialize()
{
	// Result UIは固定内部解像度1920x1080全体にレイアウトする。
	const float screenW = static_cast<float>(GameViewportConstants::Width);
	const float screenH = static_cast<float>(GameViewportConstants::Height);
	const float centerX = screenW * 0.5f;
	const float centerY = screenH * 0.5f;

	// UI生成だけをUiSpriteFactoryへ寄せ、入力・選択状態・遷移・アニメーションは既存のResultMenu側に残す。
	backdrop_ = UiSpriteFactory::CreateWhiteRectSprite({ 0.0f, 0.0f }, { screenW, screenH }, { 0.0f, 0.0f });
	backdrop_->SetColor({ 0.0f, 0.0f, 0.0f, 0.55f });
	backdrop_->Update();

	headerBorder_ = UiSpriteFactory::CreateWhiteRectSprite({ centerX, centerY + kHeaderY }, { 560.0f, 104.0f }, { 0.5f, 0.5f });
	headerBody_ = UiSpriteFactory::CreateWhiteRectSprite({ centerX, centerY + kHeaderY }, { 548.0f, 92.0f }, { 0.5f, 0.5f });
	headerLine_ = UiSpriteFactory::CreateWhiteRectSprite({ centerX, centerY + kHeaderY + 38.0f }, { 460.0f, 4.0f }, { 0.5f, 0.5f });

	nextStageButton_.border = UiSpriteFactory::CreateButtonBorderSprite({ 0.5f, 0.5f });
	nextStageButton_.body = UiSpriteFactory::CreateButtonBackgroundSprite({ 0.5f, 0.5f });
	nextStageButton_.accentLeft = UiSpriteFactory::CreateAccentSprite({ 0.5f, 0.5f });
	nextStageButton_.accentRight = UiSpriteFactory::CreateAccentSprite({ 0.5f, 0.5f });
	nextStageButton_.basePosition = { centerX, centerY - 20.0f };
	nextStageButton_.baseSize = { 360.0f, 74.0f };
	nextStageButton_.text = "次のステージへ";

	retryButton_.border = UiSpriteFactory::CreateButtonBorderSprite({ 0.5f, 0.5f });
	retryButton_.body = UiSpriteFactory::CreateButtonBackgroundSprite({ 0.5f, 0.5f });
	retryButton_.accentLeft = UiSpriteFactory::CreateAccentSprite({ 0.5f, 0.5f });
	retryButton_.accentRight = UiSpriteFactory::CreateAccentSprite({ 0.5f, 0.5f });
	retryButton_.basePosition = { centerX, centerY + 82.0f };
	retryButton_.baseSize = { 360.0f, 74.0f };
	retryButton_.text = "リトライ";

	titleButton_.border = UiSpriteFactory::CreateButtonBorderSprite({ 0.5f, 0.5f });
	titleButton_.body = UiSpriteFactory::CreateButtonBackgroundSprite({ 0.5f, 0.5f });
	titleButton_.accentLeft = UiSpriteFactory::CreateAccentSprite({ 0.5f, 0.5f });
	titleButton_.accentRight = UiSpriteFactory::CreateAccentSprite({ 0.5f, 0.5f });
	titleButton_.basePosition = { centerX, centerY + 184.0f };
	titleButton_.baseSize = { 360.0f, 74.0f };
	titleButton_.text = "タイトルへ";

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

	selectedIndex_ = 0;
	selectionAnimTime_ = 0.0f;
	RefreshButtonVisuals();

	isOpen_ = false;
}

void ResultMenu::Update()
{
	if (!isOpen_)
	{
		return;
	}

	selectionAnimTime_ += GameTimer::GetInstance()->GetDeltaTime();

	if (backdrop_) backdrop_->Update();
	if (headerBorder_) headerBorder_->Update();
	if (headerBody_) headerBody_->Update();
	if (headerLine_) headerLine_->Update();
	RefreshButtonVisuals();
}

void ResultMenu::Open(ResultMenuMode mode)
{
	mode_ = mode;
	isOpen_ = true;
	selectedIndex_ = 0;
	selectionAnimTime_ = 0.0f;
	RefreshButtonVisuals();
}

void ResultMenu::Close()
{
	isOpen_ = false;
}

bool ResultMenu::IsMouseInside(const Vector2& mousePos,
	const Vector2& center,
	const Vector2& size) const
{
	const float left = center.x - size.x * 0.5f;
	const float right = center.x + size.x * 0.5f;
	const float top = center.y - size.y * 0.5f;
	const float bottom = center.y + size.y * 0.5f;

	return (mousePos.x >= left && mousePos.x <= right &&
		mousePos.y >= top && mousePos.y <= bottom);
}

void ResultMenu::UpdateButtonVisual(Button& button, bool selected, const Vector4& accentColor)
{
	const float pulse = selected ? (0.5f + 0.5f * std::sin(selectionAnimTime_ * kSelectionAnimSpeed)) : 0.0f;
	const float scale = selected ? (1.08f + 0.03f * pulse) : 1.0f;
	const Vector2 size = { button.baseSize.x * scale, button.baseSize.y * scale };
	const Vector2 borderSize = { size.x + (selected ? 12.0f : 6.0f), size.y + (selected ? 12.0f : 6.0f) };
	const Vector2 accentSize = { selected ? 26.0f : 18.0f, size.y - 18.0f };
	const Vector2 leftAccentPos = { button.basePosition.x - size.x * 0.5f + 22.0f, button.basePosition.y };
	const Vector2 rightAccentPos = { button.basePosition.x + size.x * 0.5f - 22.0f, button.basePosition.y };

	button.selected = selected;
	button.textScale = selected ? (0.86f + 0.04f * pulse) : 0.78f;
	button.textColor = selected ? Vector4{ 1.0f, 0.98f, 0.72f, 1.0f } : Vector4{ 0.90f, 0.88f, 0.76f, 1.0f };

	// 矩形配置だけをUiSpriteFactoryへ寄せる。拡縮・点滅・色は既存ResultMenuのアニメーション値をそのまま使う。
	UiSpriteFactory::ApplyRect(button.border.get(), button.basePosition, borderSize, { 0.5f, 0.5f });
	UiSpriteFactory::ApplyRect(button.body.get(), button.basePosition, size, { 0.5f, 0.5f });
	UiSpriteFactory::ApplyRect(button.accentLeft.get(), leftAccentPos, accentSize, { 0.5f, 0.5f });
	UiSpriteFactory::ApplyRect(button.accentRight.get(), rightAccentPos, accentSize, { 0.5f, 0.5f });

	if (button.border)
	{
		button.border->SetColor(selected ? Vector4{ accentColor.x, accentColor.y, accentColor.z, 0.85f + 0.15f * pulse } : Vector4{ 0.42f, 0.38f, 0.30f, 0.95f });
		button.border->Update();
	}
	if (button.body)
	{
		button.body->SetColor(selected ? Vector4{ 0.28f, 0.22f, 0.12f, 0.98f } : Vector4{ 0.08f, 0.075f, 0.065f, 0.92f });
		button.body->Update();
	}
	if (button.accentLeft)
	{
		button.accentLeft->SetColor(selected ? Vector4{ accentColor.x, accentColor.y, accentColor.z, 1.0f } : Vector4{ accentColor.x, accentColor.y, accentColor.z, 0.70f });
		button.accentLeft->Update();
	}
	if (button.accentRight)
	{
		button.accentRight->SetColor(selected ? Vector4{ accentColor.x, accentColor.y, accentColor.z, 1.0f } : Vector4{ accentColor.x, accentColor.y, accentColor.z, 0.70f });
		button.accentRight->Update();
	}
}

void ResultMenu::RefreshButtonVisuals()
{
	const int visibleCount = GetVisibleButtonCount();
	if (visibleCount <= 0)
	{
		return;
	}

	selectedIndex_ = std::clamp(selectedIndex_, 0, visibleCount - 1);

	// 選択中のボタンだけ拡縮と点滅を行い、リザルト画面の操作対象を分かりやすくする。
	if (mode_ == ResultMenuMode::GameClear)
	{
		UpdateButtonVisual(nextStageButton_, selectedIndex_ == 0, { 0.52f, 0.82f, 0.90f, 1.0f });
		UpdateButtonVisual(retryButton_, selectedIndex_ == 1, { 0.95f, 0.82f, 0.36f, 1.0f });
		UpdateButtonVisual(titleButton_, selectedIndex_ == 2, { 0.96f, 0.42f, 0.42f, 1.0f });
	}
	else
	{
		UpdateButtonVisual(retryButton_, selectedIndex_ == 0, { 0.95f, 0.82f, 0.36f, 1.0f });
		UpdateButtonVisual(titleButton_, selectedIndex_ == 1, { 0.96f, 0.42f, 0.42f, 1.0f });
	}
}

int ResultMenu::GetVisibleButtonCount() const
{
	return (mode_ == ResultMenuMode::GameClear) ? 3 : 2;
}

int ResultMenu::HitTestVisibleButtonIndex(const Vector2& mousePos) const
{
	if (mode_ == ResultMenuMode::GameClear)
	{
		if (IsMouseInside(mousePos, nextStageButton_.basePosition, nextStageButton_.baseSize)) { return 0; }
		if (IsMouseInside(mousePos, retryButton_.basePosition, retryButton_.baseSize)) { return 1; }
		if (IsMouseInside(mousePos, titleButton_.basePosition, titleButton_.baseSize)) { return 2; }
		return -1;
	}

	if (IsMouseInside(mousePos, retryButton_.basePosition, retryButton_.baseSize)) { return 0; }
	if (IsMouseInside(mousePos, titleButton_.basePosition, titleButton_.baseSize)) { return 1; }
	return -1;
}

ResultMenuCommand ResultMenu::GetCommandByVisibleIndex(int index) const
{
	if (mode_ == ResultMenuMode::GameClear)
	{
		switch (index)
		{
		case 0: return ResultMenuCommand::NextStage;
		case 1: return ResultMenuCommand::Retry;
		case 2: return ResultMenuCommand::ToTitle;
		default: break;
		}
	}
	else
	{
		switch (index)
		{
		case 0: return ResultMenuCommand::Retry;
		case 1: return ResultMenuCommand::ToTitle;
		default: break;
		}
	}

	return ResultMenuCommand::None;
}

ResultMenuCommand ResultMenu::Update(Input* input)
{
	if (!isOpen_ || !input)
	{
		return ResultMenuCommand::None;
	}

	const int visibleCount = GetVisibleButtonCount();
	if (visibleCount <= 0)
	{
		return ResultMenuCommand::None;
	}

	// Result中はEditorがゲーム入力を解放するため、UI操作はRawキーで受け取る。
	if (input->TriggerRawKey(DIK_W) || input->TriggerRawKey(DIK_UP))
	{
		selectedIndex_ = (selectedIndex_ + visibleCount - 1) % visibleCount;
		selectionAnimTime_ = 0.0f;
	}
	if (input->TriggerRawKey(DIK_S) || input->TriggerRawKey(DIK_DOWN))
	{
		selectedIndex_ = (selectedIndex_ + 1) % visibleCount;
		selectionAnimTime_ = 0.0f;
	}

	const Vector2 mousePos = input->GetMousePosition();
	const int hoverIndex = HitTestVisibleButtonIndex(mousePos);
	if (hoverIndex >= 0 && hoverIndex != selectedIndex_)
	{
		selectedIndex_ = hoverIndex;
		selectionAnimTime_ = 0.0f;
	}

	RefreshButtonVisuals();

	const bool decideByKey =
		input->TriggerRawKey(DIK_RETURN) ||
		input->TriggerRawKey(DIK_NUMPADENTER) ||
		input->TriggerRawKey(DIK_SPACE);
	static bool wasLeftMouseDown = false;
	const bool leftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	const bool leftMouseTriggered = leftMouseDown && !wasLeftMouseDown;
	wasLeftMouseDown = leftMouseDown;
	const bool decideByMouse = (hoverIndex >= 0) && leftMouseTriggered;

	if (decideByKey || decideByMouse)
	{
		return GetCommandByVisibleIndex(selectedIndex_);
	}

	return ResultMenuCommand::None;
}

void ResultMenu::DrawButton(const Button& button) const
{
	if (button.border) button.border->Draw();
	if (button.body) button.body->Draw();
	if (button.accentLeft) button.accentLeft->Draw();
	if (button.accentRight) button.accentRight->Draw();
}

void ResultMenu::Draw()
{
	if (!isOpen_) { return; }

	if (backdrop_) { backdrop_->Draw(); }

	const bool isClear = (mode_ == ResultMenuMode::GameClear);
	if (headerBorder_)
	{
		headerBorder_->SetColor(isClear ? Vector4{ 0.52f, 0.82f, 0.90f, 0.98f } : Vector4{ 0.96f, 0.42f, 0.42f, 0.98f });
		headerBorder_->Update();
		headerBorder_->Draw();
	}
	if (headerBody_)
	{
		headerBody_->SetColor(isClear ? Vector4{ 0.08f, 0.14f, 0.15f, 0.94f } : Vector4{ 0.16f, 0.07f, 0.07f, 0.94f });
		headerBody_->Update();
		headerBody_->Draw();
	}
	if (headerLine_)
	{
		headerLine_->SetColor(isClear ? Vector4{ 0.80f, 0.96f, 1.0f, 0.80f } : Vector4{ 1.0f, 0.62f, 0.62f, 0.80f });
		headerLine_->Update();
		headerLine_->Draw();
	}

	if (mode_ == ResultMenuMode::GameClear)
	{
		DrawButton(nextStageButton_);
	}
	DrawButton(retryButton_);
	DrawButton(titleButton_);
	DrawTexts();
}

void ResultMenu::DrawTexts()
{
	if (!textReady_ || !textDrawer_)
	{
		return;
	}

	const float screenW = static_cast<float>(GameViewportConstants::Width);
	const float screenH = static_cast<float>(GameViewportConstants::Height);
	const float centerX = screenW * 0.5f;
	const float centerY = screenH * 0.5f;
	const bool isClear = (mode_ == ResultMenuMode::GameClear);

	textDrawer_->Reset();
	textDrawer_->SetLetterSpacing(2.0f);
	textDrawer_->SetLineSpacing(6.0f);

	textDrawer_->SetScale(1.35f);
	textDrawer_->SetColor(isClear ? Vector4{ 0.88f, 0.98f, 1.0f, 1.0f } : Vector4{ 1.0f, 0.80f, 0.80f, 1.0f });
	textDrawer_->DrawTextCentered(isClear ? "ステージクリア" : "ゲームオーバー", { centerX, centerY + kHeaderY - 12.0f });

	const auto drawButtonText = [this](const Button& button)
		{
			textDrawer_->SetScale(button.textScale);
			textDrawer_->SetColor(button.textColor);
			textDrawer_->DrawTextCentered(button.text, { button.basePosition.x, button.basePosition.y - 7.0f });
		};

	if (isClear)
	{
		drawButtonText(nextStageButton_);
	}
	drawButtonText(retryButton_);
	drawButtonText(titleButton_);
}