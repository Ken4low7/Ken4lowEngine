#pragma once
#include <DX12Include.h>
#include <Vector3.h>
#include <Vector4.h>
#include <Matrix4x4.h>

#include "BillboardMode.h"
#include "GpuParticleEmitterData.h"

#include <array>

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
		float padding[3]{}; // パディング
	};

	// 時間計測用
	struct PerFrame
	{
		float time = 0.0f; // 経過時間
		float deltaTime = 1.0f / 60.0f; // 前フレームからの経過時間
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// GPU パーティクル用バッファの初期化処理。
	/// </summary>
	void Initialize(Camera* camera);

	/// <summary>
	/// 毎フレームのカメラ・時間データをCPUステージングへ更新します。
	/// </summary>
	void Update(float deltaTime);

public: /// ---------- ゲッター ---------- ///

	ID3D12Resource* GetParticleBuffer() const { return particleBuffer_.Get(); }
	ID3D12Resource* GetFreeCounterBuffer() const { return freeListIndexBuffer_.Get(); }

	static uint32_t GetMaxParticles() { return kMaxParticles; }
	uint32_t GetParticleSrvIndex() const { return particleSrvIndex_; }
	uint32_t GetParticleUavIndex() const { return particleUavIndex_; }
	uint32_t GetFreeCounterUavIndex() const { return freeListIndexUavIndex_; }
	ParticleCS* GetParticleData() const { return particleData_; }

	PerView* GetPerViewData() { return &perViewData_; }
	const PerView* GetPerViewData() const { return &perViewData_; }
	GpuEmitterCBData* GetEmitterCBData() { return &emitterData_[0]; }
	const GpuEmitterCBData* GetEmitterCBData() const { return &emitterData_[0]; }

	GpuEmitterCBData* GetEmitterCBData(uint32_t slot);
	D3D12_GPU_VIRTUAL_ADDRESS GetEmitterCBAddress(uint32_t slot);
	D3D12_GPU_VIRTUAL_ADDRESS GetPerViewCBAddress();
	D3D12_GPU_VIRTUAL_ADDRESS GetPerFrameCBAddress();

	// 現在はCameraManagerの有効カメラを使用するため、互換用に状態だけ保持する
	void SetDebugCameraEnabled(bool enabled) { isDebugCamera_ = enabled; }

private: /// ---------- 内部メンバ関数 ---------- ///

	void CreateParticleBuffer();
	void InitializePerViewData();
	void InitializeEmitterData();
	void InitializePerFrameData();
	void CreateFreeListIndexBuffer();
	void CreateFreeListBuffer();

private: /// ---------- メンバ変数 ---------- ///

	static const uint32_t kMaxParticles = 131072;
	static constexpr uint32_t kEmitterCBSlotCount = 256;

	Camera* camera_ = nullptr;
	bool isDebugCamera_ = false;

	ComPtr<ID3D12Resource> particleBuffer_;
	ParticleCS* particleData_ = nullptr;
	uint32_t particleSrvIndex_ = 0;
	uint32_t particleUavIndex_ = 0;

	ComPtr<ID3D12Resource> freeListIndexBuffer_;
	uint32_t freeListIndexUavIndex_ = 0;

	ComPtr<ID3D12Resource> freeListBuffer_;
	uint32_t freeListUavIndex_ = 0;

	PerView perViewData_{};
	PerFrame perFrameData_{};
	std::array<GpuEmitterCBData, kEmitterCBSlotCount> emitterData_{};

	D3D12_GPU_VIRTUAL_ADDRESS perViewGpuAddress_ = 0;
	D3D12_GPU_VIRTUAL_ADDRESS perFrameGpuAddress_ = 0;
	uint32_t perViewUploadedFrameIndex_ = UINT32_MAX;
	uint32_t perFrameUploadedFrameIndex_ = UINT32_MAX;
	bool perViewDirty_ = true;
	bool perFrameDirty_ = true; // 動的CBは現在FrameのUpload Arenaへ転送し、GPU参照中の前Frame領域を上書きしない。

	Matrix4x4 worldViewProjectionMatrix;
	Matrix4x4 debugViewProjectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
};


} // namespace Ken4lowEngine
