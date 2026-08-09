#pragma once

#include <array>
#include <memory>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Sprite.h"

namespace K4E = ::Ken4lowEngine;

enum class DamageIndicatorDir : uint8_t
{
	Up = 0,
	Down,
	Left,
	Right,
	Count
};

struct DamageIndicatorSlot
{
	bool active = false;
	float timer = 0.0f;
	float duration = 0.6f;

	K4E::Vector2 drawPos = {};
	K4E::Vector2 drawSize = {};
	K4E::Vector4 drawColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float drawRotation = 0.0f;
};

class DamageIndicatorManager
{
public:
	void Initialize();
	void Update(float deltaTime);
	void Draw();

	void AddIndicator(
		const K4E::Vector3& playerPos,
		const K4E::Vector3& attackerPos,
		const K4E::Vector3& cameraForward,
		const K4E::Vector3& cameraRight);

private:
	float Clamp01(float v) const;
	int ToIndex(DamageIndicatorDir dir) const;
	void UpdateSlotVisual(DamageIndicatorSlot& slot, DamageIndicatorDir dir, const K4E::Vector2& screenCenter);

private:
	std::array<DamageIndicatorSlot, 4> slots_;
	std::array<std::unique_ptr<K4E::Sprite>, 4> sprites_;
};