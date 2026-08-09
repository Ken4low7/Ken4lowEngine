#include "NoAmmoUI.h"
#include <DirectXCommon.h>
#include "GameViewportConstants.h"
#include <FontAtlasLoader.h>
#include <cmath>

void NoAmmoUI::Initialize()
{
	textDrawer_ = std::make_unique<K4E::TextSpriteDrawer>();
	isReady_ = false;

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
		isReady_ = true;
	}
	catch (...)
	{
		isReady_ = false;
	}

	// NoAmmo UIは固定内部解像度1920x1080の中央付近へ配置する。
	const float w = static_cast<float>(K4E::GameViewportConstants::Width);
	const float h = static_cast<float>(K4E::GameViewportConstants::Height);

	position_ = { w * 0.5f, h * 0.55f };
	scale_ = 0.90f;
	alpha_ = 0.0f;
	blinkTimer_ = 0.0f;
}

void NoAmmoUI::Update(float deltaTime)
{
	if (!isReady_) return;

	blinkTimer_ += deltaTime;

	if (visible_)
	{
		alpha_ = 0.55f + 0.45f * std::sinf(blinkTimer_ * 8.0f);
		if (alpha_ < 0.0f) alpha_ = 0.0f;
		if (alpha_ > 1.0f) alpha_ = 1.0f;
	}
	else
	{
		alpha_ = 0.0f;
	}
}

void NoAmmoUI::Draw()
{
	if (!visible_) return;
	if (!isReady_ || !textDrawer_) return;

	textDrawer_->Reset();
	textDrawer_->SetScale(scale_);
	textDrawer_->SetLetterSpacing(2.0f);
	textDrawer_->SetColor({ 1.0f, 0.2f, 0.2f, alpha_ });
	textDrawer_->DrawTextCentered("弾切れ", position_);
}

void NoAmmoUI::Finalize()
{
	if (textDrawer_)
	{
		textDrawer_->Finalize();
		textDrawer_.reset();
	}
	isReady_ = false;
}