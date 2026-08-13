#pragma once
#include <DX12Include.h>
#include <Vector3.h>
#include <Vector4.h>
#include <Matrix4x4.h>
#include <Engine/Graphics/Device/Buffer/PerFrameUploadBuffer.h>

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
	// HLSL Particleと同一strideを持つCPU側mirror。GPU readback用途ではなくResource stride定義に使用する。
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

		Vector4 sizeCurveLut{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 colorGradientLut0{};
		Vector4 colorGradientLut1{};
		Vector4 colorGradientLut2{};
		Vector4 colorGradientLut3{};
		float noiseStrength = 0.0f;
		float noiseFrequency = 1.0f;
		float vortexStrength = 0.0f;
		float attractorStrength = 0.0f;
		Vector3 vortexAxis{ 0.0f, 1.0f, 0.0f };
		float attractorRadius = 0.0f;
		Vector3 attractorPosition{};
		float forcePadding = 0.0f;
		Vector3 forceOrigin{};
		float forceOriginPadding = 0.0f;
		Vector3 rotation3D{};
		float rotation3DPadding = 0.0f;
		Vector3 angularVelocity3D{};
		float angularVelocity3DPadding = 0.0f;
	};
	static_assert(sizeof(ParticleCS) == 384); // HLSL Particle構造体とのstride一致を保証する。

	struct PerView
	{
		Matrix4x4 viewProjectionMatrix{};
		Matrix4x4 billboardMatrix{};
		uint32_t billboardMode{};
		float padding[3]{};
	};

	struct PerFrame
	{
		float time = 0.0f;
		float deltaTime = 1.0f / 60.0f;
	};

public: /// ---------- メンバ関数 ---------- ///

	void Initialize(Camera* camera);
	void Update(float deltaTime);

public: /// ---------- ゲッター ---------- ///

	ID3D12Resource* GetParticleBuffer() const { return particleBuffer_.Get(); }
	ID3D12Resource* GetFreeCounterBuffer() const { return freeListIndexBuffer_.Get(); }
	ID3D12Resource* GetPerFrameBuffer();
	ID3D12Resource* GetEmitterBuffer();

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

	PerFrameUploadBuffer<PerFrame> perFrameBuffers_;
	PerFrameUploadBuffer<GpuEmitterCBData> emitterBootstrapBuffers_; // 旧初期化Dispatch用CBもFrame別に保持して同期契約を崩さない。

	D3D12_GPU_VIRTUAL_ADDRESS perViewGpuAddress_ = 0;
	uint32_t perViewUploadedFrameIndex_ = UINT32_MAX;
	bool perViewDirty_ = true;

	Matrix4x4 worldViewProjectionMatrix;
	Matrix4x4 debugViewProjectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
};


} // namespace Ken4lowEngine
