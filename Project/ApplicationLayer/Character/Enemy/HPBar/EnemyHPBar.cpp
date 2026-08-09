#define NOMINMAX
#include "EnemyHPBar.h"

void EnemyHPBar::Initialize()
{
	// 枠スプライト
	frameSprite_ = std::make_unique<K4E::Sprite>();
	frameSprite_->Initialize("Effects/white.dds");
	frameSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	// 背景スプライト
	backSprite_ = std::make_unique<K4E::Sprite>();
	backSprite_->Initialize("Effects/white.dds");
	backSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	// 遅れて減るダメージバースプライト
	damageDelaySprite_ = std::make_unique<K4E::Sprite>();
	damageDelaySprite_->Initialize("Effects/white.dds");
	damageDelaySprite_->SetAnchorPoint({ 0.0f, 0.5f });

	// 現在HP本体スプライト
	fillSprite_ = std::make_unique<K4E::Sprite>();
	fillSprite_->Initialize("Effects/white.dds");
	fillSprite_->SetAnchorPoint({ 0.0f, 0.5f });

	// 減少分フラッシュ演出スプライト
	damageFlashSprite_ = std::make_unique<K4E::Sprite>();
	damageFlashSprite_->Initialize("Effects/white.dds");
	damageFlashSprite_->SetAnchorPoint({ 0.0f, 0.5f });

	// 一度更新しておく
	frameSprite_->Update();
	backSprite_->Update();
	damageDelaySprite_->Update();
	fillSprite_->Update();
	damageFlashSprite_->Update();
}

void EnemyHPBar::SetVisible(bool visible)
{
	visible_ = visible;

	// 非表示になったら演出状態も止めておく
	if (!visible_)
	{
		delayWaitTimer_ = 0.0f;
		flashTimer_ = 0.0f;
	}
}

float EnemyHPBar::Clamp01(float value) const
{
	return std::clamp(value, 0.0f, 1.0f);
}

K4E::Vector4 EnemyHPBar::GetHpColor(float hpRate) const
{
	hpRate = Clamp01(hpRate);

	// 通常は緑
	K4E::Vector4 color = { 0.1f, 1.0f, 0.1f, 0.92f };

	// 半分未満で黄色
	if (hpRate < 0.5f)
	{
		color = { 1.0f, 0.85f, 0.15f, 0.95f };
	}

	// 1/4未満で赤
	if (hpRate < 0.25f)
	{
		color = { 1.0f, 0.2f, 0.2f, 0.96f };
	}

	return color;
}

void EnemyHPBar::Update(
	const K4E::Vector2& screenPos,
	float hpRate,
	bool visible,
	float deltaTime,
	float width,
	float height)
{
	// 表示フラグ更新
	visible_ = visible;

	// 非表示ならここで終了
	if (!visible_)
	{
		// 非表示中に状態だけ暴れないように最低限止める
		delayWaitTimer_ = 0.0f;
		flashTimer_ = 0.0f;
		return;
	}

	// HP割合を 0～1 に丸める
	hpRate = Clamp01(hpRate);

	// 初回更新時の不自然な演出防止
	// prev が未設定状態だと見なして大きくずれていた場合、
	// 初回は同値で揃えておく
	if (prevHpRate_ < 0.0f || prevHpRate_ > 1.0f)
	{
		prevHpRate_ = hpRate;
		currentHpRate_ = hpRate;
		delayedHpRate_ = hpRate;
	}

	// -----------------------------------------
	// ダメージを受けた瞬間を検出
	// -----------------------------------------
	if (hpRate < prevHpRate_)
	{
		// 現在HPはすぐ反映
		currentHpRate_ = hpRate;

		// 遅延バーは一旦そのまま残して、少し待ってから縮める
		delayWaitTimer_ = 0.10f;

		// フラッシュ演出開始
		flashTimer_ = flashDuration_;
		flashStartRate_ = prevHpRate_;
		flashEndRate_ = hpRate;
	}
	else
	{
		// 減っていない時は普通に現在値を更新
		currentHpRate_ = hpRate;

		// 回復や再設定時は遅延バーもすぐ追従
		if (hpRate > delayedHpRate_)
		{
			delayedHpRate_ = hpRate;
		}
	}

	// -----------------------------------------
	// 遅延バーの更新
	// -----------------------------------------
	if (delayWaitTimer_ > 0.0f)
	{
		delayWaitTimer_ -= deltaTime;
		if (delayWaitTimer_ < 0.0f)
		{
			delayWaitTimer_ = 0.0f;
		}
	}
	else
	{
		// 少しずつ現在HPまで縮める
		const float shrinkSpeed = 1.6f;
		delayedHpRate_ -= shrinkSpeed * deltaTime;
		if (delayedHpRate_ < currentHpRate_)
		{
			delayedHpRate_ = currentHpRate_;
		}
	}

	// 念のため丸める
	currentHpRate_ = Clamp01(currentHpRate_);
	delayedHpRate_ = Clamp01(delayedHpRate_);

	// -----------------------------------------
	// 共通位置計算
	// -----------------------------------------
	// 左基準バー用の左端位置
	K4E::Vector2 leftPos;
	leftPos.x = screenPos.x - width * 0.5f;
	leftPos.y = screenPos.y;

	// -----------------------------------------
	// 枠
	// -----------------------------------------
	frameSprite_->SetPosition(screenPos);
	frameSprite_->SetSize({ width + 2.0f, height + 2.0f });
	frameSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	frameSprite_->Update();

	// -----------------------------------------
	// 背景
	// -----------------------------------------
	backSprite_->SetPosition(screenPos);
	backSprite_->SetSize({ width, height });
	backSprite_->SetColor({ 0.12f, 0.12f, 0.12f, 0.75f });
	backSprite_->Update();

	// -----------------------------------------
	// 遅れて減るダメージバー
	// -----------------------------------------
	{
		const float delayWidth = width * delayedHpRate_;

		damageDelaySprite_->SetPosition(leftPos);
		damageDelaySprite_->SetSize({ delayWidth, height });
		damageDelaySprite_->SetColor({ 1.0f, 0.35f, 0.15f, 0.72f });
		damageDelaySprite_->Update();
	}

	// -----------------------------------------
	// 現在HPバー
	// -----------------------------------------
	{
		const float fillWidth = width * currentHpRate_;
		const K4E::Vector4 fillColor = GetHpColor(currentHpRate_);

		fillSprite_->SetPosition(leftPos);
		fillSprite_->SetSize({ fillWidth, height });
		fillSprite_->SetColor(fillColor);
		fillSprite_->Update();
	}

	// -----------------------------------------
	// 減少分フラッシュ演出
	// -----------------------------------------
	if (flashTimer_ > 0.0f)
	{
		flashTimer_ -= deltaTime;
		if (flashTimer_ < 0.0f)
		{
			flashTimer_ = 0.0f;
		}

		// 進行率 0～1
		const float t = 1.0f - (flashTimer_ / flashDuration_);

		// ダメージを受けた区間だけ表示
		const float lostRate = std::max(0.0f, flashStartRate_ - flashEndRate_);

		if (lostRate > 0.0001f)
		{
			// 減った分の左端は「新HPの終端位置」
			K4E::Vector2 flashPos = leftPos;
			flashPos.x += width * flashEndRate_;

			// 少し上に浮かせる
			flashPos.y -= 4.0f * t;

			// 少し横に拡大、高さも少し太くする
			const float baseFlashWidth = width * lostRate;
			const float flashWidth = baseFlashWidth * (1.0f + 0.12f * t);
			const float flashHeight = height * (1.0f + 0.35f * t);

			// フェードアウト
			const float alpha = 1.0f - t;

			damageFlashSprite_->SetPosition(flashPos);
			damageFlashSprite_->SetSize({ flashWidth, flashHeight });
			damageFlashSprite_->SetColor({ 1.0f, 0.95f, 0.65f, alpha * 0.95f });
			damageFlashSprite_->Update();
		}
	}

	// 今回値を次回比較用に保存
	prevHpRate_ = hpRate;
}

void EnemyHPBar::Draw()
{
	// 非表示なら描かない
	if (!visible_)
	{
		return;
	}

	// 描画順
	// 枠 → 背景 → 遅延バー → 現在HP → フラッシュ
	if (frameSprite_)
	{
		frameSprite_->Draw();
	}

	if (backSprite_)
	{
		backSprite_->Draw();
	}

	if (damageDelaySprite_)
	{
		damageDelaySprite_->Draw();
	}

	if (fillSprite_)
	{
		fillSprite_->Draw();
	}

	// フラッシュ中だけ前面に描画
	if (damageFlashSprite_ && flashTimer_ > 0.0f)
	{
		damageFlashSprite_->Draw();
	}
}