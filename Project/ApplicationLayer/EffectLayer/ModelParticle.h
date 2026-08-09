#pragma once
#include "Object3D.h"

#include <memory>
#include <vector>
#include <random>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }

/// -------------------------------------------------------------
///				　		　モデルパーティクル
/// -------------------------------------------------------------
class ModelParticle
{
	struct ModelParticleInfo
	{
		std::unique_ptr<K4E::Object3D> obj;
		K4E::Vector3 pos{};
		K4E::Vector3 vel{};
		K4E::Vector3 angVel{};   // 角速度(ラジアン/秒)
		K4E::Vector3 euler{};    // オイラー角
		float   ttl = 0.0f; // 残り寿命
		float   scale = 1.0f;
		bool    alive = false;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

	void SpawnBurst(const K4E::Vector3& pos, const K4E::Vector3& normal, uint32_t count);

	void OnHit(const K4E::Vector3& hitPos, const K4E::Vector3& hitNormal);
	void SetBurstTuning(float baseSpeed, float gravityY, float lifeMin, float lifeMax, float startScale);
	void SetParticleColor(const K4E::Vector4& color);
	uint32_t GetActiveCount() const;

private: /// ---------- メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // 入力

	std::unique_ptr<K4E::Object3D> object3D_; // 3Dオブジェクト

	// パーティクルプール
	std::vector<ModelParticleInfo> pool_;
	size_t poolMax_ = 256;

	// チューニング用パラメータ
	uint32_t defaultCount_ = 12;   // 一回の破片数（ピクセルガン風は多めが映える）
	float    baseSpeed_ = 6.0f; // 初速
	float    spread_ = 0.8f; // ばらけ具合（法線からの拡散）
	float    gravityY_ = -9.8f;
	float    lifeMin_ = 0.25f; // 寿命の最小値
	float    lifeMax_ = 0.45f; // 寿命の最大値
	float    startScale_ = 0.12f; // 立方体片の初期スケール
	float    shrinkRate_ = 0.9f;  // 経時縮小（1秒あたりの係数）

	// RNG
	std::mt19937 rng_{ 0xC0FFEE }; // シード値は適当
	std::uniform_real_distribution<float> urand_{ 0.0f, 1.0f };

	// 表示フラグ（従来の単体表示用）
	bool isActive_ = false;
	K4E::Vector4 particleColor_{ 1.0f, 0.5f, 0.0f, 1.0f };
};
