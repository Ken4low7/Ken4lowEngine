#define NOMINMAX
#include "Crosshair.h"
#include <DirectXCommon.h>
#include <TextureManager.h>
#include <GameTimer.h>
#include "GameViewportConstants.h"

#include <algorithm>
#include <cctype>

namespace K4E = ::Ken4lowEngine;

namespace
{
	constexpr const char* kDefaultReticleTex = "UI/Reticles/crosshair_circle_dot.dds";
	constexpr const char* kDefaultHitTex = "UI/Reticles/hitmarker_x_glow.dds";

	inline float Clamp01(float v)
	{
		return std::clamp(v, 0.0f, 1.0f);
	}
}

/// -------------------------------------------------------------
///                    初期化処理
/// -------------------------------------------------------------
void Crosshair::Initialize(const std::string& texturePath)
{
	textureName_ = NormalizeReticlePath_(texturePath);
	if (textureName_.empty()) textureName_ = kDefaultReticleTex;

	// 初期サイズ（武器設定が来る前）
	size_ = { 64.0f, 64.0f };
	RebuildReticleSprites_();

	// デフォルトのヒットマーカー（武器設定で上書きされる）
	hitMarkerTextureName_ = NormalizeReticlePath_(kDefaultHitTex);
	RebuildHitMarkerSprites_(hitMarkerTextureName_);
}

/// -------------------------------------------------------------
///                    更新処理
/// -------------------------------------------------------------
void Crosshair::Update()
{
	// CrosshairはMain Viewportの拡縮ではなく固定内部解像度1920x1080中心を基準にする。
	const float clientWidth = static_cast<float>(K4E::GameViewportConstants::Width);
	const float clientHeight = static_cast<float>(K4E::GameViewportConstants::Height);
	const float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	// ADSブレンド更新
	{
		const float target = isADS_ ? 1.0f : 0.0f;
		const float speed = (adsBlendTime_ > 0.001f) ? (1.0f / adsBlendTime_) : 1000.0f;
		if (adsBlendAlpha_ < target)
		{
			adsBlendAlpha_ = std::min(target, adsBlendAlpha_ + speed * deltaTime);
		}
		else if (adsBlendAlpha_ > target)
		{
			adsBlendAlpha_ = std::max(target, adsBlendAlpha_ - speed * deltaTime);
		}
	}

	// 着地インパルス減衰
	if (landExpandImpulseCurrent_ > 0.0f)
	{
		landExpandImpulseCurrent_ -= recoverSpeed_ * deltaTime;
		if (landExpandImpulseCurrent_ < 0.0f) landExpandImpulseCurrent_ = 0.0f;
	}

	// =========================================================
	// レティクルサイズ計算（spread + 移動拡散を反映）
	// =========================================================
	const float spreadT = std::clamp(spreadValueDeg_ * spreadToUiScale_, 0.0f, 1.0f);
	float nowSize = baseSizePx_ + (maxSizePx_ - baseSizePx_) * spreadT;

	if (enableMoveExpand_)
	{
		float moveMul = 1.0f;
		if (isAirborne_)
		{
			moveMul = airExpandMultiplier_;
		}
		else if (isSprinting_)
		{
			moveMul = sprintExpandMultiplier_;
		}
		else if (isMoving_)
		{
			moveMul = moveExpandMultiplier_;
		}

		nowSize *= std::max(0.1f, moveMul);
		nowSize += landExpandImpulseCurrent_;
	}

	if (nowSize < 1.0f) nowSize = 1.0f;

	// 今のCrosshair画像が64px前提なので、UI値(12~28)を見やすく拡大
	const float visualScale = 4.0f;
	size_ = { nowSize * visualScale, nowSize * visualScale };

	// ADS用中央ドットは小さめに表示
	const float dotSize = std::clamp(baseSizePx_ * 2.0f, 6.0f, 32.0f);
	const K4E::Vector2 adsDotSize = { dotSize, dotSize };

	// 各レイヤーの表示アルファを計算
	float hipAlpha = 1.0f;
	float adsRetAlpha = 0.0f;
	float adsDotAlpha = 0.0f;

	if (isADS_)
	{
		if (hideInADS_)
		{
			hipAlpha = 0.0f;
			adsRetAlpha = 0.0f;
			adsDotAlpha = 0.0f;
		}
		else
		{
			const bool hasAdsOverride = useAdsReticleOverride_ && adsSprite_;
			const bool hasAdsDot = useAdsCenterDot_ && adsDotSprite_;

			if (hasAdsOverride)
			{
				hipAlpha = 1.0f - adsBlendAlpha_;
				adsRetAlpha = adsBlendAlpha_;
			}
			else if (hasAdsDot)
			{
				// 「十字 → 点」挙動をデフォルトにする
				hipAlpha = 1.0f - adsBlendAlpha_;
			}

			if (hasAdsDot)
			{
				adsDotAlpha = adsBlendAlpha_;
			}
		}
	}

	// HIPレティクル
	if (sprite_)
	{
		sprite_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
		sprite_->SetSize(size_);

		const K4E::Vector4 color = isTargetingEnemy_ ? targetColor_ : normalColor_;

		sprite_->SetColor({ color.x, color.y, color.z, Clamp01(hipAlpha) * color.w });
		sprite_->Update();
	}
	if (shadow_)
	{
		shadow_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
		shadow_->SetSize({ size_.x + 4.0f, size_.y + 4.0f });
		shadow_->SetColor({ 0,0,0, 0.6f * Clamp01(hipAlpha) });
		shadow_->Update();
	}

	// ADSレティクル（別画像）
	if (adsSprite_)
	{
		adsSprite_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
		adsSprite_->SetSize(size_);

		const K4E::Vector4 color = isTargetingEnemy_ ? targetColor_ : normalColor_;

		adsSprite_->SetColor({ color.x, color.y, color.z, Clamp01(adsRetAlpha) * color.w });
		adsSprite_->Update();
	}

	if (adsShadow_)
	{
		adsShadow_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
		adsShadow_->SetSize({ size_.x + 4.0f, size_.y + 4.0f });
		adsShadow_->SetColor({ 0,0,0, 0.6f * Clamp01(adsRetAlpha) });
		adsShadow_->Update();
	}

	// ADS中央ドット
	if (adsDotSprite_)
	{
		adsDotSprite_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
		adsDotSprite_->SetSize(adsDotSize);

		const K4E::Vector4 color = isTargetingEnemy_ ? targetColor_ : normalColor_;

		adsDotSprite_->SetColor({ color.x, color.y, color.z, Clamp01(adsDotAlpha) * color.w });
		adsDotSprite_->Update();
	}

	if (adsDotShadow_)
	{
		adsDotShadow_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
		adsDotShadow_->SetSize({ adsDotSize.x + 2.0f, adsDotSize.y + 2.0f });
		adsDotShadow_->SetColor({ 0,0,0, 0.55f * Clamp01(adsDotAlpha) });
		adsDotShadow_->Update();
	}

	// ヒットマーカー更新
	if (showHitMarker_)
	{
		hitMarkerTimer_ -= deltaTime;

		hitMarkerScale_ += hitMarkerScaleVelocity_ * deltaTime;
		if (hitMarkerScale_ < 1.0f) hitMarkerScale_ = 1.0f;

		hitAlpha_ = std::clamp(hitMarkerTimer_ / std::max(0.01f, activeHitMarkerDuration_), 0.0f, 1.0f);

		if (hitMarkerTimer_ <= 0.0f)
		{
			showHitMarker_ = false;
			hitMarkerScale_ = 1.0f;
			hitAlpha_ = 0.0f;
		}
	}

	if (showHitMarker_ && hitMarkerSprite_ && hitMarkerShadow_)
	{
		const K4E::Vector2 sz = { hitBaseSize_ * hitMarkerScale_, hitBaseSize_ * hitMarkerScale_ };
		hitMarkerShadow_->SetSize({ sz.x + 4.0f, sz.y + 4.0f });
		hitMarkerSprite_->SetSize(sz);
		hitMarkerShadow_->SetColor({ 0,0,0,0.6f * hitAlpha_ });
		hitMarkerSprite_->SetColor({ 1,1,1, hitAlpha_ });
	}

	if (hitMarkerShadow_)
	{
		hitMarkerShadow_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
		hitMarkerShadow_->Update();
	}
	if (hitMarkerSprite_)
	{
		hitMarkerSprite_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
		hitMarkerSprite_->Update();
	}
}

/// -------------------------------------------------------------
///                    描画処理
/// -------------------------------------------------------------
void Crosshair::Draw()
{
	if (!isVisible_) return;

	// ADS非表示設定（スコープ系） + リロード中非表示
	const bool hideByADS = (hideInADS_ && isADS_);
	const bool hideByReload = (hideWhileReload_ && isReloading_);
	const bool hideAllReticle = (hideByADS || hideByReload);

	if (!hideAllReticle)
	{
		if (shadow_) shadow_->Draw();
		if (sprite_) sprite_->Draw();

		if (adsShadow_) adsShadow_->Draw();
		if (adsSprite_) adsSprite_->Draw();

		if (adsDotShadow_) adsDotShadow_->Draw();
		if (adsDotSprite_) adsDotSprite_->Draw();
	}

	// ヒットマーカーは別扱い（ADSでも出してOK）
	if (showHitMarker_ && hitMarkerShadow_ && hitMarkerSprite_)
	{
		hitMarkerShadow_->Draw();
		hitMarkerSprite_->Draw();
	}
}

/// -------------------------------------------------------------
///                    ヒットマーカー表示開始（通常）
/// -------------------------------------------------------------
void Crosshair::ShowHitMarker()
{
	TriggerHitMarker_(EHitMarkerKind::Normal);
}

void Crosshair::ShowHeadshotMarker()
{
	TriggerHitMarker_(EHitMarkerKind::Headshot);
}

void Crosshair::ShowKillConfirmMarker()
{
	TriggerHitMarker_(EHitMarkerKind::KillConfirm);
}

void Crosshair::NotifyEnemyHit(bool isHeadshot, bool isKill)
{
	if (isKill)
	{
		TriggerHitMarker_(EHitMarkerKind::KillConfirm);
	}
	else if (isHeadshot)
	{
		TriggerHitMarker_(EHitMarkerKind::Headshot);
	}
	else
	{
		TriggerHitMarker_(EHitMarkerKind::Normal);
	}
}

void Crosshair::SetReticleTexture(const std::string& texturePath)
{
	std::string normalized = NormalizeReticlePath_(texturePath);
	if (normalized.empty()) normalized = kDefaultReticleTex;

	if (normalized == textureName_) return;
	textureName_ = normalized;
	RebuildReticleSprites_();
}

void Crosshair::SetADSReticleTexture(const std::string& texturePath)
{
	const std::string normalized = NormalizeReticlePath_(texturePath);
	if (normalized == adsTextureName_) return;

	adsTextureName_ = normalized;
	RebuildAdsReticleSprites_();
}

void Crosshair::SetADSCenterDotTexture(const std::string& texturePath)
{
	const std::string normalized = NormalizeReticlePath_(texturePath);
	if (normalized == adsCenterDotTextureName_) return;

	adsCenterDotTextureName_ = normalized;
	RebuildAdsCenterDotSprites_();
}

void Crosshair::SetHitMarkerTexture(const std::string& texturePath)
{
	const std::string normalized = NormalizeReticlePath_(texturePath);
	if (normalized == hitMarkerTextureName_) return;

	hitMarkerTextureName_ = normalized;
	if (currentHitMarkerTextureName_.empty() || currentHitMarkerTextureName_ == kDefaultHitTex)
	{
		RebuildHitMarkerSprites_(hitMarkerTextureName_.empty() ? NormalizeReticlePath_(kDefaultHitTex) : hitMarkerTextureName_);
	}
}

void Crosshair::SetHeadshotHitMarkerTexture(const std::string& texturePath)
{
	headshotHitMarkerTextureName_ = NormalizeReticlePath_(texturePath);
}

void Crosshair::SetKillConfirmMarkerTexture(const std::string& texturePath)
{
	killConfirmMarkerTextureName_ = NormalizeReticlePath_(texturePath);
}

void Crosshair::SetMoveExpandMultipliers(float walkMul, float sprintMul, float airMul, float landImpulse)
{
	moveExpandMultiplier_ = std::max(0.0f, walkMul);
	sprintExpandMultiplier_ = std::max(0.0f, sprintMul);
	airExpandMultiplier_ = std::max(0.0f, airMul);
	landExpandImpulseCfg_ = std::max(0.0f, landImpulse);
}

void Crosshair::SetMovementState(bool isMoving, bool isSprinting, bool isAirborne)
{
	isMoving_ = isMoving;
	isSprinting_ = isSprinting;
	isAirborne_ = isAirborne;
}

void Crosshair::NotifyLanded()
{
	if (!enableMoveExpand_) return;
	landExpandImpulseCurrent_ = std::max(landExpandImpulseCurrent_, landExpandImpulseCfg_);
}

void Crosshair::RebuildReticleSprites_()
{
	// CrosshairはMain Viewportの拡縮ではなく固定内部解像度1920x1080中心を基準にする。
	const float clientWidth = static_cast<float>(K4E::GameViewportConstants::Width);
	const float clientHeight = static_cast<float>(K4E::GameViewportConstants::Height);

	const std::string tex = NormalizeReticlePath_(textureName_);
	if (tex.empty())
	{
		sprite_.reset();
		shadow_.reset();
		return;
	}

	K4E::TextureManager::GetInstance()->LoadTexture(tex);

	sprite_ = std::make_unique<K4E::Sprite>();
	sprite_->Initialize(tex);
	sprite_->SetAnchorPoint({ 0.5f, 0.5f });
	sprite_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
	sprite_->SetSize({ size_.x, size_.y });

	shadow_ = std::make_unique<K4E::Sprite>();
	shadow_->Initialize(tex);
	shadow_->SetAnchorPoint({ 0.5f, 0.5f });
	shadow_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
	shadow_->SetSize({ size_.x + 4.0f, size_.y + 4.0f });
	shadow_->SetColor({ 0,0,0,0.6f });
}

void Crosshair::RebuildAdsReticleSprites_()
{
	// CrosshairはMain Viewportの拡縮ではなく固定内部解像度1920x1080中心を基準にする。
	const float clientWidth = static_cast<float>(K4E::GameViewportConstants::Width);
	const float clientHeight = static_cast<float>(K4E::GameViewportConstants::Height);

	const std::string tex = NormalizeReticlePath_(adsTextureName_);
	if (tex.empty())
	{
		adsSprite_.reset();
		adsShadow_.reset();
		return;
	}

	K4E::TextureManager::GetInstance()->LoadTexture(tex);

	adsSprite_ = std::make_unique<K4E::Sprite>();
	adsSprite_->Initialize(tex);
	adsSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	adsSprite_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
	adsSprite_->SetSize({ size_.x, size_.y });
	adsSprite_->SetColor({ 1,1,1,0 });

	adsShadow_ = std::make_unique<K4E::Sprite>();
	adsShadow_->Initialize(tex);
	adsShadow_->SetAnchorPoint({ 0.5f, 0.5f });
	adsShadow_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
	adsShadow_->SetSize({ size_.x + 4.0f, size_.y + 4.0f });
	adsShadow_->SetColor({ 0,0,0,0 });
}

void Crosshair::RebuildAdsCenterDotSprites_()
{
	// CrosshairはMain Viewportの拡縮ではなく固定内部解像度1920x1080中心を基準にする。
	const float clientWidth = static_cast<float>(K4E::GameViewportConstants::Width);
	const float clientHeight = static_cast<float>(K4E::GameViewportConstants::Height);

	const std::string tex = NormalizeReticlePath_(adsCenterDotTextureName_);
	if (tex.empty())
	{
		adsDotSprite_.reset();
		adsDotShadow_.reset();
		return;
	}

	K4E::TextureManager::GetInstance()->LoadTexture(tex);

	adsDotSprite_ = std::make_unique<K4E::Sprite>();
	adsDotSprite_->Initialize(tex);
	adsDotSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	adsDotSprite_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
	adsDotSprite_->SetSize({ 12.0f, 12.0f });
	adsDotSprite_->SetColor({ 1,1,1,0 });

	adsDotShadow_ = std::make_unique<K4E::Sprite>();
	adsDotShadow_->Initialize(tex);
	adsDotShadow_->SetAnchorPoint({ 0.5f, 0.5f });
	adsDotShadow_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
	adsDotShadow_->SetSize({ 14.0f, 14.0f });
	adsDotShadow_->SetColor({ 0,0,0,0 });
}

void Crosshair::RebuildHitMarkerSprites_(const std::string& normalizedPath)
{
	// CrosshairはMain Viewportの拡縮ではなく固定内部解像度1920x1080中心を基準にする。
	const float clientWidth = static_cast<float>(K4E::GameViewportConstants::Width);
	const float clientHeight = static_cast<float>(K4E::GameViewportConstants::Height);

	std::string tex = NormalizeReticlePath_(normalizedPath);
	if (tex.empty()) tex = NormalizeReticlePath_(kDefaultHitTex);

	K4E::TextureManager::GetInstance()->LoadTexture(tex);
	currentHitMarkerTextureName_ = tex;

	hitMarkerSprite_ = std::make_unique<K4E::Sprite>();
	hitMarkerSprite_->Initialize(tex);
	hitMarkerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	hitMarkerSprite_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
	hitMarkerSprite_->SetSize({ hitBaseSize_, hitBaseSize_ });

	hitMarkerShadow_ = std::make_unique<K4E::Sprite>();
	hitMarkerShadow_->Initialize(tex);
	hitMarkerShadow_->SetAnchorPoint({ 0.5f,0.5f });
	hitMarkerShadow_->SetPosition({ clientWidth / 2.0f, clientHeight / 2.0f });
	hitMarkerShadow_->SetSize({ hitBaseSize_ + 4.0f, hitBaseSize_ + 4.0f });
	hitMarkerShadow_->SetColor({ 0,0,0,0.6f });
}

void Crosshair::TriggerHitMarker_(EHitMarkerKind kind)
{
	if (!enableHitMarker_) return;

	std::string targetTex = hitMarkerTextureName_;
	float duration = hitMarkerDurationCfg_;

	switch (kind)
	{
	case EHitMarkerKind::Headshot:
		if (useHeadshotMarker_ && !headshotHitMarkerTextureName_.empty())
		{
			targetTex = headshotHitMarkerTextureName_;
		}
		break;
	case EHitMarkerKind::KillConfirm:
		if (useKillConfirmMarker_ && !killConfirmMarkerTextureName_.empty())
		{
			targetTex = killConfirmMarkerTextureName_;
			duration = killConfirmDurationCfg_;
		}
		else if (useHeadshotMarker_ && !headshotHitMarkerTextureName_.empty())
		{
			// kill専用が無い場合はHS画像へフォールバック（任意）
			targetTex = headshotHitMarkerTextureName_;
			duration = killConfirmDurationCfg_;
		}
		else
		{
			duration = killConfirmDurationCfg_;
		}
		break;
	case EHitMarkerKind::Normal:
	default:
		break;
	}

	if (targetTex.empty())
	{
		targetTex = NormalizeReticlePath_(kDefaultHitTex);
	}

	if (targetTex != currentHitMarkerTextureName_ || !hitMarkerSprite_ || !hitMarkerShadow_)
	{
		RebuildHitMarkerSprites_(targetTex);
	}

	showHitMarker_ = true;
	activeHitMarkerDuration_ = std::max(0.01f, duration);
	hitMarkerTimer_ = activeHitMarkerDuration_;

	// キル確認は少し強めにポップ
	if (kind == EHitMarkerKind::KillConfirm)
	{
		hitMarkerScale_ = 2.0f;
		hitMarkerScaleVelocity_ = -5.0f;
	}
	else if (kind == EHitMarkerKind::Headshot)
	{
		hitMarkerScale_ = 1.9f;
		hitMarkerScaleVelocity_ = -4.5f;
	}
	else
	{
		hitMarkerScale_ = 1.8f;
		hitMarkerScaleVelocity_ = -4.0f;
	}

	hitAlpha_ = 1.0f;
}

std::string Crosshair::NormalizeReticlePath_(const std::string& path) const
{
	if (path.empty()) return "";

	std::string s = path;

	// バックスラッシュ対策
	for (char& c : s) { if (c == '\\') c = '/'; }

	// Editor/JSONでフルパスが来た場合の吸収
	const std::string prefix1 = "Resources/Textures/";
	const std::string prefix2 = "resources/textures/";

	auto startsWithNoCase = [](const std::string& str, const std::string& prefix)
		{
			if (str.size() < prefix.size()) return false;
			for (size_t i = 0; i < prefix.size(); ++i)
			{
				if (std::tolower((unsigned char)str[i]) != std::tolower((unsigned char)prefix[i])) return false;
			}
			return true;
		};

	if (startsWithNoCase(s, prefix1)) s = s.substr(prefix1.size());
	else if (startsWithNoCase(s, prefix2)) s = s.substr(prefix2.size());

	// reticle / reticles の揺れを吸収（必要なら）
	if (s.rfind("reticle/", 0) == 0)
	{
		s = "reticles/" + s.substr(std::string("reticle/").size());
	}

	return s;
}
