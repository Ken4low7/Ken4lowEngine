#pragma once
#include "DX12Include.h"
#include <memory>
#include <mutex>
#include <queue>
#include <cstdint>
#include <stdexcept>

namespace Ken4lowEngine
{

class DirectXCommon;

class UAVManager
{
public:
	static UAVManager* GetInstance();
	void Initialize(DirectXCommon* dxCommon);
	void Finalize();
	void PreDispatch();

	void CreateUAVForTexture2D(uint32_t uavIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	void CreateSRVForTexture2DOnThisHeap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	void CreateUAVForBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT64 bufferSize);
	void CreateUAVForStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
	void CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

public:
	uint32_t Allocate();
	void Free(uint32_t srvIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	/// ClearUnorderedAccessViewXxxはnon-shader-visible CPU descriptorを要求するため専用mirrorを返す。
	D3D12_CPU_DESCRIPTOR_HANDLE GetClearCPUDescriptorHandle(uint32_t index);

private:
	void MirrorUavDescriptorForClear(uint32_t index);

private:
	DirectXCommon* dxCommon_ = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc_{};
	ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	ComPtr<ID3D12DescriptorHeap> clearCpuDescriptorHeap_; // Clear用にshader-visible heapとは別のCPU-only mirrorを保持する。
	UINT descriptorSize_ = 0;
	static constexpr uint32_t kMaxUAVCount = 512;
	uint32_t useIndex_ = 0;
	std::mutex allocationMutex_;
	std::queue<uint32_t> freeIndices_;

private:
	UAVManager() = default;
	~UAVManager() = default;
	UAVManager(const UAVManager&) = delete;
	const UAVManager& operator=(const UAVManager&) = delete;
};

} // namespace Ken4lowEngine
