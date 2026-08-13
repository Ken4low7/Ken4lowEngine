#pragma once
#include <array>
#include <string>
#include <deque>
#include <limits>
#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>

#include "GpuParticleType.h"		// GPUパーティクルの種類
#include "GpuParticleEmitterData.h" // エミッターのCBデータ
#include "BillboardMode.h" // ビルボードモード

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///			　	GPUパーティクルエミッタークラス
/// -------------------------------------------------------------
class GpuParticleEmitter
{
public: /// ---------- 構造体 ---------- ///

	/// エミッター情報構造体
	struct EmitterInfo
	{
		std::string textureFilePath;
		float radius = 0.0f;

		// ループ発生（定期発生）
		uint32_t loopCount = 0;
		float loopFrequency = 0.0f;

		// 描画ID（0ならtypeを使う）
		uint32_t drawType = 0;

		// ★差別化の核：モード（Sprite / Ribbon など）
		GpuParticleKind kind = GpuParticleKind::Sprite;

		// ★Sprite用：21タイプ（DefaultはUIに出さない運用）
		GpuParticleType spriteType = GpuParticleType::Default;

		// ★Ribbon用：リボンタイプ（UIで別枠にする）
		GpuRibbonType ribbonType = GpuRibbonType::Trail;

		// ★下位16bitのフラグ（Camera/YAxis など）
		BillboardMode billboardFlags = BillboardMode::Camera;

		// 攻撃ヒットなど同一タイプ内の差別化用に、寿命と初速度だけエミッター単位で上書きする。
		float lifeScale = 1.0f;
		float speedScale = 1.0f;

		// Authoring DescをGPU生成値へ直接渡し、既存Type Preset経路とは独立して演出を組めるようにする。
		bool useDescSpawnOverride = false;
		uint32_t maxParticles = (std::numeric_limits<uint32_t>::max)();
		Vector3 positionRandom{};
		Vector3 velocity{};
		Vector3 velocityRandom{};
		Vector2 startSize{ 1.0f, 1.0f };
		Vector2 endSize{ 1.0f, 1.0f };
		Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		float lifeTime = 1.0f;
		float lifeTimeRandom = 0.0f;
		Vector3 gravity{};
		float damping = 0.0f;
		float speed = 0.0f;
		float speedRandom = 0.0f;
		float sizeRandom = 0.0f;
		float startRotation = 0.0f;
		float rotationSpeed = 0.0f;
		float rotationRandom = 0.0f;
		uint32_t spawnShape = 0;
		float spawnRadius = 0.0f;
		Vector3 spawnBoxSize{};
		Vector4 colorRandom{};
		bool alphaFade = true;
		Vector3 startScale3D{ 1.0f, 1.0f, 1.0f };
		Vector3 endScale3D{ 1.0f, 1.0f, 1.0f };
		bool useSpriteSheet = false;
		uint32_t spriteSheetRows = 1;
		uint32_t spriteSheetColumns = 1;
		float spriteSheetFrameRate = 0.0f;

		bool useSizeCurve = false;
		Vector4 sizeCurveLut{ 1.0f, 1.0f, 1.0f, 1.0f };
		bool useColorGradient = false;
		std::array<Vector4, 4> colorGradientLut{
			Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.66f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.33f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.0f }
		};

		float noiseStrength = 0.0f;
		float noiseFrequency = 1.0f;
		Vector3 vortexAxis{ 0.0f, 1.0f, 0.0f };
		float vortexStrength = 0.0f;
		Vector3 attractorPosition{};
		float attractorStrength = 0.0f;
		float attractorRadius = 0.0f;

		Vector3 startRotation3D{};
		Vector3 rotationRandom3D{};
		Vector3 angularVelocity{};
		Vector3 angularVelocityRandom{};
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// エミッターのコンストラクタ。<br/>
	/// 名前と基本設定（EmitterInfo）を受け取り、ループタイマーや累積発生数を初期化します。
	/// </summary>
	/// <param name="name">エミッター名（識別用のキー）</param>
	/// <param name="info">エミッターの基本設定</param>
	GpuParticleEmitter(const std::string& name, const EmitterInfo& info);

	/// <summary>
	/// 1 フレーム分の「追加バースト発生」をリクエストします。<br/>
	/// 引数 count を pendingBurstCount_ に加算するだけで、実際のエミットは BuildCB() を通じて行われます。<br/>
	/// Update() 側から複数回呼ばれても、そのフレーム内で合算されて利用されます。
	/// </summary>
	/// <param name="count">このフレームに追加で発生させたいパーティクル数</param>
	/// <returns>maxParticlesと発生待ち数を考慮して、Runtimeが実際に受理した数。</returns>
	uint32_t RequestEmit(uint32_t count);

	/// 発生済み粒子が残っている間だけ描画するための軽量更新。
	void UpdateActivity(float deltaTime);

	/// <summary>
	/// 定期発射（ループ）とバースト発生をまとめて処理し、GPU に渡すエミッター用 CB データを構築します。
	/// </summary>
	bool BuildCB(GpuEmitterCBData& out, float deltaTime);

public: /// ---------- セッター ---------- ///

	/// <summary>エミッターのワールド座標を設定します。</summary>
	void SetPosition(const Vector3& position) { position_ = position; }

public: /// ---------- ゲッター ---------- ///

	const std::string& GetName() const { return name_; }
	const EmitterInfo& GetInfo() const { return info_; }

	// 描画に使うID（0なら type を返す）
	uint32_t GetDrawType() const
	{
		const uint32_t effectiveType = GetEffectiveType();
		return (info_.drawType != 0) ? info_.drawType : effectiveType;
	}

	// Runtime Parameter反映とImGui編集で共通利用する。
	EmitterInfo& GetInfoMutable() { return info_; }

	const Vector3 GetPosition() const { return position_; }
	uint32_t GetEstimatedActiveParticleCount() const { return estimatedActiveParticleCount_; }
	bool HasActiveParticles() const { return estimatedActiveParticleCount_ > 0 || pendingBurstCount_ > 0; }

private: /// ---------- プライベート関数 ---------- ///

	static constexpr uint32_t ToU32(BillboardMode m) { return static_cast<uint32_t>(m); }

	uint32_t GetEffectiveType() const
	{
		if (info_.kind == GpuParticleKind::Sprite)
		{
			return static_cast<uint32_t>(info_.spriteType);
		}
		if (info_.kind == GpuParticleKind::Ribbon)
		{
			return static_cast<uint32_t>(ToGpuParticleType(info_.ribbonType));
		}

		return static_cast<uint32_t>(info_.spriteType);
	}

	uint32_t GetPackedBillboardMode() const
	{
		const uint32_t flags = ToU32(info_.billboardFlags);
		return PackBillboardMode(info_.kind, flags);
	}

private: /// ---------- メンバ変数 ---------- ///

	std::string name_;
	EmitterInfo info_;
	Vector3 position_{ 0.0f, 0.0f, 0.0f };
	float loopTimer_ = 0.0f;
	uint32_t pendingBurstCount_ = 0;

	struct ActiveBatch
	{
		float remainingSec = 0.0f;
		uint32_t count = 0;
	};

	float EstimateParticleLifeTimeSec() const;
	void RegisterActiveBatch(uint32_t count);

	std::deque<ActiveBatch> activeBatches_;
	uint32_t estimatedActiveParticleCount_ = 0;
};


} // namespace Ken4lowEngine
