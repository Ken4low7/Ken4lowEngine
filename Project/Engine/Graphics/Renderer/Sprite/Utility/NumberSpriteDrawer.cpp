#include "NumberSpriteDrawer.h"
#include <TextureManager.h>
#include <string>
#include <cassert>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void NumberSpriteDrawer::Initialize(const std::string& texturePath, float srcDigitW, float srcDigitH, float drawDigitW, float drawDigitH)
{
	texturePath_ = texturePath;
	srcW_ = srcDigitW;
	srcH_ = srcDigitH;

	if (drawDigitW <= 0.0f) drawDigitW = srcDigitW;
	if (drawDigitH <= 0.0f) drawDigitH = srcDigitH;
	drawW_ = drawDigitW;
	drawH_ = drawDigitH;

	TextureManager::GetInstance()->LoadTexture(texturePath_);
	reusable_.clear();
	currentIndex_ = 0;
}

void NumberSpriteDrawer::SetDrawDigitSize(float drawDigitW, float drawDigitH)
{
	if (drawDigitW <= 0.0f || drawDigitH <= 0.0f) return;
	drawW_ = drawDigitW;
	drawH_ = drawDigitH;
}

/// -------------------------------------------------------------
///				　	 左詰めで数字を描画
/// -------------------------------------------------------------
void NumberSpriteDrawer::DrawNumberLeftAligned(int value, const Vector2& pos, float spacing)
{
	std::string s = std::to_string(value);
	float x = pos.x;

	const int kCols = 5; // 0-9 が 5列×2行 前提

	for (char c : s)
	{
		int d = c - '0';
		if (d < 0 || d > 9) continue;

		if (currentIndex_ >= reusable_.size())
		{
			auto sp = std::make_unique<Sprite>();
			sp->Initialize(texturePath_);
			sp->SetAnchorPoint({ 0.0f, 0.0f }); // 左上
			reusable_.push_back(std::move(sp));
		}

		auto& sp = reusable_[currentIndex_++];

		// ★毎回表示サイズを反映（これが無いとサイズ変更が効かない）
		sp->SetSize({ drawW_, drawH_ });

		int row = d / kCols;
		int col = d % kCols;

		Vector2 uv{ col * srcW_, row * srcH_ };
		sp->SetTextureLeftTop(uv);
		sp->SetTextureSize({ srcW_, srcH_ });

		sp->SetPosition({ x, pos.y });
		sp->Update();
		sp->Draw();

		// ★次の桁：表示幅 + spacing
		x += drawW_ + spacing;
	}
}

/// -------------------------------------------------------------
///				　	 中央揃えで数字を描画
/// -------------------------------------------------------------
void NumberSpriteDrawer::DrawNumberCentered(int value, const Vector2& centerPosition, float spacing)
{
	std::string s = std::to_string(value);
	int len = (int)s.size();
	float total = len * drawW_ + (len - 1) * spacing;
	float startX = centerPosition.x - total * 0.5f;
	DrawNumberLeftAligned(value, { startX, centerPosition.y }, spacing);
}

/// -------------------------------------------------------------
///				　	 右詰めで数字を描画
/// -------------------------------------------------------------
void NumberSpriteDrawer::DrawNumberRightAligned(int value, Vector2 rightPosition, float spacing)
{
	std::string s = std::to_string(value);
	int len = (int)s.size();
	float total = len * drawW_ + (len - 1) * spacing;
	float startX = rightPosition.x - total;
	DrawNumberLeftAligned(value, { startX, rightPosition.y }, spacing);
}

} // namespace Ken4lowEngine
