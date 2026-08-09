#define NOMINMAX
#include "BossHudUI.h"
#include "ParameterManager.h"
#include <Sprite.h>

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	constexpr const char* kBossHpBarGroup = "BossHpBar";

	float Length2D(float x, float y)
	{
		return std::sqrt(x * x + y * y);
	}
}

BossHudUI::~BossHudUI()
{
	K4E::ParameterManager::GetInstance()->UnregisterParameterApplier(kBossHpBarGroup, this);
}

void BossHudUI::Initialize()
{
	InitializeHpBarSprites();
	InitializeGuideSprites();
	RegisterHpBarParameters();
	ApplyHpBarParameters();
}

void BossHudUI::Update(float deltaTime)
{
	ApplyHpBarParameters();
	UpdateHpBarSprites();
	UpdateGuideSprites(deltaTime);
}

void BossHudUI::Draw()
{
	DrawHpBar();
	DrawGuide();
}

void BossHudUI::SetBossHP(float hp, float maxHp, bool bossBattleActive)
{
	bossBattleActive_ = bossBattleActive;
	bossHp_ = std::max(0.0f, hp);
	bossMaxHp_ = std::max(0.0f, maxHp);

	// 最大HPが0以下のフレームでもHUD計算が壊れないよう、HP率は0として扱う。
	hpRate_ = (bossMaxHp_ > 0.0f) ? std::clamp(bossHp_ / bossMaxHp_, 0.0f, 1.0f) : 0.0f;
	hpBarRuntimeVisible_ = hpBarSettings_.visible && bossBattleActive_ && bossHp_ > 0.0f;
}

void BossHudUI::SetBossGuide(const K4E::Vector3& playerPos,
	const K4E::Vector3& bossPos,
	const K4E::Vector3& cameraForward,
	bool bossBattleActive)
{
	guideBossPosition_ = bossPos;
	guideActive_ = guideSettings_.visible && bossBattleActive && guideTimer_ > 0.0f;
	if (!guideActive_)
	{
		return;
	}

	K4E::Vector3 toBoss = bossPos - playerPos;
	toBoss.y = 0.0f;
	toBoss = K4E::Vector3::NormalizeXZSafe(toBoss, { 0.0f, 0.0f, 1.0f });

	K4E::Vector3 forward = cameraForward;
	forward.y = 0.0f;
	forward = K4E::Vector3::NormalizeXZSafe(forward, { 0.0f, 0.0f, 1.0f });
	const K4E::Vector3 right = K4E::Vector3::PerpRightXZ(forward);

	const float screenX = K4E::Vector3::Dot(toBoss, right);
	const float screenY = -K4E::Vector3::Dot(toBoss, forward);
	const float screenLen = std::max(Length2D(screenX, screenY), 0.0001f);
	const float dirX = screenX / screenLen;
	const float dirY = screenY / screenLen;

	const K4E::Vector2 center = guideSettings_.center;
	guideDotPosition_ = {
		center.x + dirX * guideSettings_.radius,
		center.y + dirY * guideSettings_.radius
	};
	guideLineCenter_ = {
		(center.x + guideDotPosition_.x) * 0.5f,
		(center.y + guideDotPosition_.y) * 0.5f
	};
	guideLineLength_ = guideSettings_.radius;
	guideAngle_ = std::atan2(dirY, dirX);
}

void BossHudUI::NotifyBossIntroCompleted(const K4E::Vector3& bossPos)
{
	// ボス登場演出直後だけ方向ガイドを出し、プレイヤーが復帰後にボス位置を追いやすくする。
	guideBossPosition_ = bossPos;
	guideTimer_ = std::max(0.0f, guideSettings_.holdTime);
	guideActive_ = guideSettings_.visible && guideTimer_ > 0.0f;
}

void BossHudUI::RegisterHpBarParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	parameters->CreateGroup(kBossHpBarGroup);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarVisible", hpBarSettings_.visible);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarPosition", hpBarSettings_.position, K4E::Vector3{ 0.0f, 0.0f, 0.0f }, K4E::Vector3{ 1920.0f, 1080.0f, 0.0f });
	parameters->AddItem(kBossHpBarGroup, "bossHpBarWidth", hpBarSettings_.width, 100.0f, 1600.0f);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarHeight", hpBarSettings_.height, 4.0f, 80.0f);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarNameOffset", hpBarSettings_.nameOffset, K4E::Vector3{ -500.0f, -200.0f, 0.0f }, K4E::Vector3{ 500.0f, 200.0f, 0.0f });
	parameters->AddStringItem(kBossHpBarGroup, "bossHpBarDisplayName", hpBarSettings_.displayName, {});
	parameters->AddItem(kBossHpBarGroup, "bossHpBarShowAfterIntro", hpBarSettings_.showAfterIntro);
	parameters->AddItem(kBossHpBarGroup, "bossHpBarHideWaveUI", hpBarSettings_.hideWaveUI);
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarVisible", "ボスHPバー表示");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarPosition", "表示位置");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarWidth", "バー幅");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarHeight", "バー高さ");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarNameOffset", "名前表示オフセット");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarDisplayName", "ボス表示名");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarShowAfterIntro", "登場後に表示");
	parameters->SetDisplayName(kBossHpBarGroup, "bossHpBarHideWaveUI", "ボス戦中Wave UI非表示");
	parameters->RegisterParameterApplier(kBossHpBarGroup, this, [this]() { ApplyHpBarParameters(); });
	parameters->LoadFile(kBossHpBarGroup);
}

void BossHudUI::ApplyHpBarParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	hpBarSettings_.visible = parameters->GetValue<bool>(kBossHpBarGroup, "bossHpBarVisible");
	hpBarSettings_.position = parameters->GetValue<K4E::Vector3>(kBossHpBarGroup, "bossHpBarPosition");
	hpBarSettings_.width = std::max(1.0f, parameters->GetValue<float>(kBossHpBarGroup, "bossHpBarWidth"));
	hpBarSettings_.height = std::max(1.0f, parameters->GetValue<float>(kBossHpBarGroup, "bossHpBarHeight"));
	hpBarSettings_.nameOffset = parameters->GetValue<K4E::Vector3>(kBossHpBarGroup, "bossHpBarNameOffset");
	hpBarSettings_.displayName = parameters->GetValue<std::string>(kBossHpBarGroup, "bossHpBarDisplayName");
	hpBarSettings_.showAfterIntro = parameters->GetValue<bool>(kBossHpBarGroup, "bossHpBarShowAfterIntro");
	hpBarSettings_.hideWaveUI = parameters->GetValue<bool>(kBossHpBarGroup, "bossHpBarHideWaveUI");
	hpBarRuntimeVisible_ = hpBarSettings_.visible && bossBattleActive_ && bossHp_ > 0.0f;
}

void BossHudUI::InitializeHpBarSprites()
{
	hpFrameSprite_ = std::make_unique<K4E::Sprite>();
	hpFrameSprite_->Initialize("Effects/white.dds");
	hpFrameSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	hpBackSprite_ = std::make_unique<K4E::Sprite>();
	hpBackSprite_->Initialize("Effects/white.dds");
	hpBackSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	hpDelaySprite_ = std::make_unique<K4E::Sprite>();
	hpDelaySprite_->Initialize("Effects/white.dds");
	hpDelaySprite_->SetAnchorPoint({ 0.0f, 0.5f });

	hpFillSprite_ = std::make_unique<K4E::Sprite>();
	hpFillSprite_->Initialize("Effects/white.dds");
	hpFillSprite_->SetAnchorPoint({ 0.0f, 0.5f });
}

void BossHudUI::InitializeGuideSprites()
{
	guideLineSprite_ = std::make_unique<K4E::Sprite>();
	guideLineSprite_->Initialize("Effects/white.dds");
	guideLineSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	guideDotBackSprite_ = std::make_unique<K4E::Sprite>();
	guideDotBackSprite_->Initialize("Effects/white.dds");
	guideDotBackSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	guideDotSprite_ = std::make_unique<K4E::Sprite>();
	guideDotSprite_->Initialize("Effects/white.dds");
	guideDotSprite_->SetAnchorPoint({ 0.5f, 0.5f });
}

void BossHudUI::UpdateHpBarSprites()
{
	const float approachSpeed = 0.9f;
	delayedHpRate_ += (hpRate_ - delayedHpRate_) * approachSpeed * 0.016f;
	if (std::fabs(delayedHpRate_ - hpRate_) < 0.002f)
	{
		delayedHpRate_ = hpRate_;
	}

	const K4E::Vector2 center{ hpBarSettings_.position.x, hpBarSettings_.position.y };
	const float width = hpBarSettings_.width;
	const float height = hpBarSettings_.height;
	const K4E::Vector2 left{ center.x - width * 0.5f, center.y };

	if (hpFrameSprite_)
	{
		hpFrameSprite_->SetPosition(center);
		hpFrameSprite_->SetSize({ width + 6.0f, height + 6.0f });
		hpFrameSprite_->SetColor({ 0.02f, 0.01f, 0.015f, 0.95f });
		hpFrameSprite_->Update();
	}
	if (hpBackSprite_)
	{
		hpBackSprite_->SetPosition(center);
		hpBackSprite_->SetSize({ width, height });
		hpBackSprite_->SetColor({ 0.12f, 0.05f, 0.08f, 0.86f });
		hpBackSprite_->Update();
	}
	if (hpDelaySprite_)
	{
		hpDelaySprite_->SetPosition(left);
		hpDelaySprite_->SetSize({ width * std::clamp(delayedHpRate_, 0.0f, 1.0f), height });
		hpDelaySprite_->SetColor({ 1.0f, 0.55f, 0.18f, 0.75f });
		hpDelaySprite_->Update();
	}
	if (hpFillSprite_)
	{
		hpFillSprite_->SetPosition(left);
		hpFillSprite_->SetSize({ width * hpRate_, height });
		hpFillSprite_->SetColor({ 0.82f, 0.06f, 0.18f, 0.96f });
		hpFillSprite_->Update();
	}
}

void BossHudUI::UpdateGuideSprites(float deltaTime)
{
	if (guideTimer_ > 0.0f)
	{
		guideTimer_ = std::max(0.0f, guideTimer_ - deltaTime);
	}

	if (!guideActive_)
	{
		return;
	}

	const float fade = std::clamp(guideTimer_ / std::max(0.01f, guideSettings_.holdTime), 0.0f, 1.0f);
	const float alpha = std::clamp(fade * 1.4f, 0.0f, 0.88f);

	if (guideLineSprite_)
	{
		guideLineSprite_->SetPosition(guideLineCenter_);
		guideLineSprite_->SetSize({ guideLineLength_, guideSettings_.lineThickness });
		guideLineSprite_->SetRotation(guideAngle_);
		guideLineSprite_->SetColor({ 1.0f, 0.72f, 0.18f, alpha });
		guideLineSprite_->Update();
	}
	if (guideDotBackSprite_)
	{
		guideDotBackSprite_->SetPosition(guideDotPosition_);
		guideDotBackSprite_->SetSize({ guideSettings_.dotSize + 12.0f, guideSettings_.dotSize + 12.0f });
		guideDotBackSprite_->SetRotation(0.0f);
		guideDotBackSprite_->SetColor({ 0.05f, 0.02f, 0.01f, alpha * 0.65f });
		guideDotBackSprite_->Update();
	}
	if (guideDotSprite_)
	{
		guideDotSprite_->SetPosition(guideDotPosition_);
		guideDotSprite_->SetSize({ guideSettings_.dotSize, guideSettings_.dotSize });
		guideDotSprite_->SetRotation(0.0f);
		guideDotSprite_->SetColor({ 1.0f, 0.18f, 0.08f, alpha });
		guideDotSprite_->Update();
	}
}

void BossHudUI::DrawHpBar()
{
	// ボスHPバーを表示する条件判定。登場演出完了後のボス戦中だけ画面固定UIとして描く。
	if (!hpBarRuntimeVisible_ || !hpBarSettings_.showAfterIntro)
	{
		return;
	}

	if (hpFrameSprite_) hpFrameSprite_->Draw();
	if (hpBackSprite_) hpBackSprite_->Draw();
	if (hpDelaySprite_) hpDelaySprite_->Draw();
	if (hpFillSprite_) hpFillSprite_->Draw();
}

void BossHudUI::DrawGuide()
{
	if (!guideActive_ || guideTimer_ <= 0.0f)
	{
		return;
	}

	if (guideLineSprite_) guideLineSprite_->Draw();
	if (guideDotBackSprite_) guideDotBackSprite_->Draw();
	if (guideDotSprite_) guideDotSprite_->Draw();
}
