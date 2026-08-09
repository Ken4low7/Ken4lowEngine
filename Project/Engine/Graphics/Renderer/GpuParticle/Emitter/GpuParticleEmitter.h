#pragma once
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

		// ImGuiで編集したEmitterDescを実際のGPU生成値へ渡すPreview用上書き設定。
		// useDescSpawnOverride=falseの既存Emitterは従来のType別Shader設定をそのまま使用する。
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
		uint32_t spawnShape = 0; // GpuParticleSpawnShapeと同じPoint/Sphere/Boxの値。
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
	/// 定期発射（ループ）とバースト発生をまとめて処理し、<br/>
	/// GPU に渡すエミッター用 CB データを構築します。<br/>
	/// ・loopFrequency / loopCount に基づいてループ発生数を計算し pendingBurstCount_ に加算<br/>
	/// ・pendingBurstCount_ が 0 なら何も書き込まず false を返す<br/>
	/// ・1 フレーム分の発生パラメータを GpuEmitterCBData に書き込み、pendingBurstCount_ を消費<br/>
	/// という流れで動作します。<br/>
	/// 戻り値 true のときだけ DispatchEmit() 側で Emit 用 Compute Shader を実行します。
	/// </summary>
	/// <param name="out">GPU に送るエミッター用 CB データの書き込み先</param>
	/// <param name="deltaTime">前フレームからの経過時間（秒）</param>
	/// <returns>今フレーム何かしらパーティクルを発生させる場合は true、それ以外は false</returns>
	bool BuildCB(GpuEmitterCBData& out, float deltaTime);

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// エミッターのワールド座標を設定します。<br/>
	/// GPU 側ではこの位置と radius をもとに、発生位置のランダムサンプリングなどを行います。
	/// </summary>
	/// <param name="position">新しいエミッター位置</param>
	void SetPosition(const Vector3& position) { position_ = position; }

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// エミッター名を取得します。
	/// </summary>
	/// <returns>コンストラクタで指定したエミッター名</returns>
	const std::string& GetName() const { return name_; }

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	const EmitterInfo& GetInfo() const { return info_; }

	// 描画に使うID（0なら type を返す）
	uint32_t GetDrawType() const
	{
		const uint32_t effectiveType = GetEffectiveType();
		return (info_.drawType != 0) ? info_.drawType : effectiveType;
	}

	// ImGui編集用
	EmitterInfo& GetInfoMutable() { return info_; }

	// 位置もUIで見たい
	const Vector3 GetPosition() const { return position_; }

	/// GPU readback を避けるため、発生要求数と寿命から推定した生存数を返す。
	uint32_t GetEstimatedActiveParticleCount() const { return estimatedActiveParticleCount_; }
	bool HasActiveParticles() const { return estimatedActiveParticleCount_ > 0 || pendingBurstCount_ > 0; }

private: /// ---------- プライベート関数 ---------- ///

	static constexpr uint32_t ToU32(BillboardMode m) { return static_cast<uint32_t>(m); }

	/// kind に応じて GPUへ渡す type を決める
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

		// Mesh/Beam未実装時の保険：とりあえずSpriteTypeを流す（必要ならここを拡張）
		return static_cast<uint32_t>(info_.spriteType);
	}

	/// kind + flags をパックした billboardMode を返す
	uint32_t GetPackedBillboardMode() const
	{
		const uint32_t flags = ToU32(info_.billboardFlags);
		return PackBillboardMode(info_.kind, flags);
	}

private: /// ---------- メンバ変数 ---------- ///

	std::string name_; // エミッター名
	EmitterInfo info_; // エミッター情報

	Vector3 position_{ 0.0f, 0.0f, 0.0f };

	// ループ用タイマー
	float loopTimer_ = 0.0f;

	// このフレームに放出予定の累積数
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
