#pragma once
#include <Sprite.h>
#include "Vector4.h"

#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///                            十字カーソルクラス
/// -------------------------------------------------------------
class Crosshair
{
public:
	// 初期化（テクスチャロード）
	void Initialize(const std::string& texturePath = "UI/Reticles/crosshair_circle_dot.dds");

	// 更新
	void Update();

	// 描画
	void Draw();

	// ヒットマーカー表示開始（既存互換: 通常ヒット）
	void ShowHitMarker();
	void ShowHeadshotMarker();
	void ShowKillConfirmMarker();
	void NotifyEnemyHit(bool isHeadshot = false, bool isKill = false);

	// 表示ON/OFF切替
	void SetVisible(bool visible) { isVisible_ = visible; }

	// 表示状態取得
	bool IsVisible() const { return isVisible_; }

	// =========================================================
	// Weapon reticleData を反映する setter 群
	// =========================================================
	void SetReticleTexture(const std::string& texturePath);               // HIP
	void SetADSReticleTexture(const std::string& texturePath);            // ADS override
	void SetADSCenterDotTexture(const std::string& texturePath);          // ADS dot
	void SetReticleType(int type) { reticleType_ = type; } // 今は保持だけ（将来用）

	void SetBaseSize(float v) { baseSizePx_ = (v > 1.0f ? v : 1.0f); }
	void SetMaxSize(float v) { maxSizePx_ = (v > baseSizePx_ ? v : baseSizePx_); }

	void SetExpandPerShot(float v) { expandPerShot_ = (v >= 0.0f ? v : 0.0f); }
	void SetRecoverSpeed(float v) { recoverSpeed_ = (v >= 0.0f ? v : 0.0f); }

	void SetHideInADS(bool v) { hideInADS_ = v; }
	void SetADSState(bool v) { isADS_ = v; }
	void SetReloadState(bool v) { isReloading_ = v; }
	void SetHideWhileReload(bool v) { hideWhileReload_ = v; }
	void SetUseADSReticleOverride(bool v) { useAdsReticleOverride_ = v; }
	void SetUseADSCenterDot(bool v) { useAdsCenterDot_ = v; }
	void SetADSBlendTime(float sec) { adsBlendTime_ = (sec > 0.001f ? sec : 0.001f); }

	void SetShowHitMarker(bool v) { enableHitMarker_ = v; }
	void SetHitMarkerTexture(const std::string& texturePath);
	void SetHeadshotHitMarkerTexture(const std::string& texturePath);
	void SetKillConfirmMarkerTexture(const std::string& texturePath);
	void SetUseHeadshotMarker(bool v) { useHeadshotMarker_ = v; }
	void SetUseKillConfirmMarker(bool v) { useKillConfirmMarker_ = v; }
	void SetHitMarkerDuration(float sec) { hitMarkerDurationCfg_ = (sec > 0.01f ? sec : 0.01f); }
	void SetKillConfirmDuration(float sec) { killConfirmDurationCfg_ = (sec > 0.01f ? sec : 0.01f); }

	// WeaponInstance 側の spread（動的拡散値）を渡す
	void SetSpreadValue(float spreadDeg) { spreadValueDeg_ = (spreadDeg >= 0.0f ? spreadDeg : 0.0f); }

	// APEXっぽい移動拡散用（状態は外部から渡す）
	void SetMoveExpandEnabled(bool v) { enableMoveExpand_ = v; }
	void SetMoveExpandMultipliers(float walkMul, float sprintMul, float airMul, float landImpulse);
	void SetMovementState(bool isMoving, bool isSprinting, bool isAirborne);
	void NotifyLanded();
	
	void SetTargetingEnemy(bool flag) { isTargetingEnemy_ = flag; }
	void SetTargetColors(const K4E::Vector4& normalColor, const K4E::Vector4& targetColor)
	{
		normalColor_ = normalColor;
		targetColor_ = targetColor;
	}

private:
	enum class EHitMarkerKind
	{
		Normal,
		Headshot,
		KillConfirm,
	};

	// 内部ユーティリティ
	void RebuildReticleSprites_();
	void RebuildAdsReticleSprites_();
	void RebuildAdsCenterDotSprites_();
	void RebuildHitMarkerSprites_(const std::string& normalizedPath);
	void TriggerHitMarker_(EHitMarkerKind kind);
	std::string NormalizeReticlePath_(const std::string& path) const;

private:
	// HIPレティクル
	std::unique_ptr<K4E::Sprite> sprite_; // 十字カーソル本体
	std::unique_ptr<K4E::Sprite> shadow_; // 影

	// ADS別レティクル
	std::unique_ptr<K4E::Sprite> adsSprite_;
	std::unique_ptr<K4E::Sprite> adsShadow_;

	// ADS中央ドット
	std::unique_ptr<K4E::Sprite> adsDotSprite_;
	std::unique_ptr<K4E::Sprite> adsDotShadow_;

	std::string textureName_;                  // HIPレティクル
	std::string adsTextureName_;               // ADSレティクル
	std::string adsCenterDotTextureName_;      // ADSドット

	K4E::Vector2 size_ = { 16, 16 };           // 現在サイズ（描画用）
	bool isVisible_ = true;                    // 表示フラグ

	// =========================================================
	// レティクル設定
	// =========================================================
	int   reticleType_ = 0;                    // EReticleType相当（今は未使用でも保持）
	float baseSizePx_ = 12.0f;                 // 通常サイズ（武器データ）
	float maxSizePx_ = 28.0f;                  // 最大サイズ（武器データ）
	float expandPerShot_ = 2.0f;               // 将来用/保持
	float recoverSpeed_ = 18.0f;               // 将来用/保持

	bool  hideInADS_ = false;                  // ADS時に隠す
	bool  isADS_ = false;                      // 現在ADS中か
	bool  isReloading_ = false;                // 現在リロード中か
	bool  hideWhileReload_ = true;             // リロード中にレティクル本体を隠す
	bool  useAdsReticleOverride_ = false;      // ADS時に別画像を使う
	bool  useAdsCenterDot_ = false;            // ADS時に中央ドットを使う
	float adsBlendTime_ = 0.06f;               // ADS切替ブレンド時間
	float adsBlendAlpha_ = 0.0f;               // 0=HIP,1=ADS

	bool  enableHitMarker_ = true;             // ヒットマーカー有効

	float spreadValueDeg_ = 0.0f;              // WeaponInstanceから渡す spread
	float spreadToUiScale_ = 0.25f;            // 度→UIサイズ変換係数（調整用）

	// 移動拡散（外部状態入力）
	bool  enableMoveExpand_ = true;
	float moveExpandMultiplier_ = 1.15f;
	float sprintExpandMultiplier_ = 1.35f;
	float airExpandMultiplier_ = 1.60f;
	float landExpandImpulseCfg_ = 2.0f;
	bool  isMoving_ = false;
	bool  isSprinting_ = false;
	bool  isAirborne_ = false;
	float landExpandImpulseCurrent_ = 0.0f;

	// ヒットマーカー（通常 / HS / Kill）
	bool showHitMarker_ = false;
	float hitMarkerTimer_ = 0.0f;
	float activeHitMarkerDuration_ = 0.25f;
	float hitMarkerDurationCfg_ = 0.06f;
	float killConfirmDurationCfg_ = 0.12f;

	bool useHeadshotMarker_ = false;
	bool useKillConfirmMarker_ = false;
	std::string hitMarkerTextureName_;
	std::string headshotHitMarkerTextureName_;
	std::string killConfirmMarkerTextureName_;
	std::string currentHitMarkerTextureName_;

	float hitMarkerScale_ = 1.0f;
	float hitMarkerScaleVelocity_ = 0.0f;

	std::unique_ptr<K4E::Sprite> hitMarkerSprite_;
	std::unique_ptr<K4E::Sprite> hitMarkerShadow_;
	float hitAlpha_ = 0.0f;

	float hitBaseSize_ = 112.0f;

	bool isTargetingEnemy_ = false;
	K4E::Vector4 normalColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
	K4E::Vector4 targetColor_{ 1.0f, 0.2f, 0.2f, 1.0f };
};
