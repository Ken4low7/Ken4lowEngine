#include "GpuSphParticleBuffer.h"
#include "../Interaction/GpuSphRigidbodyInteraction.h"

#include <DirectXCommon.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <UAVManager.h>

#include <algorithm>

namespace Ken4lowEngine
{

GpuSphParticleBuffer::~GpuSphParticleBuffer()
{
	Finalize();
}

bool GpuSphParticleBuffer::Initialize(uint32_t capacity)
{
	if (capacity == 0)
	{
		return false;
	}

	Finalize();

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	if (dxCommon == nullptr || dxCommon->GetDevice() == nullptr)
	{
		return false;
	}

	capacity_ = capacity;
	const uint64_t bufferBytes = static_cast<uint64_t>(sizeof(GpuSphParticle)) * capacity_;
	resource_ = ResourceManager::CreateBufferResource(
		dxCommon->GetDevice(),
		bufferBytes,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON);
	if (!resource_)
	{
		Finalize();
		return false;
	}

	resource_->SetName(L"GpuSph.ParticleBuffer");
	currentState_ = D3D12_RESOURCE_STATE_COMMON;

	try
	{
		SRVManager* srvManager = SRVManager::GetInstance();
		UAVManager* uavManager = UAVManager::GetInstance();

		srvIndex_ = srvManager->Allocate();
		srvManager->CreateSRVForStructureBuffer(
			srvIndex_, resource_.Get(), capacity_, sizeof(GpuSphParticle));

		// ComputeではSRVとUAVを同じshader-visible heapから同時に参照できるようにする。
		computeSrvIndex_ = uavManager->Allocate();
		uavManager->CreateSRVForStructureBuffer(
			computeSrvIndex_, resource_.Get(), capacity_, sizeof(GpuSphParticle));

		uavIndex_ = uavManager->Allocate();
		uavManager->CreateUAVForStructuredBuffer(
			uavIndex_, resource_.Get(), capacity_, sizeof(GpuSphParticle));
	}
	catch (...)
	{
		Finalize();
		return false;
	}

	activeParticleCount_ = 0;
	initialized_ = true;
	return true;
}

void GpuSphParticleBuffer::Finalize()
{
	// W9のFrame Resource/ReadbackをParticle Bufferより先に解放し、DX12終了時のLive Resourceを残さない。
	GpuSphRigidbodyInteraction::GetInstance()->Finalize();

	if (srvIndex_ != UINT32_MAX)
	{
		SRVManager::GetInstance()->Free(srvIndex_);
	}

	UAVManager* uavManager = UAVManager::GetInstance();
	if (computeSrvIndex_ != UINT32_MAX)
	{
		uavManager->Free(computeSrvIndex_);
	}
	if (uavIndex_ != UINT32_MAX)
	{
		uavManager->Free(uavIndex_);
	}

	resource_.Reset();
	srvIndex_ = UINT32_MAX;
	computeSrvIndex_ = UINT32_MAX;
	uavIndex_ = UINT32_MAX;
	currentState_ = D3D12_RESOURCE_STATE_COMMON;
	capacity_ = 0;
	activeParticleCount_ = 0;
	initialized_ = false;
}

void GpuSphParticleBuffer::SetActiveParticleCount(uint32_t activeCount)
{
	activeParticleCount_ = (std::min)(activeCount, capacity_);
}

uint64_t GpuSphParticleBuffer::GetApproximateGpuMemoryBytes() const
{
	return static_cast<uint64_t>(capacity_) * sizeof(GpuSphParticle);
}

GpuSphParticleBufferStats GpuSphParticleBuffer::GetStats() const
{
	GpuSphParticleBufferStats stats{};
	stats.capacity = capacity_;
	stats.activeCount = activeParticleCount_;
	stats.strideBytes = sizeof(GpuSphParticle);
	stats.approximateGpuMemoryBytes = GetApproximateGpuMemoryBytes();
	stats.initialized = initialized_;
	return stats;
}

void GpuSphParticleBuffer::Transition(
	ID3D12GraphicsCommandList* commandList,
	D3D12_RESOURCE_STATES newState)
{
	if (commandList == nullptr || !resource_ || currentState_ == newState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource_.Get();
	barrier.Transition.StateBefore = currentState_;
	barrier.Transition.StateAfter = newState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
	currentState_ = newState;
}

void GpuSphParticleBuffer::InsertUavBarrier(ID3D12GraphicsCommandList* commandList) const
{
	if (commandList == nullptr || !resource_)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = resource_.Get();
	commandList->ResourceBarrier(1, &barrier);
}

} // namespace Ken4lowEngine
