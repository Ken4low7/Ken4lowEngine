#pragma once

#include "RenderGraph.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// Compile済みRenderGraphのResourceLifetimeから再利用可能なTransient allocation slotを計画する。
	/// D3D12固有のHeap生成はBackendへ分離し、このクラスはLifetimeと互換性だけを扱う。
	/// </summary>
	class RenderGraphTransientPool
	{
	public:
		struct ResourceDesc
		{
			std::size_t allocationSizeBytes = 0;
			std::size_t alignmentBytes = 1;
			uint64_t compatibilityKey = 0;
			bool allowAliasing = true;
		};

		struct AllocationRecord
		{
			RenderGraph::ResourceHandle resource{};
			uint32_t slotIndex = 0;
			std::size_t sizeBytes = 0;
			std::size_t alignmentBytes = 1;
			bool aliased = false;
		};

		struct SlotRecord
		{
			uint32_t slotIndex = 0;
			std::size_t capacityBytes = 0;
			std::size_t alignmentBytes = 1;
			uint64_t compatibilityKey = 0;
			bool aliasingEnabled = true;
		};

		struct AliasingRecord
		{
			RenderGraph::ResourceHandle beforeResource{};
			RenderGraph::ResourceHandle afterResource{};
			RenderGraph::PassHandle beforePass{};
			uint32_t slotIndex = 0;
		};

		struct Stats
		{
			std::size_t registeredResourceCount = 0;
			std::size_t activeResourceCount = 0;
			std::size_t physicalSlotCount = 0;
			std::size_t aliasingReuseCount = 0;
			std::size_t logicalBytes = 0;
			std::size_t physicalBytes = 0;
			std::size_t peakLiveBytes = 0;
			std::size_t savedBytes = 0;
			std::size_t fragmentationBytes = 0;
		};

		void Reset()
		{
			registrations_.clear();
			allocations_.clear();
			slots_.clear();
			aliasingPlan_.clear();
			stats_ = {};
		}

		bool RegisterResource(RenderGraph::ResourceHandle resource, const ResourceDesc& desc)
		{
			if (!resource.IsValid())
			{
				return false;
			}

			const auto existing = std::find_if(
				registrations_.begin(), registrations_.end(),
				[resource](const Registration& registration)
				{
					return registration.resource == resource;
				});
			if (existing != registrations_.end())
			{
				existing->desc = desc;
			}
			else
			{
				registrations_.push_back({ resource, desc });
			}
			return true;
		}

		bool Build(const RenderGraph& graph, std::string* outError = nullptr)
		{
			if (outError) outError->clear();
			allocations_.clear();
			slots_.clear();
			aliasingPlan_.clear();
			stats_ = {};
			stats_.registeredResourceCount = registrations_.size();

			struct ActiveRequest
			{
				RenderGraph::ResourceHandle resource{};
				ResourceDesc desc{};
				RenderGraph::ResourceLifetime lifetime{};
				std::size_t alignedSizeBytes = 0;
			};

			std::vector<ActiveRequest> requests;
			requests.reserve(registrations_.size());
			for (const Registration& registration : registrations_)
			{
				const RenderGraph::ResourceLifetime* lifetime = graph.GetResourceLifetime(registration.resource);
				if (!lifetime)
				{
					if (outError) *outError = "Transient Resource HandleがRenderGraphに存在しません。";
					return false;
				}
				if (lifetime->imported)
				{
					if (outError) *outError = "Imported ResourceはTransient Poolへ登録できません。";
					return false;
				}
				if (lifetime->firstPass == (std::numeric_limits<std::size_t>::max)())
				{
					continue;
				}
				if (registration.desc.allocationSizeBytes == 0 || !IsPowerOfTwo(registration.desc.alignmentBytes))
				{
					if (outError) *outError = "Transient ResourceのSizeまたはAlignmentが無効です。";
					return false;
				}

				std::size_t alignedSizeBytes = 0;
				if (!TryAlignUp(registration.desc.allocationSizeBytes, registration.desc.alignmentBytes, alignedSizeBytes))
				{
					if (outError) *outError = "Transient ResourceのAllocation Sizeがoverflowしました。";
					return false;
				}

				requests.push_back({ registration.resource, registration.desc, *lifetime, alignedSizeBytes });
				stats_.logicalBytes += alignedSizeBytes;
			}

			std::sort(
				requests.begin(), requests.end(),
				[](const ActiveRequest& left, const ActiveRequest& right)
				{
					if (left.lifetime.firstPass != right.lifetime.firstPass) return left.lifetime.firstPass < right.lifetime.firstPass;
					if (left.lifetime.lastPass != right.lifetime.lastPass) return left.lifetime.lastPass < right.lifetime.lastPass;
					return left.resource.id < right.resource.id;
				});

			stats_.activeResourceCount = requests.size();
			for (std::size_t passIndex = 0; passIndex < graph.GetCompiledPassCount(); ++passIndex)
			{
				std::size_t liveBytes = 0;
				for (const ActiveRequest& request : requests)
				{
					if (request.lifetime.firstPass <= passIndex && passIndex <= request.lifetime.lastPass)
					{
						liveBytes += request.alignedSizeBytes;
					}
				}
				stats_.peakLiveBytes = (std::max)(stats_.peakLiveBytes, liveBytes);
			}

			struct SlotState
			{
				SlotRecord record{};
				std::size_t lastPass = 0;
				RenderGraph::ResourceHandle lastResource{};
			};
			std::vector<SlotState> slotStates;

			// Lifetimeが重ならずCompatibility Keyが一致するResourceだけを同一物理Slotへaliasする。
			for (const ActiveRequest& request : requests)
			{
				std::size_t bestSlot = (std::numeric_limits<std::size_t>::max)();
				if (request.desc.allowAliasing)
				{
					for (std::size_t slotIndex = 0; slotIndex < slotStates.size(); ++slotIndex)
					{
						const SlotState& slot = slotStates[slotIndex];
						if (!slot.record.aliasingEnabled || slot.record.compatibilityKey != request.desc.compatibilityKey) continue;
						if (slot.lastPass >= request.lifetime.firstPass || slot.record.capacityBytes < request.alignedSizeBytes) continue;
						if (bestSlot == (std::numeric_limits<std::size_t>::max)() ||
							slot.record.capacityBytes < slotStates[bestSlot].record.capacityBytes)
						{
							bestSlot = slotIndex;
						}
					}
				}

				if (bestSlot != (std::numeric_limits<std::size_t>::max)())
				{
					SlotState& slot = slotStates[bestSlot];
					aliasingPlan_.push_back({
						slot.lastResource,
						request.resource,
						graph.GetCompiledPassHandle(request.lifetime.firstPass),
						slot.record.slotIndex,
					});
					allocations_.push_back({
						request.resource,
						slot.record.slotIndex,
						request.alignedSizeBytes,
						request.desc.alignmentBytes,
						true,
					});
					slot.lastPass = request.lifetime.lastPass;
					slot.lastResource = request.resource;
					slot.record.alignmentBytes = (std::max)(slot.record.alignmentBytes, request.desc.alignmentBytes);
					++stats_.aliasingReuseCount;
					continue;
				}

				SlotState newSlot{};
				newSlot.record.slotIndex = static_cast<uint32_t>(slotStates.size());
				newSlot.record.capacityBytes = request.alignedSizeBytes;
				newSlot.record.alignmentBytes = request.desc.alignmentBytes;
				newSlot.record.compatibilityKey = request.desc.compatibilityKey;
				newSlot.record.aliasingEnabled = request.desc.allowAliasing;
				newSlot.lastPass = request.lifetime.lastPass;
				newSlot.lastResource = request.resource;
				allocations_.push_back({
					request.resource,
					newSlot.record.slotIndex,
					request.alignedSizeBytes,
					request.desc.alignmentBytes,
					false,
				});
				slotStates.push_back(newSlot);
			}

			slots_.reserve(slotStates.size());
			for (const SlotState& slot : slotStates)
			{
				slots_.push_back(slot.record);
				stats_.physicalBytes += slot.record.capacityBytes;
			}
			stats_.physicalSlotCount = slots_.size();
			stats_.savedBytes = stats_.logicalBytes > stats_.physicalBytes ? stats_.logicalBytes - stats_.physicalBytes : 0;
			stats_.fragmentationBytes = stats_.physicalBytes > stats_.peakLiveBytes ? stats_.physicalBytes - stats_.peakLiveBytes : 0;
			return true;
		}

		[[nodiscard]] const AllocationRecord* GetAllocation(RenderGraph::ResourceHandle resource) const
		{
			const auto it = std::find_if(
				allocations_.begin(), allocations_.end(),
				[resource](const AllocationRecord& allocation)
				{
					return allocation.resource == resource;
				});
			return it != allocations_.end() ? &(*it) : nullptr;
		}

		[[nodiscard]] const std::vector<AllocationRecord>& GetAllocations() const { return allocations_; }
		[[nodiscard]] const std::vector<SlotRecord>& GetSlots() const { return slots_; }
		[[nodiscard]] const std::vector<AliasingRecord>& GetAliasingPlan() const { return aliasingPlan_; }
		[[nodiscard]] const Stats& GetStats() const { return stats_; }

	private:
		struct Registration
		{
			RenderGraph::ResourceHandle resource{};
			ResourceDesc desc{};
		};

		static bool IsPowerOfTwo(std::size_t value)
		{
			return value != 0 && (value & (value - 1)) == 0;
		}

		static bool TryAlignUp(std::size_t value, std::size_t alignment, std::size_t& outValue)
		{
			const std::size_t mask = alignment - 1;
			if (value > (std::numeric_limits<std::size_t>::max)() - mask)
			{
				return false;
			}
			outValue = (value + mask) & ~mask;
			return true;
		}

		std::vector<Registration> registrations_;
		std::vector<AllocationRecord> allocations_;
		std::vector<SlotRecord> slots_;
		std::vector<AliasingRecord> aliasingPlan_;
		Stats stats_{};
	};
} // namespace Ken4lowEngine
