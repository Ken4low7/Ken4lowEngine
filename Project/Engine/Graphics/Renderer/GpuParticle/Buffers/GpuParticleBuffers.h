#pragma once
#include <DX12Include.h>
#include <Vector3.h>
#include <Vector4.h>
#include <Matrix4x4.h>

#include "BillboardMode.h"
#include "GpuParticleEmitterData.h"

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class Camera;

/// -------------------------------------------------------------
///			　GPUパーティクルバッファクラス
/// -------------------------------------------------------------
class GpuParticleBuffers
{
	// パーティクルの構造体
	struct ParticleCS
	{
		Vector3 translate;
		float pad0 = 0.0f;

		Vector3 scale;
		float lifeTime = 0.0f;

		Vector3 velocity;
		float currentTime = 0.0f;

		uint32_t type = 0;
		uint32_t billboardMode = 0;
		uint32_t atlasCols = 1;
		uint32_t atlasRows = 1;

		uint32_t animFrameCount = 1;
		float animFps = 0.0f;
		uint32_t animFlags = 0;
		uint32_t startFrame = 0;

		float animSpeed = 1.0f;
		float pad1[3] = {};

		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		Vector3 startScale{};
		uint32_t customFlags = 0;
		Vector3 endScale{};
		float customPadding = 0.0f;
		Vector4 startColor{};
		Vector4 endColor{};
		Vector3 gravity{};
		float damping = 0.0f;
		float rotation = 0.0f;
		float rotationSpeed = 0.0f;
		float customPadding2[2]{};
	};
	static_assert(sizeof(ParticleCS) == 208); // HLSL Particle構造体とのstride一致を保証する。

	// ビュー行列と射影行列
	struct PerView
	{
		Matrix4x4 viewProjectionMatrix{}; // ビュー射影行列
		Matrix4x4 billboardMatrix{}; // ビルボード用行列
		uint32_t billboardMode{}; // ビルボードモード
		float padding[3]; // パディング
	};

	// 時間計測用
	struct PerFrame
	{
		float time; // 経過時間
		float deltaTime; // 前フレームからの経過時間
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// GPU パーティクル用バッファの初期化処理。<br/>
	/// ・カメラポインタの保持<br/>
	/// ・パーティクル Structured Buffer の生成（SRV/UAV 作成）<br/>
	/// ・PerView / PerFrame 定数バッファの生成とマップ<br/>
	/// ・エミッター CB の生成とマップ<br/>
	/// ・フリーカウンター / フリーリスト用バッファの生成と UAV 作成<br/>
	/// を行います。
	/// </summary>
	/// <param name="camera">ビュー／プロジェクション行列取得に使用するカメラ。</param>
	void Initialize(Camera* camera);

	/// <summary>
	/// 毎フレームの更新処理。<br/>
	/// ・PerFrame の deltaTime / time を更新<br/>
	/// ・現在の描画カメラから View / Projection を取得して ViewProjection を計算<br/>
	/// ・ビルボード用行列（回転のみ）を現在の描画カメラから計算して書き込み<br/>
	/// といった処理を行います。
	/// </summary>
	/// <param name="deltaTime">前フレームからの経過時間（秒）。</param>
	void Update(float deltaTime);

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// パーティクル Structured Buffer リソースを取得します。<br/>
	/// Compute / Draw 両方から SRV/UAV 経由で参照されます。
	/// </summary>
	ID3D12Resource* GetParticleBuffer() const { return particleBuffer_.Get(); }

	/// <summary>
	/// PerView 定数バッファを取得します。<br/>
	/// 描画時に b0 などとしてバインドします。
	/// </summary>
	ID3D12Resource* GetPerViewBuffer() const { return perViewBuffer_.Get(); }

	/// <summary>
	/// エミッター用定数バッファを取得します。<br/>
	/// Emit 用 Compute シェーダに b1 などとして渡します。
	/// </summary>
	ID3D12Resource* GetEmitterBuffer() const { return emitterBuffer_.Get(); }

	/// <summary>
	/// 時間計測用（PerFrame）定数バッファを取得します。<br/>
	/// シミュレーションや Emit 用の Compute シェーダに b2 などとして渡します。
	/// </summary>
	ID3D12Resource* GetPerFrameBuffer() const { return perFrameBuffer_.Get(); }

	/// <summary>
	/// フリーカウンター（空きインデックス管理用）のバッファを取得します。<br/>
	/// Compute シェーダ側で Append/Consume 的な挙動を実装するために使用します。
	/// </summary>
	ID3D12Resource* GetFreeCounterBuffer() const { return freeListIndexBuffer_.Get(); }

	/// <summary>
	/// 管理する最大パーティクル数を取得します。
	/// </summary>
	static uint32_t GetMaxParticles() { return kMaxParticles; }

	/// <summary>
	/// パーティクル Structured Buffer の SRV インデックスを取得します。
	/// </summary>
	uint32_t GetParticleSrvIndex() const { return particleSrvIndex_; }

	/// <summary>
	/// パーティクル Structured Buffer の UAV インデックスを取得します。
	/// </summary>
	uint32_t GetParticleUavIndex() const { return particleUavIndex_; }

	/// <summary>
	/// フリーカウンターバッファの UAV インデックスを取得します。
	/// </summary>
	uint32_t GetFreeCounterUavIndex() const { return freeListIndexUavIndex_; }

	/// <summary>
	/// パーティクルバッファの CPU 側マッピングポインタを取得します。<br/>
	/// ※ 現状はマップしていないため nullptr のままです。<br/>
	/// 　将来 CPU から直接書き込みたい場合に使用します。
	/// </summary>
	ParticleCS* GetParticleData() const { return particleData_; }

	/// <summary>
	/// PerView 定数バッファの CPU 側マッピングポインタを取得します。<br/>
	/// 必要に応じてカメラ情報を直接書き換えることができます。
	/// </summary>
	PerView* GetPerViewData() const { return perViewData_; }

	/// <summary>
	/// エミッター用 CB データのマッピングポインタを取得します。<br/>
	/// GPU Emit 用のパラメータ（GpuEmitterCBData）を直接編集する際に使用します。
	/// </summary>
	GpuEmitterCBData* GetEmitterCBData() const { return emitterCBData_; }

	// 256byteアライン
	static inline constexpr UINT Align256(UINT size) { return (size + 255) & ~255u; }
	GpuEmitterCBData* GetEmitterCBData(uint32_t slot);
	D3D12_GPU_VIRTUAL_ADDRESS GetEmitterCBAddress(uint32_t slot);

	// 現在はCameraManagerの有効カメラを使用するため、互換用に状態だけ保持する
	void SetDebugCameraEnabled(bool enabled) { isDebugCamera_ = enabled; }

private: /// ---------- 内部メンバ関数 ---------- ///

	/// <summary>
	/// パーティクル Structured Buffer を生成します。<br/>
	/// ・kMaxParticles 分の ParticleCS を格納できる DEFAULT ヒープのリソースを作成<br/>
	/// ・SRVManager で SRV を作成<br/>
	/// ・UAVManager で UAV を作成<br/>
	/// を行います。
	/// </summary>
	void CreateParticleBuffer();

	/// <summary>
	/// PerView 定数バッファを生成し、CPU 側にマップします。<br/>
	/// 初期値として単位行列と Camera ビルボードモードを設定します。
	/// </summary>
	void CreatePerViewBuffer();

	/// <summary>
	/// エミッター用定数バッファを生成し、CPU 側にマップします。<br/>
	/// 初期値としてカウント・頻度・半径・位置・タイプなどを設定します。
	/// </summary>
	void CreateEmitterBuffer();

	/// <summary>
	/// PerFrame 定数バッファを生成し、CPU 側にマップします。<br/>
	/// 初期値として time=0, deltaTime=1/60 を設定します。
	/// </summary>
	void CreatePerFrameBuffer();

	/// <summary>
	/// フリーカウンター用バッファを生成し、UAV を作成します。<br/>
	/// 空きパーティクルのインデックス管理に使用します。
	/// </summary>
	void CreateFreeListIndexBuffer();

	/// <summary>
	/// フリーリスト用バッファを生成し、UAV を作成します。<br/>
	/// 各パーティクルのインデックスを保持し、Allocate/Free を GPU 上で行うために使用します。
	/// </summary>
	void CreateFreeListBuffer();

private: /// ---------- メンバ変数 ---------- ///

	// 最大パーティクル数
	static const uint32_t kMaxParticles = 131072;

	Camera* camera_ = nullptr; // カメラのポインタ
	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ

	// パーティクルバッファ
	ComPtr<ID3D12Resource> particleBuffer_;
	ParticleCS* particleData_ = nullptr; // マッピング用ポインタ
	uint32_t particleSrvIndex_ = 0;	 // SRVのインデックス
	uint32_t particleUavIndex_ = 0;	 // UAVのインデックス

	// フリーリストインデックスバッファ
	ComPtr<ID3D12Resource> freeListIndexBuffer_;
	uint32_t freeListIndexUavIndex_ = 0;

	// フリーリスト
	ComPtr<ID3D12Resource> freeListBuffer_;
	uint32_t freeListUavIndex_ = 0;

	// ビュー行列と射影行列バッファ
	ComPtr<ID3D12Resource> perViewBuffer_;
	PerView* perViewData_ = nullptr; // マッピング用ポインタ

	// エミッターバッファ
	ComPtr<ID3D12Resource> emitterBuffer_;
	GpuEmitterCBData* emitterCBData_ = nullptr; // マッピング用ポインタ

	// 時間計測用バッファ
	ComPtr<ID3D12Resource> perFrameBuffer_;
	PerFrame* perFrameData_ = nullptr; // マッピング用ポインタ

	// 行列関連
	Matrix4x4 worldViewProjectionMatrix;  // ワールドビュー射影行列
	Matrix4x4 debugViewProjectionMatrix_; // デバッグカメラのビュー射影行列
	Matrix4x4 viewProjectionMatrix_;	  // ビュー射影行列
};


} // namespace Ken4lowEngine
