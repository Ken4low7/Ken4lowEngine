#pragma once
#include <Sprite.h>
#include <Vector2.h>
#include <memory>

namespace K4E = ::Ken4lowEngine;

class ControlGuideUI
{
private:/// ---------- 内部構造体 ---------- ///

	// アイコンペアの構造体
	struct GuidePair
	{
		std::unique_ptr<K4E::Sprite> iconA;
		std::unique_ptr<K4E::Sprite> iconB;
		K4E::Vector2 basePos = { 0.0f, 0.0f };
		K4E::Vector2 iconSizeA = { 32.0f, 32.0f };
		K4E::Vector2 iconSizeB = { 32.0f, 32.0f };
		float gap = 12.0f;
	};

public: /// ---------- メンバ関数 ---------- ///

	void Initialize(
		const std::string& ammoIconPath,
		const std::string& leftClickPath,
		const std::string& reticleIconPath,
		const std::string& rightClickPath,
		const std::string& rKeyIconPath,
		const std::string& reloadIconPath
	);

	void Update(float deltaTime);
	void Draw();

	void SetVisible(bool v) { isVisible_ = v; }
	bool IsVisible() const { return isVisible_; }

	void SetAlpha(float alpha) { alpha_ = alpha; }
	void SetAnchorTopLeft(const K4E::Vector2& pos) { anchorTopLeft_ = pos; }

private:
	void UpdatePairLayout(GuidePair& pair);

private:
	bool isVisible_ = true;
	float alpha_ = 1.0f;
	float pulseTimer_ = 0.0f;

	K4E::Vector2 anchorTopLeft_ = { 40.0f, 40.0f };

	GuidePair shootGuide_;
	GuidePair adsGuide_;
	GuidePair reloadGuide_;
};