#pragma once
#include "Vector3.h"
#include "Vector4.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// モデル崩壊専用のCPU粒子データ
/// -------------------------------------------------------------
struct DisintegrationParticle
{
	K4E::Vector3 initialPosition{};
	K4E::Vector3 origin{};
	K4E::Vector3 position{};
	K4E::Vector3 rotation{};
	K4E::Vector3 rotationVelocity{};
	K4E::Vector3 scale{ 1.0f, 1.0f, 1.0f };
	K4E::Vector3 velocity{};
	K4E::Vector3 outward{};
	K4E::Vector4 color{ 0.72f, 0.66f, 0.55f, 1.0f };
	K4E::Vector4 edgeColor{ 0.0f, 0.0f, 0.0f, 0.0f };
	float age = 0.0f;
	float life = 1.0f;
	float startDelay = 0.0f;
	float size = 0.04f;
	float alpha = 1.0f;
	float edgeFactor = 0.0f;
	float erosionNoise = 0.5f;
	float sweepCoord = 0.0f;
	float centerDistance = 0.0f;
	bool active = true;
	bool alive = false;
};
