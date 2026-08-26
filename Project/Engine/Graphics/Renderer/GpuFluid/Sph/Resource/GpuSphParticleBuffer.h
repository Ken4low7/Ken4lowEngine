#pragma once

#include "../Data/GpuSphParticleTypes.h"

#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

/// SPH particle StructuredBufferとSRV/UAV descriptorを所有するW5.1基盤。
class GpuSphParticleBuffer
{
public:
	GpuSphParticleBuffer() = default;
	~GpuSphParticleBuffer();

	GpuSphParticleBuffer(const GpuSphParticleBuffer&) = delete;
	GpuSphParticleBuffer& operator=(const GpuSphParticleBuffer&) = delete;

	bool Initialize(uint32_t capacity);
	void Finalize();

	void SetActiveParticleCount(uint32_t activeCount);

	[[nodiscard]] bool IsInitialized() const { return initialized_; }
	[[nodiscard]] uint32_t GetCapacity() const { return capacity_; }
	[[nodiscard]] uint32_t GetActiveParticleCount() const { return activeParticleCount_; }
	[[nodiscard]] uint32_t GetStrideBytes() const { return sizeof(GpuSphParticle); }
	[[nodiscard]] uint64_t GetApproximateGpuMemoryBytes() const;
	[[nodiscard]] GpuSphParticleBufferStats GetStats() const;

	[[nodiscard]] ID3D12Resource* GetResource() const { return resource_.Get(); }
	[[nodiscard]] uint32_t GetSrvIndex() const { return srvIndex_; }
	[[nodiscard]] uint32_t GetComputeSrvIndex() const { return computeSrvIndex_; }
	[[nodiscard]] uint32_t GetUavIndex() const { return uavIndex_; }
	[[nodiscard]] D3D12_RESOURCE_STATES GetCurrentState() const { return currentState_; }

	void Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES newState);
	void InsertUavBarrier(ID3D12GraphicsCommandList* commandList) const;

private:
	ComPtr<ID3D12Resource> resource_{};
	uint32_t srvIndex_ = UINT32_MAX;
	uint32_t computeSrvIndex_ = UINT32_MAX;
	uint32_t uavIndex_ = UINT32_MAX;
	D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;
	uint32_t capacity_ = 0;
	uint32_t activeParticleCount_ = 0;
	bool initialized_ = false;
};

} // namespace Ken4lowEngine
