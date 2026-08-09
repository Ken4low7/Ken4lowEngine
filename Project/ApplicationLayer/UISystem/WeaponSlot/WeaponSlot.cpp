#include "WeaponSlot.h"
#include "GameViewportConstants.h"
#include <DirectXCommon.h>

#include <cstdlib> // std::abs
#include <Vector2.h>
#include <string>
#include <Sprite.h>
#include <memory>
#include <array>
#include <algorithm>

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
void WeaponSlot::Initialize(const std::string& frameTex, const std::string& selectedTex, const Layout& layout)
{
	frameTex_ = frameTex;
	selectedTex_ = selectedTex;
	layout_ = layout;

	for (int i = 0; i < kSlotCount; ++i)
	{
		frame_[i] = std::make_unique<Sprite>();
		frameSelected_[i] = std::make_unique<Sprite>();

		frame_[i]->Initialize(frameTex_);
		frameSelected_[i]->Initialize(selectedTex_);
	}

	RebuildLayout();
}

void WeaponSlot::InitializeSlotNumbers(const std::string& numberTex, float srcDigitWidth, float srcDigitHeight, const Vector2& offset, float spacing, float drawDigitWidth, float drawDigitHeight)
{
	drawSlotNumbers_ = !numberTex.empty();
	if (!drawSlotNumbers_) { return; }

	numberOffset_ = offset;
	numberSpacing_ = spacing;

	// draw が未指定なら src と同じ
	if (drawDigitWidth <= 0.0f)  drawDigitWidth = srcDigitWidth;
	if (drawDigitHeight <= 0.0f) drawDigitHeight = srcDigitHeight;

	numberDigitW_ = drawDigitWidth;
	numberDigitH_ = drawDigitHeight;

	// 切り出しサイズ(src)と表示サイズ(draw)を分けて初期化
	numberDrawer_.Initialize(numberTex, srcDigitWidth, srcDigitHeight, drawDigitWidth, drawDigitHeight);
}

/// -------------------------------------------------------------
///				　	弾薬番号初期化処理
/// -------------------------------------------------------------
void WeaponSlot::InitializeAmmoNumbers(const std::string& numberTex, float srcDigitW, float srcDigitH, const Vector2& padding, float spacing, float drawDigitW, float drawDigitH)
{
	drawAmmo_ = !numberTex.empty();
	if (!drawAmmo_) return;

	ammoPadding_ = padding;
	ammoSpacing_ = spacing;

	if (drawDigitW <= 0.0f) drawDigitW = srcDigitW;
	if (drawDigitH <= 0.0f) drawDigitH = srcDigitH;

	ammoDigitW_ = drawDigitW;
	ammoDigitH_ = drawDigitH;

	ammoDrawer_.Initialize(numberTex, srcDigitW, srcDigitH, drawDigitW, drawDigitH);
}

/// -------------------------------------------------------------
///				　	弾薬区切り文字初期化処理
/// -------------------------------------------------------------
void WeaponSlot::InitializeAmmoDelimiter(const std::string& slashTex, const Vector2& size, const Vector2& offset)
{
	// いったん全スロット消す
	for (int i = 0; i < kSlotCount; ++i) ammoSlash_[i].reset();

	// テクスチャ無ければ終了
	if (slashTex.empty()) return;

	// スロット分作る（使い回ししない）
	for (int i = 0; i < kSlotCount; ++i)
	{
		ammoSlash_[i] = std::make_unique<Sprite>();
		ammoSlash_[i]->Initialize(slashTex);
		ammoSlash_[i]->SetAnchorPoint({ 0.5f, 0.5f });
	}

	ammoSlashOffset_ = offset;

	// サイズ未指定なら数字サイズから自動
	if (size.x <= 0.0f || size.y <= 0.0f)
	{
		ammoSlashSize_ = { ammoDigitW_ * 0.6f, ammoDigitH_ };
	}
	else
	{
		ammoSlashSize_ = size;
	}
}

/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void WeaponSlot::Update(const HudSnapshot& hud)
{
	selectedIndex_ = std::clamp(hud.selectedIndex, -1, kSlotCount - 1);
	if (selectedIndex_ >= visibleSlotCount_)
	{
		selectedIndex_ = -1;
	}
	RebuildLayout();

	if (!drawAmmo_) return;

	if (ammoInfos_.size() != kSlotCount) ammoInfos_.resize(kSlotCount);

	for (int i = 0; i < kSlotCount; ++i)
	{
		ammoUses_[i] = hud.slotStates[i].useAmmo;
		ammoInfos_[i] = hud.slotStates[i].ammoInfo;
	}
}

/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void WeaponSlot::Draw()
{
	if (drawSlotNumbers_) numberDrawer_.Reset();
	if (drawAmmo_)       ammoDrawer_.Reset();

	for (int i = 0; i < visibleSlotCount_; ++i)
	{
		const bool selected = (i == selectedIndex_);
		if (selected) frameSelected_[i]->Draw();
		else          frame_[i]->Draw();

		if (icon_[i]) icon_[i]->Draw();

		if (drawAmmo_ && i < static_cast<int>(ammoInfos_.size()) && ammoUses_[i])
		{
			const Vector2 base = frame_[i]->GetPosition();
			const Vector2 size = frame_[i]->GetSize();

			const float y = base.y + size.y - ammoDigitH_ - ammoPadding_.y;

			// 桁数→幅
			auto CountDigits = [](int v) -> int {
				v = std::abs(v);
				int d = 1;
				while (v >= 10) { v /= 10; ++d; }
				return d;
				};
			auto WidthForDigits = [&](int digits) -> float {
				if (digits <= 0) digits = 1;
				return digits * ammoDigitW_ + (digits - 1) * ammoSpacing_;
				};

			const int dCur = CountDigits(ammoInfos_[i].currentAmmo);
			const int dRes = CountDigits(ammoInfos_[i].reserveAmmo);
			const float curW = WidthForDigits(dCur);
			const float resW = WidthForDigits(dRes);

			// 左右の安全領域（padding）
			const float boundL = base.x + ammoPadding_.x;
			const float boundR = base.x + size.x - ammoPadding_.x;

			// “数字 / 数字” の詰め具合（ここを 0〜4 くらいで調整）
			const float slashPad = -2.0f;

			// スラッシュ幅（無ければ 0 扱い）
			const float slashW = (ammoSlash_[i] ? ammoSlashSize_.x : 0.0f);

			// グループ全体幅（残弾 + パッド + / + パッド + 予備弾）
			const float groupW = curW + slashPad + slashW + slashPad + resW;

			// グループをスロット中央に置く（はみ出すならクランプ）
			float groupL = (base.x + size.x * 0.5f) - groupW * 0.5f;
			const float maxL = boundR - groupW;

			if (maxL < boundL)
			{
				// そもそも収まらないので clamp しない（左寄せなど安全な位置に）
				groupL = boundL;
			}
			else
			{
				groupL = std::clamp(groupL, boundL, maxL);
			}

			// 残弾：/ の左に “右寄せ” で配置
			const float curRightEdgeX = groupL + curW;
			ammoDrawer_.DrawNumberRightAligned(ammoInfos_[i].currentAmmo, { curRightEdgeX, y }, ammoSpacing_);

			// /：残弾の右端 + pad + 半分
			if (ammoSlash_[i])
			{
				const float slashCX = curRightEdgeX + slashPad + slashW * 0.5f;
				const float slashY = y + ammoDigitH_ * 0.5f;

				ammoSlash_[i]->SetPosition({ slashCX + ammoSlashOffset_.x, slashY + ammoSlashOffset_.y });
				ammoSlash_[i]->SetSize(ammoSlashSize_);
				ammoSlash_[i]->Update();
				ammoSlash_[i]->Draw();
			}

			// 予備弾：/ の右に “左寄せ” で配置
			const float resLeftX = curRightEdgeX + slashPad + slashW + slashPad;
			ammoDrawer_.DrawNumberLeftAligned(ammoInfos_[i].reserveAmmo, { resLeftX, y }, ammoSpacing_);
		}

		// スロット番号（必要なら最後に）
		if (drawSlotNumbers_)
		{
			const Vector2 base = frame_[i]->GetPosition();
			numberDrawer_.DrawNumberLeftAligned(i + 1, { base.x + numberOffset_.x, base.y + numberOffset_.y }, numberSpacing_);
		}
	}
}

void WeaponSlot::SetVisibleSlotCount(int count)
{
	visibleSlotCount_ = std::clamp(count, 1, kSlotCount);
	if (selectedIndex_ >= visibleSlotCount_)
	{
		selectedIndex_ = -1;
	}
	RebuildLayout();
}

void WeaponSlot::InitializeIcons(const std::array<std::string, kSlotCount>& iconTex)
{
	for (int i = 0; i < kSlotCount; ++i)
	{
		// 空文字ならアイコン無しにできる
		if (iconTex[i].empty())
		{
			icon_[i].reset();
			continue;
		}

		icon_[i] = std::make_unique<Sprite>();
		icon_[i]->Initialize(iconTex[i]);

		// 中央基準で置くとレイアウトが楽
		icon_[i]->SetAnchorPoint({ 0.5f, 0.5f });
	}

	RebuildLayout(); // 位置反映
}

void WeaponSlot::InitializeIcons(const std::string& iconTex)
{
	std::array<std::string, kSlotCount> tex{};
	tex.fill(iconTex);
	InitializeIcons(tex);
}

/// -------------------------------------------------------------
///				　		レイアウト再構築処理
/// -------------------------------------------------------------
void WeaponSlot::RebuildLayout()
{
	const float slot = layout_.slotSize;
	const float space = layout_.spacing;
	const int visibleSlotCount = std::clamp(visibleSlotCount_, 1, kSlotCount);
	const float totalW = visibleSlotCount * slot + (visibleSlotCount - 1) * space;

	// Weapon slot HUDは固定内部解像度1920x1080の下端を基準にする。
	float screenW = static_cast<float>(GameViewportConstants::Width);
	float screenH = static_cast<float>(GameViewportConstants::Height);

	const float startX = (screenW - totalW) * 0.5f;
	const float y = screenH - layout_.marginBottom - slot;

	const float iconScale = 0.78f;
	const float iconSize = slot * iconScale;
	const float iconOffsetY = -10.0f; // 少し下げる（数字と干渉しない）

	for (int i = 0; i < visibleSlotCount; ++i)
	{
		const float x = startX + i * (slot + space);

		// ↓ここもあなたのSprite APIに合わせてください（Vector2型など）
		frame_[i]->SetPosition({ x, y });
		frame_[i]->SetSize({ slot, slot });
		frame_[i]->Update();

		frameSelected_[i]->SetPosition({ x, y });
		frameSelected_[i]->SetSize({ slot, slot });
		frameSelected_[i]->Update();

		if (icon_[i])
		{
			// スロットの中心（＋少し下）
			const float cx = x + slot * 0.5f;
			const float cy = y + slot * 0.5f + iconOffsetY;

			icon_[i]->SetPosition({ cx, cy });
			icon_[i]->SetSize({ iconSize, iconSize });
			icon_[i]->Update();
		}
	}
}
