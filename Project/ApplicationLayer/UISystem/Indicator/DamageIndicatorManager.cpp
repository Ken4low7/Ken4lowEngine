#define NOMINMAX
#include "GameViewportConstants.h"
#include "DamageIndicatorManager.h"
#include "DirectXCommon.h"
#include "LinearInterpolation.h"
#include <algorithm>
#include <cmath>

void DamageIndicatorManager::Initialize()
{
	for (auto& sprite : sprites_)
	{
		sprite = std::make_unique<K4E::Sprite>();
		sprite->Initialize("Effects/white.dds");
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
		sprite->Update();
	}
}

float DamageIndicatorManager::Clamp01(float v) const
{
	return std::clamp(v, 0.0f, 1.0f);
}

int DamageIndicatorManager::ToIndex(DamageIndicatorDir dir) const
{
	return static_cast<int>(dir);
}

void DamageIndicatorManager::AddIndicator(
	const K4E::Vector3& playerPos,
	const K4E::Vector3& attackerPos,
	const K4E::Vector3& cameraForward,
	const K4E::Vector3& cameraRight)
{
	K4E::Vector3 toAttacker = attackerPos - playerPos;
	toAttacker.y = 0.0f;

	const float lenSq =
		toAttacker.x * toAttacker.x +
		toAttacker.z * toAttacker.z;

	if (lenSq <= 0.0001f)
	{
		return;
	}

	const float invLen = 1.0f / std::sqrt(lenSq);
	toAttacker.x *= invLen;
	toAttacker.z *= invLen;

	K4E::Vector3 flatForward = cameraForward;
	flatForward.y = 0.0f;
	const float forwardLenSq =
		flatForward.x * flatForward.x +
		flatForward.z * flatForward.z;

	if (forwardLenSq > 0.0001f)
	{
		const float invForwardLen = 1.0f / std::sqrt(forwardLenSq);
		flatForward.x *= invForwardLen;
		flatForward.z *= invForwardLen;
	}

	K4E::Vector3 flatRight = cameraRight;
	flatRight.y = 0.0f;
	const float rightLenSq =
		flatRight.x * flatRight.x +
		flatRight.z * flatRight.z;

	if (rightLenSq > 0.0001f)
	{
		const float invRightLen = 1.0f / std::sqrt(rightLenSq);
		flatRight.x *= invRightLen;
		flatRight.z *= invRightLen;
	}

	const float frontDot =
		flatForward.x * toAttacker.x +
		flatForward.z * toAttacker.z;

	const float rightDot =
		flatRight.x * toAttacker.x +
		flatRight.z * toAttacker.z;

	DamageIndicatorDir dir = DamageIndicatorDir::Up;

	if (std::fabs(frontDot) >= std::fabs(rightDot))
	{
		dir = (frontDot >= 0.0f) ? DamageIndicatorDir::Up : DamageIndicatorDir::Down;
	}
	else
	{
		dir = (rightDot >= 0.0f) ? DamageIndicatorDir::Right : DamageIndicatorDir::Left;
	}

	auto& slot = slots_[ToIndex(dir)];
	slot.active = true;
	slot.timer = slot.duration;
}

void DamageIndicatorManager::Update(float deltaTime)
{
	// ダメージ方向UIは固定内部解像度1920x1080の中心を基準にする。
	uint32_t width = K4E::GameViewportConstants::Width;
	uint32_t height = K4E::GameViewportConstants::Height;

	const K4E::Vector2 screenCenter =
	{
		static_cast<float>(width) * 0.5f,
		static_cast<float>(height) * 0.5f
	};

	for (int i = 0; i < 4; ++i)
	{
		auto& slot = slots_[i];

		if (!slot.active)
		{
			continue;
		}

		slot.timer -= deltaTime;
		if (slot.timer <= 0.0f)
		{
			slot.timer = 0.0f;
			slot.active = false;
			continue;
		}

		UpdateSlotVisual(slot, static_cast<DamageIndicatorDir>(i), screenCenter);
	}
}

void DamageIndicatorManager::UpdateSlotVisual(
	DamageIndicatorSlot& slot,
	DamageIndicatorDir dir,
	const K4E::Vector2& screenCenter)
{
	const float t = 1.0f - (slot.timer / slot.duration);

	// スッと消える
	const float alpha = K4E::EaseOutCubic(1.0f - t);

	// 出た瞬間だけ少し大きい
	const float scale = 1.15f - 0.15f * t;

	// 出た瞬間だけ少し外へ飛ばす
	const float push = 16.0f * (1.0f - t);

	K4E::Vector2 pos = screenCenter;
	K4E::Vector2 baseSize = { 80.0f, 12.0f };

	switch (dir)
	{
	case DamageIndicatorDir::Up:
		pos.y -= (80.0f + push);
		baseSize = { 90.0f, 10.0f };
		break;

	case DamageIndicatorDir::Down:
		pos.y += (80.0f + push);
		baseSize = { 90.0f, 10.0f };
		break;

	case DamageIndicatorDir::Left:
		pos.x -= (80.0f + push);
		baseSize = { 10.0f, 90.0f };
		break;

	case DamageIndicatorDir::Right:
		pos.x += (80.0f + push);
		baseSize = { 10.0f, 90.0f };
		break;

	default:
		break;
	}

	const float flashDuration = 0.18f;
	const float flashT = Clamp01(t / flashDuration);

	const K4E::Vector4 flashColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	const K4E::Vector4 damageColor = { 1.0f, 0.15f, 0.15f, 1.0f };
	K4E::Vector4 color = K4E::Lerp(flashColor, damageColor, flashT);
	color.w = alpha * 0.9f;

	slot.drawPos = pos;
	slot.drawSize = {
		baseSize.x * scale,
		baseSize.y * scale
	};
	slot.drawColor = color;
	slot.drawRotation = 0.0f;
}

void DamageIndicatorManager::Draw()
{
	for (int i = 0; i < 4; ++i)
	{
		auto& slot = slots_[i];
		auto& sprite = sprites_[i];

		if (!slot.active || !sprite)
		{
			continue;
		}

		sprite->SetPosition(slot.drawPos);
		sprite->SetSize(slot.drawSize);
		sprite->SetRotation(slot.drawRotation);
		sprite->SetColor(slot.drawColor);
		sprite->Update();
		sprite->Draw();
	}
}