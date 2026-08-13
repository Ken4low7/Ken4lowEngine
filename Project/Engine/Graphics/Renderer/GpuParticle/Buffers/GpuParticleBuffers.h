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

class Camera;

class GpuParticleBuffers
{
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

public:
	// Compaction CSへ1 Draw分のMaterial identityとIndirect geometry情報を渡す。
	struct GpuDrivenDrawCBData
	{
		uint32_t renderGroup = 0;
		uint32_t primitiveCount = 0;
		uint32_t indexed = 0;
		uint32_t maxParticles = 0;
	};
	static_assert(sizeof(GpuDrivenDrawCBData) == 16);

	void Initialize(Camera* camera);
	void Update(float deltaTime);

public:
	ID3D12Resource* GetParticleBuffer() const { return particleBuffer_.Get(); }
	ID3D12Resource* GetFreeCounterBuffer() const { return freeListIndexBuffer_.Get(); }
	ID3D12Resource* GetPerFrameBuffer();
	ID3D12Resource* GetEmitterBuffer();
	ID3D12Resource* GetVisibleParticleIndexBuffer() const { return visibleParticleIndexBuffer_.Get(); }
	ID3D12Resource* GetIndirectDrawArgsBuffer() const { return indirectDrawArgsBuffer_.Get(); }

	static uint32_t GetMaxParticles() { return kMaxParticles; }
	uint32_t GetParticleSrvIndex() const { return particleSrvIndex_; }
	uint32_t GetParticleUavIndex() const { return particleUavIndex_; }
	uint32_t GetFreeCounterUavIndex() const { return freeListIndexUavIndex_; }
	uint32_t GetVisibleParticleIndexSrvIndex() const { return visibleParticleIndexSrvIndex_; }
	uint32_t GetVisibleParticleIndexUavIndex() const { return visibleParticleIndexUavIndex_; }
	uint32_t GetIndirectDrawArgsUavIndex() const { return indirectDrawArgsUavIndex_; }
	ParticleCS* GetParticleData() const { return particleData_; }

	PerView* GetPerViewData() { return &perViewData_; }
	const PerView* GetPerViewData() const { return &perViewData_; }
	GpuEmitterCBData* GetEmitterCBData() { return &emitterData_[0]; }
	const GpuEmitterCBData* GetEmitterCBData() const { return &emitterData_[0]; }

	GpuEmitterCBData* GetEmitterCBData(uint32_t slot);
	D3D12_GPU_VIRTUAL_ADDRESS GetEmitterCBAddress(uint32_t slot);
	D3D12_GPU_VIRTUAL_ADDRESS GetPerViewCBAddress();
	D3D12_GPU_VIRTUAL_ADDRESS GetPerFrameCBAddress();
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuDrivenDrawCBAddress(uint32_t renderGroup, uint32_t primitiveCount, bool indexed);

	void SetDebugCameraEnabled(bool enabled) { isDebugCamera_ = enabled; }

private:
	void CreateParticleBuffer();
	void InitializePerViewData();
	void InitializeEmitterData();
	void InitializePerFrameData();
	void CreateFreeListIndexBuffer();
	void CreateFreeListBuffer();
	void CreateGpuDrivenDrawBuffers();

private:
	static const uint32_t kMaxParticles = 131072;
	static constexpr uint32_t kEmitterCBSlotCount = 256;
	static constexpr uint32_t kIndirectArgumentWordCount = 5;

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

	// Visible indexとIndirect argsはComputeで毎Draw再構築し、Graphics側は生存Particleだけを参照する。
	ComPtr<ID3D12Resource> visibleParticleIndexBuffer_;
	uint32_t visibleParticleIndexSrvIndex_ = 0;
	uint32_t visibleParticleIndexUavIndex_ = 0;
	ComPtr<ID3D12Resource> indirectDrawArgsBuffer_;
	uint32_t indirectDrawArgsUavIndex_ = 0;

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
