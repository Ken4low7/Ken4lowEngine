#include "ReloadCircle.h"
#include "GameViewportConstants.h"
#include "winApp.h"
#include <DirectXCommon.h>
#include <numbers>

namespace K4E = ::Ken4lowEngine;


/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
void ReloadCircle::Initialize(const std::string& texturePath)
{
	sprite_ = std::make_unique<K4E::Sprite>();
	sprite_->Initialize(texturePath);
	sprite_->SetAnchorPoint({ 0.5f, 0.5f }); // アンカーを中央に設定
	sprite_->SetSize({ 64, 64 }); // サイズは適宜調整
	sprite_->SetRotation(-std::numbers::pi_v<float> / 2.0f); // 初期回転
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void ReloadCircle::Update()
{
	// 武器とスプライトがセットされていない場合は処理しない
	if (!sprite_) return;

	// リロード円は固定内部解像度1920x1080の中央に配置する。
	sprite_->SetPosition({ static_cast<float>(K4E::GameViewportConstants::Width) * 0.5f,
						   static_cast<float>(K4E::GameViewportConstants::Height) * 0.5f });

	// 位置/サイズ反映
	sprite_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void ReloadCircle::Draw()
{
	if (isVisible_) sprite_->Draw();
}


/// -------------------------------------------------------------
///				　	　リロードの進捗を設定
/// -------------------------------------------------------------
void ReloadCircle::SetProgress(float progress)
{
	progress_ = std::clamp(progress, 0.0f, 1.0f);
	if (sprite_) sprite_->SetReloadProgress(true, progress); // HLSLへ進行度を反映
}

/// -------------------------------------------------------------
///				　	リロード状態を設定
/// -------------------------------------------------------------
void ReloadCircle::SetReloading(bool isReloading, float progress)
{
	isVisible_ = isReloading;
	progress_ = std::clamp(progress, 0.0f, 1.0f);

	if (sprite_)
	{
		// シェーダーへ渡す（円の塗り）
		sprite_->SetReloadProgress(isReloading, progress_);
	}
}