#include "SRVManager.h"

#include "DirectXCommon.h"

#include <algorithm>

namespace Ken4lowEngine
{

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

	SRVManager* SRVManager::GetInstance()
	{
		static SRVManager instance;
		return &instance;
	}

	void SRVManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = kMaxSRVCount;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;

		HRESULT result = dxCommon_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
		if (FAILED(result))
		{
			throw std::runtime_error("Failed to create SRV descriptor heap");
		}

		descriptorSize = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		descriptorHeap_->SetName(L"SRV Descriptor Heap");

		std::lock_guard<std::mutex> lock(allocationMutex);
		useIndex = 1;
		while (!freeIndices.empty())
		{
			freeIndices.pop();
		}
		persistentAllocated_.assign(kTransientBeginIndex, uint8_t{ 0 });
		transientFrameStates_.clear();

		const uint32_t frameCount = dxCommon_->GetCommandManager()
			? (std::max)(1u, dxCommon_->GetCommandManager()->GetFrameResourceCount())
			: 1u;
		if (frameCount > kTransientSRVCount)
		{
			throw std::runtime_error("Frame resource count exceeds transient SRV descriptor capacity");
		}

		transientFrameStates_.resize(frameCount);
		const uint32_t descriptorsPerFrame = kTransientSRVCount / frameCount;
		const uint32_t remainder = kTransientSRVCount % frameCount;
		uint32_t nextTransientIndex = kTransientBeginIndex;
		uint32_t minimumFrameCapacity = kTransientSRVCount;
		for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
		{
			FrameTransientState& state = transientFrameStates_[frameIndex];
			state.firstIndex = nextTransientIndex;
			state.capacity = descriptorsPerFrame + (frameIndex < remainder ? 1u : 0u);
			minimumFrameCapacity = (std::min)(minimumFrameCapacity, state.capacity);
			nextTransientIndex += state.capacity;
		}

		descriptorStats_ = {};
		descriptorStats_.persistentCapacity = kTransientBeginIndex - 1;
		descriptorStats_.transientCapacity = kTransientSRVCount;
		descriptorStats_.transientCapacityPerFrame = minimumFrameCapacity;
		descriptorStats_.currentFrameIndex = dxCommon_->GetCommandManager()
			? dxCommon_->GetCommandManager()->GetCurrentFrameIndex()
			: 0u;
	}

	void SRVManager::Finalize()
	{
		std::lock_guard<std::mutex> lock(allocationMutex);

		descriptorHeap_.Reset();
		descriptorSize = 0;
		useIndex = 1;
		while (!freeIndices.empty())
		{
			freeIndices.pop();
		}
		persistentAllocated_.clear();
		transientFrameStates_.clear();
		descriptorStats_ = {};
		dxCommon_ = nullptr;
	}

	void SRVManager::CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
	{
		if (!pResource)
		{
			throw std::runtime_error("pResource is null in CreateSRVForTexture2D");
		}
		if (srvIndex >= kMaxSRVCount)
		{
			throw std::runtime_error("srvIndex out of bounds in CreateSRVForTexture2D");
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = MipLevels;
		dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForTexture3D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
	{
		if (!pResource)
		{
			throw std::runtime_error("pResource is null in CreateSRVForTexture3D");
		}
		if (srvIndex >= kMaxSRVCount)
		{
			throw std::runtime_error("srvIndex out of bounds in CreateSRVForTexture3D");
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MostDetailedMip = 0;
		srvDesc.Texture3D.MipLevels = MipLevels;
		srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
		// Phase17のVolume Textureも既存Persistent SRV allocatorを共有し、描画側Descriptor寿命を2D Textureと揃える。
		dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
	{
		if (!pResource)
		{
			throw std::runtime_error("pResource is null in CreateSRVForStructureBuffer");
		}
		if (srvIndex >= kMaxSRVCount)
		{
			throw std::runtime_error("srvIndex out of bounds in CreateSRVForStructureBuffer");
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = numElements;
		srvDesc.Buffer.StructureByteStride = structureByteStride;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForShadowMap(uint32_t srvIndex, ID3D12Resource* shadowMap)
	{
		assert(shadowMap && "shadowMap resource is null in CreateSRVForShadowMap");
		if (srvIndex >= kMaxSRVCount)
		{
			throw std::runtime_error("srvIndex out of bounds in CreateSRVForShadowMap");
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		dxCommon_->GetDevice()->CreateShaderResourceView(shadowMap, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForShadowMapArray(uint32_t srvIndex, ID3D12Resource* shadowMapArray, uint32_t arraySize)
	{
		assert(shadowMapArray && "ShadowMap array resource is null");
		if (srvIndex >= kMaxSRVCount)
		{
			throw std::runtime_error("srvIndex out of bounds in CreateSRVForShadowMapArray");
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = arraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		dxCommon_->GetDevice()->CreateShaderResourceView(shadowMapArray, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForShadowCube(uint32_t srvIndex, ID3D12Resource* shadowCube)
	{
		assert(shadowCube && "Shadow cube resource is null");
		if (srvIndex >= kMaxSRVCount)
		{
			throw std::runtime_error("srvIndex out of bounds in CreateSRVForShadowCube");
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		dxCommon_->GetDevice()->CreateShaderResourceView(shadowCube, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::PreDraw()
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
		dxCommon_->GetCommandManager()->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
	}

	void SRVManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
	{
		dxCommon_->GetCommandManager()->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForDepthBuffer(uint32_t srvIndex, ID3D12Resource* depthBuffer)
	{
		if (srvIndex >= kMaxSRVCount)
		{
			throw std::runtime_error("srvIndex out of bounds in CreateSRVForDepthBuffer");
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = GetCPUDescriptorHandle(srvIndex);
		dxCommon_->GetDevice()->CreateShaderResourceView(depthBuffer, &srvDesc, srvHandle);
	}

	uint32_t SRVManager::Allocate()
	{
		std::lock_guard<std::mutex> lock(allocationMutex);

		uint32_t index = UINT32_MAX;
		if (!freeIndices.empty())
		{
			index = freeIndices.front();
			freeIndices.pop();
		}
		else
		{
			if (useIndex >= kTransientBeginIndex)
			{
				++descriptorStats_.exhaustionCount;
				throw std::runtime_error("No more persistent SRV descriptors can be allocated");
			}
			index = useIndex++;
		}

		if (index == 0 || index >= persistentAllocated_.size() || persistentAllocated_[index] != uint8_t{ 0 })
		{
			throw std::runtime_error("Persistent SRV allocator state is corrupted");
		}

		persistentAllocated_[index] = uint8_t{ 1 };
		++descriptorStats_.persistentInUse;
		descriptorStats_.persistentHighWater = (std::max)(descriptorStats_.persistentHighWater, descriptorStats_.persistentInUse);
		return index;
	}

	void SRVManager::Free(uint32_t srvIndex)
	{
		std::lock_guard<std::mutex> lock(allocationMutex);

		if (srvIndex == 0 || srvIndex >= kTransientBeginIndex || srvIndex >= persistentAllocated_.size())
		{
			throw std::runtime_error("Invalid persistent SRV index for freeing");
		}
		if (persistentAllocated_[srvIndex] == uint8_t{ 0 })
		{
			throw std::runtime_error("Persistent SRV descriptor was freed twice");
		}

		persistentAllocated_[srvIndex] = uint8_t{ 0 };
		freeIndices.push(srvIndex);
		if (descriptorStats_.persistentInUse > 0)
		{
			--descriptorStats_.persistentInUse;
		}
	}

	SRVManager::TransientDescriptorAllocation SRVManager::AllocateTransient(uint32_t count)
	{
		if (count == 0)
		{
			throw std::runtime_error("Transient descriptor allocation count must be greater than zero");
		}
		if (!dxCommon_ || !dxCommon_->GetCommandManager())
		{
			throw std::runtime_error("Transient descriptor allocation requires an initialized command manager");
		}
		if (dxCommon_->GetCommandManager()->IsCommandListSubmitted())
		{
			throw std::runtime_error("Transient descriptors cannot be allocated while the command list is submitted");
		}

		std::lock_guard<std::mutex> lock(allocationMutex);
		const uint32_t frameIndex = dxCommon_->GetCommandManager()->GetCurrentFrameIndex();
		RefreshTransientFrameLocked(frameIndex);

		FrameTransientState& state = transientFrameStates_[frameIndex];
		if (count > state.capacity - state.cursor)
		{
			++descriptorStats_.exhaustionCount;
			throw std::runtime_error("No contiguous transient SRV descriptor range is available for the current frame");
		}

		const uint32_t firstIndex = state.firstIndex + state.cursor;
		state.cursor += count;
		state.highWater = (std::max)(state.highWater, state.cursor);

		descriptorStats_.currentFrameIndex = frameIndex;
		descriptorStats_.transientInUse += count;
		descriptorStats_.transientHighWater = (std::max)(descriptorStats_.transientHighWater, descriptorStats_.transientInUse);
		++descriptorStats_.transientAllocationCount;

		TransientDescriptorAllocation allocation{};
		allocation.firstIndex = firstIndex;
		allocation.count = count;
		allocation.frameIndex = frameIndex;
		allocation.cpuHandle = GetCPUDescriptorHandle(firstIndex);
		allocation.gpuHandle = GetGPUDescriptorHandle(firstIndex);
		return allocation;
	}

	void SRVManager::RefreshTransientFrameLocked(uint32_t frameIndex)
	{
		if (!dxCommon_ || !dxCommon_->GetCommandManager() || frameIndex >= transientFrameStates_.size())
		{
			throw std::runtime_error("Invalid frame index for transient descriptor allocation");
		}

		FrameTransientState& state = transientFrameStates_[frameIndex];
		const UINT64 frameFenceValue = dxCommon_->GetCommandManager()->GetFrameFenceValue(frameIndex);
		if (state.initialized && state.observedFrameFenceValue == frameFenceValue)
		{
			return;
		}

		if (state.initialized && state.cursor > 0)
		{
			const uint32_t reclaimedCount = state.cursor;
			descriptorStats_.transientInUse = descriptorStats_.transientInUse >= reclaimedCount
				? descriptorStats_.transientInUse - reclaimedCount
				: 0u;
			descriptorStats_.transientReclaimedCount += reclaimedCount;
			++descriptorStats_.transientFrameRecycleCount;
		}

		state.cursor = 0;
		state.observedFrameFenceValue = frameFenceValue;
		state.initialized = true; // Frame fence世代が変わった時だけArenaを戻し、GPU使用中Descriptorの上書きを防ぐ。
	}

	SRVManager::DescriptorStats SRVManager::GetDescriptorStats() const
	{
		std::lock_guard<std::mutex> lock(allocationMutex);
		return descriptorStats_;
	}

	ComPtr<ID3D12DescriptorHeap> SRVManager::CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible)
	{
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
		descriptorHeapDesc.Type = heapType;
		descriptorHeapDesc.NumDescriptors = numDescriptors;
		descriptorHeapDesc.Flags = shadervisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
		assert(SUCCEEDED(hr));
		return descriptorHeap;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE SRVManager::GetCPUDescriptorHandle(uint32_t index)
	{
		if (!descriptorHeap_ || index >= kMaxSRVCount)
		{
			throw std::runtime_error("Invalid SRV CPU descriptor handle request");
		}
		D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
		return handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE SRVManager::GetGPUDescriptorHandle(uint32_t index)
	{
		if (!descriptorHeap_ || index >= kMaxSRVCount)
		{
			throw std::runtime_error("Invalid SRV GPU descriptor handle request");
		}
		D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<UINT64>(descriptorSize) * index;
		return handle;
	}

} // namespace Ken4lowEngine
