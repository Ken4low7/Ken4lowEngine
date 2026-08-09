#define NOMINMAX
#include "HPWidget.h"

#include <DirectXCommon.h>
#include <TextureManager.h>
#include <GameTimer.h>

#include <algorithm>
#include <cmath>

namespace K4E = ::Ken4lowEngine;

void HPWidget::Initialize(const std::string& fullTex, const std::string& halfTex, const std::string& emptyTex)
{
	texFull_ = fullTex;
	texHalf_ = halfTex;
	texEmpty_ = emptyTex;

	// 先読み
	K4E::TextureManager::GetInstance()->LoadTexture(texFull_);
	K4E::TextureManager::GetInstance()->LoadTexture(texHalf_);
	K4E::TextureManager::GetInstance()->LoadTexture(texEmpty_);

	rebuildRequested_ = true;
}

void HPWidget::SetHP(float hp, float maxHp)
{
	const float oldHp = hp_;
	hp_ = std::max(0.0f, hp);

	// HPが減ったら揺れ（被弾）扱い
	if (hp_ + 0.001f < oldHp)
	{
		trauma_ = std::min(1.0f, trauma_ + damageTraumaKick_);
	}
	prevHp_ = hp_;
	const float newMax = std::max(1.0f, maxHp);
	// 最大HPが変わるとハート数が変わるので、そのときだけ作り直す
	if (std::abs(newMax - maxHp_) > 0.001f || slots_.empty())
	{
		maxHp_ = newMax;
		rebuildRequested_ = true;
	}
	else
	{
		maxHp_ = newMax;
	}
}

void HPWidget::NotifyHit(float strength01)
{
	strength01 = std::clamp(strength01, 0.0f, 1.0f);
	trauma_ = std::min(1.0f, trauma_ + hitTraumaKick_ * strength01);
}

void HPWidget::RebuildSlots(int heartCount)
{
	heartCount = std::clamp(heartCount, 1, maxHeartsCap_);

	slots_.clear();
	slots_.resize(static_cast<size_t>(heartCount));

	for (auto& s : slots_)
	{
		s.full = std::make_unique<K4E::Sprite>();
		s.full->Initialize(texFull_);
		s.full->SetAnchorPoint({ 0.0f, 0.0f });
		s.full->SetSize(iconSize_);

		s.half = std::make_unique<K4E::Sprite>();
		s.half->Initialize(texHalf_);
		s.half->SetAnchorPoint({ 0.0f, 0.0f });
		s.half->SetSize(iconSize_);

		s.empty = std::make_unique<K4E::Sprite>();
		s.empty->Initialize(texEmpty_);
		s.empty->SetAnchorPoint({ 0.0f, 0.0f });
		s.empty->SetSize(iconSize_);
	}

	UpdateSlotPositions();
}

void HPWidget::UpdateSlotPositions()
{
	// 左上から横に並べる
	for (size_t i = 0; i < slots_.size(); ++i)
	{
		const float x = (anchorPos_.x + shakeOffset_.x) + (iconSize_.x + padding_) * static_cast<float>(i);
		const float y = (anchorPos_.y + shakeOffset_.y);

		auto& s = slots_[i];
		s.full->SetPosition({ x, y });
		s.half->SetPosition({ x, y });
		s.empty->SetPosition({ x, y });
	}
}

void HPWidget::Update()
{
	if (!isVisible_) return;

	// dt取得（Crosshairと同じ）
	const float dt = K4E::GameTimer::GetInstance()->GetDeltaTime();
	shakeTime_ += dt;

	// 瀕死ほど荒く（HP割合が低いほど振幅/周波数アップ）
	const float hpRatio = std::clamp(hp_ / std::max(1.0f, maxHp_), 0.0f, 1.0f);
	const float low = 1.0f - hpRatio; // 0(元気) -> 1(瀕死)

	// 残りハート数
	const float per = std::max(1.0f, hpPerHeart_);
	const int heartsRemaining = (hp_ <= 0.0f) ? 0 : static_cast<int>(std::ceil(hp_ / per));

	// トラウマ減衰
	trauma_ = std::max(0.0f, trauma_ - traumaDecayPerSec_ * dt);

	// 残りハートが少ないと常時揺れ（最低トラウマを確保）
	if (criticalHeartsThreshold_ > 0 && heartsRemaining > 0 && heartsRemaining <= criticalHeartsThreshold_)
	{
		// 瀕死ほどさらに強め
		const float baseline = std::clamp(baselineTraumaAtCritical_ + 0.25f * low, 0.0f, 1.0f);
		trauma_ = std::max(trauma_, baseline);
	}

	// 周波数/振幅（瀕死ほど荒く）
	const float freq = 8.0f + 18.0f * low; // Hz相当
	const float ampMax = maxShakePixels_ * (0.45f + 0.55f * low);
	const float amp = ampMax * trauma_; // 直線の方が視認しやすい

	// 疑似ノイズ（sin合成）
	const float w = 6.2831853f; // 2pi
	const float t = shakeTime_;
	float nx = std::sin((t * freq) * w + 1.37f) + 0.5f * std::sin((t * (freq * 2.13f)) * w + 7.11f);
	float ny = std::sin((t * (freq * 1.23f)) * w + 3.19f) + 0.5f * std::sin((t * (freq * 2.41f)) * w + 11.7f);
	// -1..1っぽく
	nx = std::clamp(nx * 0.666f, -1.0f, 1.0f);
	ny = std::clamp(ny * 0.666f, -1.0f, 1.0f);
	shakeOffset_ = { nx * amp, ny * amp };

	// 必要ならスロット作り直し
	if (rebuildRequested_)
	{
		rebuildRequested_ = false;
		const int hearts = static_cast<int>(std::ceil(maxHp_ / per));
		RebuildSlots(hearts);
	}

	UpdateSlotPositions();

	// Sprite側がUpdate前提なので、全スロットUpdateしておく
	for (auto& s : slots_)
	{
		s.full->Update();
		s.half->Update();
		s.empty->Update();
	}
}

void HPWidget::Draw()
{
	if (!isVisible_) return;

	const float per = std::max(1.0f, hpPerHeart_);

	for (size_t i = 0; i < slots_.size(); ++i)
	{
		const float slotStart = per * static_cast<float>(i);
		const float remain = hp_ - slotStart;

		if (remain >= per)
		{
			slots_[i].full->Draw();
		}
		else if (remain > 0.0f)
		{
			// HPが少しでも残っていれば半分表示にする
			slots_[i].half->Draw();
		}
		else
		{
			slots_[i].empty->Draw();
		}
	}
}
