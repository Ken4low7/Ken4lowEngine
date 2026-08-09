#pragma once
#include <TextSpriteDrawer.h>
#include <Vector2.h>
#include <memory>

namespace K4E = ::Ken4lowEngine;

class NoAmmoUI
{
public:
	void Initialize();
	void Update(float deltaTime);
	void Draw();
	void Finalize();

	void SetVisible(bool v) { visible_ = v; }
	bool IsVisible() const { return visible_; }

private:
	std::unique_ptr<K4E::TextSpriteDrawer> textDrawer_;
	bool visible_ = false;
	bool isReady_ = false;

	float blinkTimer_ = 0.0f;
	float alpha_ = 0.0f;

	K4E::Vector2 position_ = { 960.0f, 700.0f }; // 1920x1080想定の中央やや下
	float scale_ = 0.9f;
};