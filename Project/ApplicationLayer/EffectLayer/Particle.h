#pragma once
#include "ParticleTransform.h"	
#include "Vector3.h"
#include "Vector4.h"

namespace K4E = ::Ken4lowEngine;

// パーティクルのエフェクトタイプ
enum class ParticleMode
{
	Orbit,     // 回転（チャージ中）
	Explode    // 爆発（チャージ解除後）
};

// パーティクル用の構造体
struct Particle
{
	K4E::ParticleTransform transform{};	 // 位置
	K4E::Vector3 velocity = {};	 // 速度
	K4E::Vector4 color = {};		 // 色
	float lifeTime = 0;		 // 生存可能な時間
	float currentTime = 0;	 // 発生してからの経過時間

	// スケールアニメーション用（追加）
	K4E::Vector3 startScale = { 1.0f, 1.0f, 1.0f };
	K4E::Vector3 endScale = { 0.0f, 0.0f, 0.0f };

	K4E::Vector3 orbitCenter{};         // 回転の中心
	K4E::Vector3 orbitAxis{};           // 回転軸（Normalize済）
	float orbitRadius = 1.0f;      // 回転半径
	float orbitSpeed = 1.0f;       // 速度（時間の倍率）
	float orbitPhase = 0.0f;       // 初期角度

	ParticleMode mode = ParticleMode::Orbit; // パーティクルのモード（回転 or 爆発）
};
