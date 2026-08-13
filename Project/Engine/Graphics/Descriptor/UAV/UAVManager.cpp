#include "UAVManager.h"
#include "DirectXCommon.h"

namespace Ken4lowEngine
{

UAVManager* UAVManager::GetInstance()
{
	static UAVManager instance;
	return &instance;
}

void UAVManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	descriptorHeapDesc_.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc_.NumDescriptors = kMaxUAVCount;
	descriptorHeapDesc_.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc_.NodeMask = 0;
	HRESULT result = dxCommon_->GetDevice()->CreateDescriptorHeap(&descriptorHeapDesc_, IID_PPV_ARGS(&descriptorHeap_));
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create UAV descriptor heap");
	}

	D3D12_DESCRIPTOR_HEAP_DESC clearHeapDesc = descriptorHeapDesc_;
	clearHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = dxCommon_->GetDevice()->CreateDescriptorHeap(&clearHeapDesc, IID_PPV_ARGS(&clearCpuDescriptorHeap_));
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create CPU-only UAV clear descriptor heap");
	}

	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorHeap_->SetName(L"UAV Descriptor Heap");
	clearCpuDescriptorHeap_->SetName(L"UAV Clear CPU Descriptor Heap");
}

void UAVManager::Finalize()
{
	std::lock_guard<std::mutex> lock(allocationMutex_);
	descriptorHeap_.Reset();
	clearCpuDescriptorHeap_.Reset();
	descriptorSize_ = 0;
	useIndex_ = 0;
	while (!freeIndices_.empty()) { freeIndices_.pop(); }
	dxCommon_ = nullptr;
}

void UAVManager::PreDispatch()
{
	ID3D12DescriptorHeap* heaps[] = { descriptorHeap_.Get() };
	dxCommon_->GetCommandManager()->GetCommandList()->SetDescriptorHeaps(_countof(heaps), heaps);
}

void UAVManager::CreateUAVForTexture2D(uint32_t uavIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	if (!pResource) throw std::runtime_error("pResource is null in CreateUAVForTexture2D");
	if (uavIndex >= kMaxUAVCount) throw std::runtime_error("uavIndex out of bounds in CreateUAVForTexture2D");

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = Format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = MipLevels;

	// Clear用descriptorはshader-visible heapからコピーせず、同一UAVをCPU-only heapへ直接生成する。
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(uavIndex));
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetClearCPUDescriptorHandle(uavIndex));
}

void UAVManager::CreateSRVForTexture2DOnThisHeap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	if (!pResource) throw std::runtime_error("pResource is null in CreateSRVForTexture2DOnThisHeap");
	D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
	srv.Format = Format;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv.Texture2D.MipLevels = MipLevels;
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srv, GetCPUDescriptorHandle(srvIndex));
}

void UAVManager::CreateUAVForBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT64 bufferSize)
{
	if (!pResource) throw std::runtime_error("pResource is null in CreateUAVForBuffer");
	if (uavIndex >= kMaxUAVCount) throw std::runtime_error("uavIndex out of bounds in CreateUAVForBuffer");

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = static_cast<UINT>(bufferSize / 4);
	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(uavIndex));
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetClearCPUDescriptorHandle(uavIndex));
}

void UAVManager::CreateUAVForStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	if (!pResource) throw std::runtime_error("pResource is null in CreateUAVForStructuredBuffer");
	if (uavIndex >= kMaxUAVCount) throw std::runtime_error("uavIndex out of bounds in CreateUAVForStructuredBuffer");

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	uavDesc.Buffer.StructureByteStride = structureByteStride;
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(uavIndex));
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetClearCPUDescriptorHandle(uavIndex));
}

uint32_t UAVManager::Allocate()
{
	std::lock_guard<std::mutex> lock(allocationMutex_);
	if (!freeIndices_.empty())
	{
		uint32_t index = freeIndices_.front();
		freeIndices_.pop();
		return index;
	}
	if (useIndex_ >= kMaxUAVCount)
	{
		throw std::runtime_error("No more UAV descriptors can be allocated");
	}
	return useIndex_++;
}

void UAVManager::Free(uint32_t uavIndex)
{
	std::lock_guard<std::mutex> lock(allocationMutex_);
	if (uavIndex >= kMaxUAVCount) throw std::runtime_error("Invalid UAV index for freeing");
	freeIndices_.push(uavIndex);
}

D3D12_CPU_DESCRIPTOR_HANDLE UAVManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<unsigned long long>(index) * descriptorSize_;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE UAVManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<unsigned long long>(index) * descriptorSize_;
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE UAVManager::GetClearCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = clearCpuDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<unsigned long long>(index) * descriptorSize_;
	return handle;
}

void UAVManager::CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	if (!pResource) throw std::runtime_error("pResource is null in CreateSRVForStructureBuffer");
	D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
	srv.Format = DXGI_FORMAT_UNKNOWN;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srv.Buffer.FirstElement = 0;
	srv.Buffer.NumElements = numElements;
	srv.Buffer.StructureByteStride = structureByteStride;
	srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srv, GetCPUDescriptorHandle(srvIndex));
}

} // namespace Ken4lowEngine
