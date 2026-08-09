#include "ControlGuideUI.h"
#include <algorithm>
#include <cmath>

void ControlGuideUI::Initialize(
	const std::string& ammoIconPath,
	const std::string& leftClickPath,
	const std::string& reticleIconPath,
	const std::string& rightClickPath,
	const std::string& rKeyIconPath,
	const std::string& reloadIconPath
)
{
	shootGuide_.iconA = std::make_unique<K4E::Sprite>();
	shootGuide_.iconB = std::make_unique<K4E::Sprite>();
	adsGuide_.iconA = std::make_unique<K4E::Sprite>();
	adsGuide_.iconB = std::make_unique<K4E::Sprite>();
	reloadGuide_.iconA = std::make_unique<K4E::Sprite>();
	reloadGuide_.iconB = std::make_unique<K4E::Sprite>();

	shootGuide_.iconA->Initialize(ammoIconPath.c_str());
	shootGuide_.iconB->Initialize(leftClickPath.c_str());

	adsGuide_.iconA->Initialize(reticleIconPath.c_str());
	adsGuide_.iconB->Initialize(rightClickPath.c_str());

	reloadGuide_.iconA->Initialize(reloadIconPath.c_str());
	reloadGuide_.iconB->Initialize(rKeyIconPath.c_str());

	shootGuide_.basePos = { 0.0f, 0.0f };
	adsGuide_.basePos = { 110.0f, 0.0f };
	reloadGuide_.basePos = { 220.0f, 0.0f };

	shootGuide_.iconSizeA = { 44.0f, 44.0f };
	shootGuide_.iconSizeB = { 52.0f, 52.0f };

	adsGuide_.iconSizeA = { 44.0f, 44.0f };
	adsGuide_.iconSizeB = { 52.0f, 52.0f };

	reloadGuide_.iconSizeA = { 44.0f, 44.0f };
	reloadGuide_.iconSizeB = { 52.0f, 52.0f };

	shootGuide_.gap = 10.0f;
	adsGuide_.gap = 10.0f;
	reloadGuide_.gap = 10.0f;
}

void ControlGuideUI::Update(float deltaTime)
{
	if (!isVisible_) return;

	pulseTimer_ += deltaTime;

	UpdatePairLayout(shootGuide_);
	UpdatePairLayout(adsGuide_);
	UpdatePairLayout(reloadGuide_);
}

void ControlGuideUI::UpdatePairLayout(GuidePair& pair)
{
	float pulse = (std::sin(pulseTimer_ * 2.0f) + 1.0f) * 0.5f;
	float a = 0.80f + pulse * 0.20f;
	a *= alpha_;

	K4E::Vector2 posA = { anchorTopLeft_.x + pair.basePos.x, anchorTopLeft_.y + pair.basePos.y };
	K4E::Vector2 posB = { posA.x, posA.y + pair.iconSizeA.y + pair.gap };

	pair.iconA->SetAnchorPoint({ 0.0f, 0.0f });
	pair.iconB->SetAnchorPoint({ 0.0f, 0.0f });

	pair.iconA->SetPosition(posA);
	pair.iconB->SetPosition(posB);

	pair.iconA->SetSize(pair.iconSizeA);
	pair.iconB->SetSize(pair.iconSizeB);

	pair.iconA->SetColor({ 1.0f, 1.0f, 1.0f, a });
	pair.iconB->SetColor({ 1.0f, 1.0f, 1.0f, a });

	pair.iconA->Update();
	pair.iconB->Update();
}

void ControlGuideUI::Draw()
{
	if (!isVisible_) return;

	shootGuide_.iconA->Draw();
	shootGuide_.iconB->Draw();

	adsGuide_.iconA->Draw();
	adsGuide_.iconB->Draw();

	reloadGuide_.iconA->Draw();
	reloadGuide_.iconB->Draw();
}