#pragma once

#include "EnemySpawnCrystal.h"
#include "EnemyHPBar.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class Sprite;
	class TextSpriteDrawer;
}

/// -------------------------------------------------------------
/// クリスタル頭上HPバーの生成・更新・描画・Debug表示を担当するクラス。
///
/// CrystalManagerからHPバー専用の状態を切り離し、クリスタル本体の
/// 生成・破壊・敵スポーン管理とUI表示管理が混ざらないようにする。
/// -------------------------------------------------------------
class CrystalHpBarController
{
public:
	struct Settings
	{
		bool visible = true;
		bool alwaysVisible = true;
		bool showDistance = true;
		bool showOffscreenMarker = true;
		float offsetY = 0.55f;
		float width = 140.0f;
		float height = 14.0f;
		float showTime = 3.0f;
		float markerMargin = 58.0f;
		float markerSize = 26.0f;
		float hpTextScale = 0.48f;
		float distanceTextScale = 0.48f;
		float objectiveNoticeTime = 2.6f;
	};

	struct DebugInfo
	{
		int hp = 0;
		int maxHp = 0;
		float hpRate = 0.0f;
		float distance = 0.0f;
		K4E::Vector3 worldPosition{};
		K4E::Vector2 screenPosition{};
		K4E::Vector2 markerPosition{};
		K4E::Vector2 hpLabelPosition{};
		K4E::Vector2 distanceLabelPosition{};
		bool active = false;
		bool broken = false;
		bool inFront = false;
		bool inScreen = false;
		bool visible = false;
		bool hpLabelVisible = false;
		bool markerVisible = false;
		bool distanceVisible = false;
		std::string hiddenReason;
	};

public:
	~CrystalHpBarController();

	void Initialize(size_t crystalCount);
	void Finalize();
	void Update(const std::vector<EnemySpawnCrystal>& crystals, const K4E::Matrix4x4& viewMatrix, const K4E::Matrix4x4& projMatrix, float screenWidth, float screenHeight, float deltaTime, const EnemySpawnCrystal* aimedCrystal, bool showOnlyWhenAimed, float visibleHoldTime);
	void Draw();
	void DrawImGui() const;

	Settings& GetSettings() { return settings_; }
	const Settings& GetSettings() const { return settings_; }

private:
	void EnsureBarCount(size_t crystalCount);
	void InitializeTextRenderer();
	void UpdateObjectiveNotice(const std::vector<EnemySpawnCrystal>& crystals, float deltaTime);
	K4E::Vector2 BuildOffscreenMarkerPosition(const K4E::Vector3& worldPosition, const K4E::Matrix4x4& viewMatrix, float screenWidth, float screenHeight) const;

private:
	Settings settings_{};
	std::vector<std::unique_ptr<EnemyHPBar>> hpBars_;
	std::vector<std::unique_ptr<K4E::Sprite>> directionMarkers_;
	std::unique_ptr<K4E::TextSpriteDrawer> textDrawer_;
	std::vector<DebugInfo> debugInfos_;
	std::vector<float> aimTimers_;
	bool textReady_ = false;
	bool drawCalled_ = false;
	int visibleCount_ = 0;
	int previousAliveCrystalCount_ = -1;
	float objectiveNoticeTimer_ = 0.0f;
	float markerPulseTimer_ = 0.0f;
	float screenWidth_ = 1920.0f;
	float screenHeight_ = 1080.0f;
	std::string objectiveNoticeText_;
};
