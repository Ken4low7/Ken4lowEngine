#include "SRVManager.h"

#include "DirectXCommon.h"

#include <algorithm>

namespace Ken4lowEngine
{

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


	/// -------------------------------------------------------------
	///				　	シングルトンインスタンス
	/// -------------------------------------------------------------
	SRVManager* SRVManager::GetInstance()
	{
		static SRVManager instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
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
		freeTransientRanges_.clear();
		freeTransientRanges_.push_back({ kTransientBeginIndex, kTransientSRVCount });
		pendingTransientRanges_.clear();
		descriptorStats_ = {};
		descriptorStats_.persistentCapacity = kTransientBeginIndex - 1;
		descriptorStats_.transientCapacity = kTransientSRVCount;
	}

	/// -------------------------------------------------------------
	///					　		終了処理
	/// -------------------------------------------------------------
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
		freeTransientRanges_.clear();
		pendingTransientRanges_.clear();
		descriptorStats_ = {};
		dxCommon_ = nullptr;
	}


	/// -------------------------------------------------------------
	///						　スプライト用のSRV生成
	/// -------------------------------------------------------------
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


	/// -------------------------------------------------------------
	///					ストラクチャバッファ用のSRV生成
	/// -------------------------------------------------------------
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


	/// -------------------------------------------------------------
	///						ヒープセットコマンド
	/// -------------------------------------------------------------
	void SRVManager::PreDraw()
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
		dxCommon_->GetCommandManager()->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
	}


	/// -------------------------------------------------------------
	///						SRVセットコマンド
	/// -------------------------------------------------------------
	void SRVManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
	{
		dxCommon_->GetCommandManager()->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
	}


	/// -------------------------------------------------------------
	///						深度バッファのSRVを作成
	/// -------------------------------------------------------------
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


	/// -------------------------------------------------------------
	///						Persistent Descriptor確保
	/// -------------------------------------------------------------
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


	/// -------------------------------------------------------------
	///						Persistent Descriptor解放
	/// -------------------------------------------------------------
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


	/// -------------------------------------------------------------
	///						Transient Descriptor確保
	/// -------------------------------------------------------------
	SRVManager::TransientDescriptorAllocation SRVManager::AllocateTransient(uint32_t count)
	{
		if (count == 0)
		{
			throw std::runtime_error("Transient descriptor allocation count must be greater than zero");
		}

		std::lock_guard<std::mutex> lock(allocationMutex);
		CollectTransientLocked();

		if (!dxCommon_ || !dxCommon_->GetFenceManager())
		{
			throw std::runtime_error("Transient descriptor allocation requires an initialized fence manager");
		}

		for (std::size_t rangeIndex = 0; rangeIndex < freeTransientRanges_.size(); ++rangeIndex)
		{
			DescriptorRange& freeRange = freeTransientRanges_[rangeIndex];
			if (freeRange.count < count)
			{
				continue;
			}

			const uint32_t firstIndex = freeRange.firstIndex;
			freeRange.firstIndex += count;
			freeRange.count -= count;
			if (freeRange.count == 0)
			{
				freeTransientRanges_.erase(freeTransientRanges_.begin() + static_cast<std::ptrdiff_t>(rangeIndex));
			}

			const UINT64 retireFenceValue = dxCommon_->GetFenceManager()->GetCurrentValue() + 1;
			pendingTransientRanges_.push_back({ { firstIndex, count }, retireFenceValue });
			descriptorStats_.transientInFlight += count;
			descriptorStats_.transientHighWater = (std::max)(descriptorStats_.transientHighWater, descriptorStats_.transientInFlight);
			descriptorStats_.pendingTransientRangeCount = static_cast<uint32_t>(pendingTransientRanges_.size());
			++descriptorStats_.transientAllocationCount;

			TransientDescriptorAllocation allocation{};
			allocation.firstIndex = firstIndex;
			allocation.count = count;
			allocation.cpuHandle = GetCPUDescriptorHandle(firstIndex);
			allocation.gpuHandle = GetGPUDescriptorHandle(firstIndex);
			allocation.retireFenceValue = retireFenceValue;
			return allocation;
		}

		++descriptorStats_.exhaustionCount;
		throw std::runtime_error("No contiguous transient SRV descriptor range is available");
	}

	void SRVManager::CollectTransient()
	{
		std::lock_guard<std::mutex> lock(allocationMutex);
		CollectTransientLocked();
	}

	void SRVManager::CollectTransientLocked()
	{
		if (!dxCommon_ || !dxCommon_->GetFenceManager())
		{
			return;
		}

		const UINT64 completedFenceValue = dxCommon_->GetFenceManager()->GetCompletedValue();
		for (auto it = pendingTransientRanges_.begin(); it != pendingTransientRanges_.end();)
		{
			if (completedFenceValue < it->retireFenceValue)
			{
				++it;
				continue;
			}

			const uint32_t reclaimedCount = it->range.count;
			InsertFreeTransientRangeLocked(it->range);
			if (descriptorStats_.transientInFlight >= reclaimedCount)
			{
				descriptorStats_.transientInFlight -= reclaimedCount;
			}
			descriptorStats_.transientReclaimedCount += reclaimedCount;
			it = pendingTransientRanges_.erase(it);
		}
		descriptorStats_.pendingTransientRangeCount = static_cast<uint32_t>(pendingTransientRanges_.size());
	}

	void SRVManager::InsertFreeTransientRangeLocked(DescriptorRange range)
	{
		if (range.count == 0)
		{
			return;
		}

		freeTransientRanges_.push_back(range);
		std::sort(
			freeTransientRanges_.begin(), freeTransientRanges_.end(),
			[](const DescriptorRange& left, const DescriptorRange& right)
			{
				return left.firstIndex < right.firstIndex;
			});

		std::vector<DescriptorRange> merged;
		merged.reserve(freeTransientRanges_.size());
		for (const DescriptorRange& candidate : freeTransientRanges_)
		{
			if (merged.empty())
			{
				merged.push_back(candidate);
				continue;
			}

			DescriptorRange& back = merged.back();
			const uint64_t backEnd = static_cast<uint64_t>(back.firstIndex) + back.count;
			const uint64_t candidateEnd = static_cast<uint64_t>(candidate.firstIndex) + candidate.count;
			if (candidate.firstIndex > backEnd)
			{
				merged.push_back(candidate);
				continue;
			}

			const uint64_t mergedEnd = (std::max)(backEnd, candidateEnd);
			back.count = static_cast<uint32_t>(mergedEnd - back.firstIndex);
		}
		freeTransientRanges_ = std::move(merged); // Fence完了後だけRangeを戻し、GPU使用中Descriptorの上書きを防ぐ。
	}

	SRVManager::DescriptorStats SRVManager::GetDescriptorStats() const
	{
		std::lock_guard<std::mutex> lock(allocationMutex);
		return descriptorStats_;
	}


	/// -------------------------------------------------------------
	///				デスクリプタヒープを生成する
	/// -------------------------------------------------------------
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


	/// -------------------------------------------------------------
	///				　CPUデスクリプタヒープを取得する
	/// -------------------------------------------------------------
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


	/// -------------------------------------------------------------
	///				　GPUデスクリプタヒープを取得する
	/// -------------------------------------------------------------
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
