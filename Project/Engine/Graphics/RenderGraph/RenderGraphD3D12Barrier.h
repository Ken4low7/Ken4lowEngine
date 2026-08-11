#pragma once

#include "RenderGraph.h"

#include <d3d12.h>

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// RenderGraphのAPI非依存Barrier計画をD3D12 ResourceBarrierへ変換する薄いBackend Adapter。
	/// </summary>
	class RenderGraphD3D12BarrierEmitter
	{
	public:
		void ResetBindings()
		{
			resourceBindings_.clear();
		}

		bool BindResource(RenderGraph::ResourceHandle handle, ID3D12Resource* resource)
		{
			if (!handle.IsValid() || !resource)
			{
				return false;
			}

			const std::size_t index = handle.id;
			if (resourceBindings_.size() <= index)
			{
				resourceBindings_.resize(index + 1, nullptr);
			}
			resourceBindings_[index] = resource;
			return true;
		}

		[[nodiscard]] ID3D12Resource* ResolveResource(RenderGraph::ResourceHandle handle) const
		{
			if (!handle.IsValid() || handle.id >= resourceBindings_.size())
			{
				return nullptr;
			}
			return resourceBindings_[handle.id];
		}

		bool Emit(
			ID3D12GraphicsCommandList* commandList,
			const RenderGraph::BarrierRecord& record,
			std::string* outError = nullptr) const
		{
			if (outError) outError->clear();
			if (!commandList)
			{
				if (outError) *outError = "D3D12 CommandListがありません。";
				return false;
			}

			ID3D12Resource* resource = ResolveResource(record.resource);
			if (!resource)
			{
				if (outError) *outError = "RenderGraph ResourceにD3D12 ResourceがBindされていません。";
				return false;
			}

			D3D12_RESOURCE_BARRIER barrier{};
			if (record.type == RenderGraph::BarrierType::UnorderedAccess)
			{
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.UAV.pResource = resource;
			}
			else
			{
				D3D12_RESOURCE_STATES beforeState{};
				D3D12_RESOURCE_STATES afterState{};
				if (!TryMapResourceState(record.before, beforeState) || !TryMapResourceState(record.after, afterState))
				{
					if (outError) *outError = "Unknown ResourceStateはD3D12 Barrierへ変換できません。";
					return false;
				}

				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition.pResource = resource;
				barrier.Transition.StateBefore = beforeState;
				barrier.Transition.StateAfter = afterState;
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			}

			// Physical emission is opt-in per bound resource so legacy owner-managed barriers are never duplicated accidentally.
			commandList->ResourceBarrier(1, &barrier);
			return true;
		}

		static bool TryMapResourceState(
			RenderGraph::ResourceState state,
			D3D12_RESOURCE_STATES& outState)
		{
			switch (state)
			{
			case RenderGraph::ResourceState::Common:
				outState = D3D12_RESOURCE_STATE_COMMON;
				return true;
			case RenderGraph::ResourceState::RenderTarget:
				outState = D3D12_RESOURCE_STATE_RENDER_TARGET;
				return true;
			case RenderGraph::ResourceState::DepthWrite:
				outState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				return true;
			case RenderGraph::ResourceState::DepthRead:
				outState = D3D12_RESOURCE_STATE_DEPTH_READ;
				return true;
			case RenderGraph::ResourceState::ShaderResource:
				outState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				return true;
			case RenderGraph::ResourceState::UnorderedAccess:
				outState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				return true;
			case RenderGraph::ResourceState::CopySource:
				outState = D3D12_RESOURCE_STATE_COPY_SOURCE;
				return true;
			case RenderGraph::ResourceState::CopyDestination:
				outState = D3D12_RESOURCE_STATE_COPY_DEST;
				return true;
			case RenderGraph::ResourceState::Present:
				outState = D3D12_RESOURCE_STATE_PRESENT;
				return true;
			default:
				outState = D3D12_RESOURCE_STATE_COMMON;
				return false;
			}
		}

	private:
		std::vector<ID3D12Resource*> resourceBindings_;
	};
} // namespace Ken4lowEngine
