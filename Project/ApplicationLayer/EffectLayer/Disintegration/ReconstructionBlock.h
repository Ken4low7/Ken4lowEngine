#pragma once
#include "Vector3.h"
#include "Vector4.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// モデル再構築専用のCPUブロックデータ
/// -------------------------------------------------------------
struct ReconstructionBlock
{
	K4E::Vector3 startPosition{};
	K4E::Vector3 targetPosition{};
	K4E::Vector3 targetNormal{ 0.0f, 1.0f, 0.0f };
	K4E::Vector3 position{};
	K4E::Vector3 rotation{};
	K4E::Vector3 startRotation{};
	K4E::Vector3 rotationVelocity{};
	K4E::Vector3 scale{ 1.0f, 1.0f, 1.0f };
	K4E::Vector4 color{ 0.64f, 0.72f, 0.86f, 1.0f };
	float age = 0.0f;
	float startDelay = 0.0f;
	float size = 0.04f;
	float alpha = 1.0f;
	float fadeInDelay = 0.0f;
	float fadeInDuration = 0.4f;
	float appearAge = 0.0f;
	float visibility = 1.0f;
	float startDistance = 0.0f;
	bool alive = false;
	bool arrived = false;
};
