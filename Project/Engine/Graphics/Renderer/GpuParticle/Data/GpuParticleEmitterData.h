#pragma once
#include <cstdint>
#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>

#include "GpuParticleType.h"
#include "BillboardMode.h"

namespace Ken4lowEngine
{

// エミッターの球体情報
struct GpuEmitterCBData
{
	Vector3 translate;		// 位置
	float radius;			// 半径
	uint32_t count;			// 発生数
	float frequency;		// 発生頻度
	float frequencyTime;	// 発生頻度タイマー
	uint32_t emit; 			// 発生フラグ
	uint32_t type;			// エミッターの種類
	uint32_t billboardMode; // ビルボードモード
	float lifeScale = 1.0f;	// 寿命倍率
	float speedScale = 1.0f;	// 初速度倍率
	uint32_t overrideFlags = 0; // Desc由来の生成値上書きフラグ（0なら既存挙動）
	uint32_t maxParticles = UINT32_MAX; // Emitter単位のCPU側発生上限
	float overridePadding[2]{};
	Vector3 positionRandom{};
	float lifeTime = 1.0f;
	Vector3 velocity{};
	float lifeTimeRandom = 0.0f;
	Vector3 velocityRandom{};
	float sizeRandom = 0.0f;
	Vector2 startSize{ 1.0f, 1.0f };
	Vector2 endSize{ 1.0f, 1.0f };
	Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
	Vector3 gravity{};
	float damping = 0.0f;
	float speed = 0.0f;
	float speedRandom = 0.0f;
	float startRotation = 0.0f;
	float rotationSpeed = 0.0f;
	float rotationRandom = 0.0f;
	float spawnRadius = 0.0f;
	uint32_t spawnShape = 0;
	uint32_t alphaFade = 1;
	Vector3 spawnBoxSize{};
	float spawnBoxPadding = 0.0f;
	Vector4 colorRandom{};
	Vector3 startScale3D{ 1.0f, 1.0f, 1.0f };
	float startScalePadding = 0.0f;
	Vector3 endScale3D{ 1.0f, 1.0f, 1.0f };
	float endScalePadding = 0.0f;
	uint32_t useSpriteSheet = 0;
	uint32_t spriteSheetRows = 1;
	uint32_t spriteSheetColumns = 1;
	float spriteSheetFrameRate = 0.0f;
};

// HLSLのEmitterCBDataと16byteパッキングを一致させる。
static_assert(sizeof(GpuEmitterCBData) == 288);
} // namespace Ken4lowEngine
