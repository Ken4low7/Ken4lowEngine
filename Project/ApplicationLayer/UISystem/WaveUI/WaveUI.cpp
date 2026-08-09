#include "WaveUI.h"
#include "GameViewportConstants.h"
#include "DirectXCommon.h"

using namespace Ken4lowEngine;

void WaveUI::Initialize()
{
	const float screenW = static_cast<float>(GameViewportConstants::Width); // Wave UIは固定内部解像度1920x1080幅で中央揃えする。
	const float screenH = static_cast<float>(GameViewportConstants::Height); // Wave UIは固定内部解像度1920x1080高さで配置する。

	const float centerX = screenW * 0.5f;
	const float centerY = screenH * 0.35f;

	// -----------------------------
	// 数字描画
	// -----------------------------
	numberDrawer_ = std::make_unique<NumberSpriteDrawer>();
	numberDrawer_->Initialize("UI/Common/Number.dds", 50.0f, 50.0f, 28.0f, 28.0f);

	// -----------------------------
	// 上部中央：WAVEラベル
	// 元画像は大きいので中央帯だけ切り出して使う
	// -----------------------------
	waveLabelSprite_ = std::make_unique<Sprite>();
	waveLabelSprite_->Initialize("UI/Common/wave_label.dds");
	waveLabelSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	waveLabelSprite_->SetTextureLeftTop({ 240.0f, 300.0f });
	waveLabelSprite_->SetTextureSize({ 1050.0f, 320.0f });
	waveLabelSprite_->SetSize({ 220.0f, 64.0f });
	waveLabelSprite_->SetPosition({ centerX, 42.0f });

	// -----------------------------
	// 画面中央：WAVE START
	// -----------------------------
	waveStartBannerSprite_ = std::make_unique<Sprite>();
	waveStartBannerSprite_->Initialize("UI/Common/wave_start.dds");
	waveStartBannerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	waveStartBannerSprite_->SetTextureLeftTop({ 220.0f, 220.0f });
	waveStartBannerSprite_->SetTextureSize({ 1100.0f, 560.0f });
	waveStartBannerSprite_->SetSize({ 420.0f, 164.0f });
	waveStartBannerSprite_->SetPosition({ centerX, centerY });

	// -----------------------------
	// 画面中央：FINAL WAVE
	// -----------------------------
	finalWaveBannerSprite_ = std::make_unique<Sprite>();
	finalWaveBannerSprite_->Initialize("UI/Common/final_wave.dds");
	finalWaveBannerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	finalWaveBannerSprite_->SetTextureLeftTop({ 220.0f, 220.0f });
	finalWaveBannerSprite_->SetTextureSize({ 1100.0f, 560.0f });
	finalWaveBannerSprite_->SetSize({ 420.0f, 164.0f });
	finalWaveBannerSprite_->SetPosition({ centerX, centerY });

	// -----------------------------
	// 画面中央：WAVE CLEAR
	// -----------------------------
	clearBannerSprite_ = std::make_unique<Sprite>();
	clearBannerSprite_->Initialize("UI/Common/wave_clear.dds");
	clearBannerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	clearBannerSprite_->SetTextureLeftTop({ 220.0f, 220.0f });
	clearBannerSprite_->SetTextureSize({ 1100.0f, 560.0f });
	clearBannerSprite_->SetSize({ 420.0f, 164.0f });
	clearBannerSprite_->SetPosition({ centerX, centerY });

	visible_ = true;
}

void WaveUI::Update(float deltaTime)
{
	if (!visible_) return;

	if (waveLabelSprite_) waveLabelSprite_->Update();

	if (showWaveStartBanner_ && waveStartBannerSprite_) waveStartBannerSprite_->Update();
	if (showFinalWaveBanner_ && finalWaveBannerSprite_) finalWaveBannerSprite_->Update();
	if (showClearBanner_ && clearBannerSprite_) clearBannerSprite_->Update();

	UpdateWaveBanner(deltaTime);
}

void WaveUI::Draw()
{
	if (!visible_) return;

	const float screenW = static_cast<float>(GameViewportConstants::Width); // Wave UIは固定内部解像度1920x1080幅で中央揃えする。
	const float centerX = screenW * 0.5f;

	// 上部中央のWAVEラベル
	if (waveLabelSprite_) waveLabelSprite_->Draw();

	// ラベルの真下に現在Wave番号を中央表示
	if (numberDrawer_)
	{
		numberDrawer_->Reset();

		numberDrawer_->SetDrawDigitSize(26.0f, 26.0f);
		numberDrawer_->DrawNumberCentered(displayState_.currentWave, { centerX - 14.0f, 74.0f }, 2.0f);

		// 合計Wave数も出したいならこれを使う
		 numberDrawer_->SetDrawDigitSize(26.0f, 26.0f);
		 numberDrawer_->DrawNumberCentered(displayState_.totalWaves, { centerX + 14.0f, 74.0f }, 2.0f);
	}

	// 画面中央の演出バナー
	if (showWaveStartBanner_ && waveStartBannerSprite_) waveStartBannerSprite_->Draw();
	if (showFinalWaveBanner_ && finalWaveBannerSprite_) finalWaveBannerSprite_->Draw();
	if (showClearBanner_ && clearBannerSprite_) clearBannerSprite_->Draw();
}

void WaveUI::NotifyWaveStarted(int waveNumber, bool isFinalWave)
{
	displayState_.currentWave = waveNumber;
	showWaveStartBanner_ = !isFinalWave;
	showFinalWaveBanner_ = isFinalWave;
	showClearBanner_ = false;
	bannerTimer_ = bannerDuration_;
}

void WaveUI::NotifyAllWavesCleared()
{
	showWaveStartBanner_ = false;
	showFinalWaveBanner_ = false;
	showClearBanner_ = true;
	bannerTimer_ = bannerDuration_;
}

void WaveUI::UpdateWaveBanner(float deltaTime)
{
	if (bannerTimer_ > 0.0f)
	{
		bannerTimer_ -= deltaTime;
		if (bannerTimer_ <= 0.0f)
		{
			bannerTimer_ = 0.0f;
			showWaveStartBanner_ = false;
			showFinalWaveBanner_ = false;
			showClearBanner_ = false;
		}
	}
}