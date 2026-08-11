#pragma once

#include "RenderGraphTransientPool.h"

#include <d3d12.h>
#include <wrl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// RenderGraphTransientPoolの物理SlotをD3D12 HeapへMaterializeし、同じoffset 0へPlaced Resourceを生成するBackend。
	/// </summary>
	class RenderGraphD3D12TransientPool
	{
	public:
		struct HeapSlot
		{
			uint32_t slotIndex = 0;
			UINT64 capacityBytes = 0;
			UINT64 heapAlignmentBytes = 0;
			Microsoft::WRL::ComPtr<ID3D12Heap> heap;
		};

		void Reset()
		{
			heapSlots_.clear();
			physicalHeapBytes_ = 0;
		}

		static bool DescribeResource(
			ID3D12Device* device,
			const D3D12_RESOURCE_DESC& resourceDesc,
			uint64_t compatibilityKey,
			RenderGraphTransientPool::ResourceDesc& outDesc,
			bool allowAliasing = true)
		{
			if (!device)
			{
				return false;
			}

			const D3D12_RESOURCE_ALLOCATION_INFO info = device->GetResourceAllocationInfo(0, 1, &resourceDesc);
			if (info.SizeInBytes == 0 || info.SizeInBytes == UINT64_MAX || info.Alignment == 0)
			{
				return false;
			}
			if (info.SizeInBytes > static_cast<UINT64>((std::numeric_limits<std::size_t>::max)()) ||
				info.Alignment > static_cast<UINT64>((std::numeric_limits<std::size_t>::max)()))
			{
				return false;
			}

			outDesc.allocationSizeBytes = static_cast<std::size_t>(info.SizeInBytes);
			outDesc.alignmentBytes = static_cast<std::size_t>(info.Alignment);
			outDesc.compatibilityKey = compatibilityKey;
			outDesc.allowAliasing = allowAliasing;
			return true;
		}

		bool BuildHeaps(
			ID3D12Device* device,
			const RenderGraphTransientPool& plan,
			D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
			D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
			std::string* outError = nullptr)
		{
			if (outError) outError->clear();
			Reset();
			if (!device)
			{
				if (outError) *outError = "D3D12 Deviceがありません。";
				return false;
			}

			heapSlots_.reserve(plan.GetSlots().size());
			for (const RenderGraphTransientPool::SlotRecord& slot : plan.GetSlots())
			{
				const UINT64 resourceAlignment = static_cast<UINT64>(slot.alignmentBytes);
				const UINT64 heapAlignment = resourceAlignment > D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
					? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
					: D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
				UINT64 heapSize = 0;
				if (!TryAlignUp(static_cast<UINT64>(slot.capacityBytes), heapAlignment, heapSize))
				{
					if (outError) *outError = "Transient D3D12 Heap Sizeがoverflowしました。";
					Reset();
					return false;
				}

				D3D12_HEAP_DESC heapDesc{};
				heapDesc.SizeInBytes = heapSize;
				heapDesc.Alignment = heapAlignment;
				heapDesc.Properties.Type = heapType;
				heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heapDesc.Properties.CreationNodeMask = 1;
				heapDesc.Properties.VisibleNodeMask = 1;
				heapDesc.Flags = heapFlags;

				HeapSlot physicalSlot{};
				physicalSlot.slotIndex = slot.slotIndex;
				physicalSlot.capacityBytes = heapSize;
				physicalSlot.heapAlignmentBytes = heapAlignment;
				if (FAILED(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&physicalSlot.heap))))
				{
					if (outError) *outError = "Transient D3D12 Heapの作成に失敗しました。";
					Reset();
					return false;
				}

				physicalSlot.heap->SetName(L"RenderGraph Transient Heap");
				physicalHeapBytes_ += heapSize;
				heapSlots_.push_back(std::move(physicalSlot));
			}
			return true;
		}

		Microsoft::WRL::ComPtr<ID3D12Resource> CreatePlacedResource(
			ID3D12Device* device,
			const RenderGraphTransientPool& plan,
			RenderGraph::ResourceHandle resource,
			const D3D12_RESOURCE_DESC& resourceDesc,
			D3D12_RESOURCE_STATES initialState,
			const D3D12_CLEAR_VALUE* clearValue = nullptr,
			std::string* outError = nullptr) const
		{
			if (outError) outError->clear();
			Microsoft::WRL::ComPtr<ID3D12Resource> placedResource;
			if (!device)
			{
				if (outError) *outError = "D3D12 Deviceがありません。";
				return placedResource;
			}

			const RenderGraphTransientPool::AllocationRecord* allocation = plan.GetAllocation(resource);
			if (!allocation)
			{
				if (outError) *outError = "Transient ResourceのAllocation Planがありません。";
				return placedResource;
			}
			const HeapSlot* heapSlot = FindHeapSlot(allocation->slotIndex);
			if (!heapSlot || !heapSlot->heap)
			{
				if (outError) *outError = "Transient ResourceのPhysical Heap Slotがありません。";
				return placedResource;
			}

			const D3D12_RESOURCE_ALLOCATION_INFO info = device->GetResourceAllocationInfo(0, 1, &resourceDesc);
			if (info.SizeInBytes == 0 || info.SizeInBytes == UINT64_MAX ||
				info.SizeInBytes > heapSlot->capacityBytes || info.Alignment > heapSlot->heapAlignmentBytes)
			{
				if (outError) *outError = "Placed ResourceがTransient Heap SlotのSize/Alignmentに収まりません。";
				return placedResource;
			}

			// 同一SlotのResourceはoffset 0を共有し、Lifetime切替時のAliasing Barrierで所有権を移す。
			if (FAILED(device->CreatePlacedResource(
				heapSlot->heap.Get(),
				0,
				&resourceDesc,
				initialState,
				clearValue,
				IID_PPV_ARGS(&placedResource))))
			{
				if (outError) *outError = "Transient Placed Resourceの作成に失敗しました。";
				placedResource.Reset();
			}
			return placedResource;
		}

		[[nodiscard]] const HeapSlot* FindHeapSlot(uint32_t slotIndex) const
		{
			const auto it = std::find_if(
				heapSlots_.begin(), heapSlots_.end(),
				[slotIndex](const HeapSlot& slot)
				{
					return slot.slotIndex == slotIndex;
				});
			return it != heapSlots_.end() ? &(*it) : nullptr;
		}

		[[nodiscard]] const std::vector<HeapSlot>& GetHeapSlots() const { return heapSlots_; }
		[[nodiscard]] UINT64 GetPhysicalHeapBytes() const { return physicalHeapBytes_; }

	private:
		static bool TryAlignUp(UINT64 value, UINT64 alignment, UINT64& outValue)
		{
			if (alignment == 0)
			{
				return false;
			}
			const UINT64 mask = alignment - 1;
			if (value > UINT64_MAX - mask)
			{
				return false;
			}
			outValue = (value + mask) & ~mask;
			return true;
		}

		std::vector<HeapSlot> heapSlots_;
		UINT64 physicalHeapBytes_ = 0;
	};
} // namespace Ken4lowEngine
