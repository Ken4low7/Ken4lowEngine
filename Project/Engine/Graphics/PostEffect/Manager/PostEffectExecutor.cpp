#define NOMINMAX
#include "PostEffectExecutor.h"

#include "PostEffectRenderTargetManager.h"
#include "PostEffectRegistry.h"
#include "PostEffectChain.h"
#include "PostEffectRuntimeState.h"
#include "PostEffectPipelineBuilder.h"
#include "DirectXCommon.h"
#include "SRVManager.h"
#include "UAVManager.h"
#include "LogString.h"

#include <cassert>
#include <utility>

namespace Ken4lowEngine
{
	void PostEffectExecutor::Initialize(
		DirectXCommon* dxCommon,
		PostEffectPipelineBuilder* pipelineBuilder,
		PostEffectRegistry* registry,
		PostEffectChain* chain,
		PostEffectRuntimeState* runtimeState,
		PostEffectRenderTargetManager* renderTargetManager)
	{
		dxCommon_ = dxCommon;
		pipelineBuilder_ = pipelineBuilder;
		registry_ = registry;
		chain_ = chain;
		runtimeState_ = runtimeState;
		renderTargetManager_ = renderTargetManager;
	}

	void PostEffectExecutor::Finalize()
	{
		renderTargetManager_ = nullptr;
		runtimeState_ = nullptr;
		chain_ = nullptr;
		registry_ = nullptr;
		pipelineBuilder_ = nullptr;
		dxCommon_ = nullptr;
	}

	void PostEffectExecutor::TransitionTo(
		PostEffectRenderTarget& renderTarget,
		ID3D12GraphicsCommandList* commandList,
		D3D12_RESOURCE_STATES nextState)
	{
		if (!commandList || !renderTarget.resource || renderTarget.currentState == nextState)
		{
			return;
		}

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = renderTarget.resource.Get();
		barrier.Transition.StateBefore = renderTarget.currentState;
		barrier.Transition.StateAfter = nextState;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		renderTarget.currentState = nextState; // 実Resourceの状態を唯一の記録値として次Barrierへ引き継ぐ。
	}

	void PostEffectExecutor::TransitionDepthTo(D3D12_RESOURCE_STATES nextState)
	{
		ID3D12Resource* depthResource = renderTargetManager_->GetDepthResource();
		const D3D12_RESOURCE_STATES currentState = renderTargetManager_->GetDepthState();
		if (!depthResource || currentState == nextState)
		{
			return;
		}
		dxCommon_->ResourceTransition(depthResource, currentState, nextState);
		renderTargetManager_->SetDepthState(nextState);
	}

	void PostEffectExecutor::CopyRenderTarget(
		PostEffectRenderTarget& source,
		PostEffectRenderTarget& destination,
		ID3D12GraphicsCommandList* commandList)
	{
		assert(source.resource.Get() != destination.resource.Get());
		SRVManager::GetInstance()->PreDraw();
		TransitionTo(source, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		TransitionTo(destination, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (!renderTargetManager_->ValidateForDraw(destination, false, "CopyRenderTarget"))
		{
			return;
		}

		commandList->OMSetRenderTargets(1, &destination.rtvHandle, false, nullptr);
		commandList->SetPipelineState(pipelineBuilder_->GetCopyPipelineState().Get());
		commandList->SetGraphicsRootSignature(pipelineBuilder_->GetCopyRootSignature().Get());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, source.srvIndex);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}

	void PostEffectExecutor::CopyRenderTargetToBackBuffer(
		PostEffectRenderTarget& source,
		ID3D12GraphicsCommandList* commandList)
	{
		const uint32_t backBufferIndex = dxCommon_->GetSwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = dxCommon_->GetBackBuffer(backBufferIndex);
		auto* swapChain = dxCommon_->GetSwapChain();

		SRVManager::GetInstance()->PreDraw();
		TransitionTo(source, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		dxCommon_->ResourceTransition(backBuffer.Get(), swapChain->GetBackBufferState(backBufferIndex), D3D12_RESOURCE_STATE_RENDER_TARGET);
		swapChain->SetBackBufferState(backBufferIndex, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = dxCommon_->GetBackBufferRTV(backBufferIndex);
		commandList->OMSetRenderTargets(1, &backBufferRtv, false, nullptr);
		commandList->RSSetViewports(1, &dxCommon_->GetMainRenderTarget()->GetViewport());
		commandList->RSSetScissorRects(1, &dxCommon_->GetMainRenderTarget()->GetScissorRect());
		commandList->SetPipelineState(pipelineBuilder_->GetCopyPipelineState().Get());
		commandList->SetGraphicsRootSignature(pipelineBuilder_->GetCopyRootSignature().Get());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, source.srvIndex);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}

	void PostEffectExecutor::BeginDraw()
	{
		if (!renderTargetManager_ || renderTargetManager_->Empty())
		{
			Log("[PostEffectManager] BeginDraw skipped: renderTargets is empty.\n");
			return;
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		PostEffectRenderTarget& renderTarget = renderTargetManager_->GetGameRenderTarget();
		if (!commandList || !renderTargetManager_->ValidateForDraw(renderTarget, true, "BeginDraw"))
		{
			return;
		}

		TransitionTo(renderTarget, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		TransitionDepthTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = renderTargetManager_->GetDsvHandle();
		commandList->OMSetRenderTargets(1, &renderTarget.rtvHandle, false, &dsvHandle);
		const float clearColor[] = {
			renderTarget.clearColor.x, renderTarget.clearColor.y,
			renderTarget.clearColor.z, renderTarget.clearColor.w };
		commandList->ClearRenderTargetView(renderTarget.rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		commandList->RSSetViewports(1, &renderTargetManager_->GetViewport());
		commandList->RSSetScissorRects(1, &renderTargetManager_->GetScissorRect());
	}

	void PostEffectExecutor::EndDraw()
	{
		if (!renderTargetManager_ || renderTargetManager_->Empty())
		{
			return;
		}
		TransitionDepthTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		TransitionTo(renderTargetManager_->GetGameRenderTarget(), commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		// 後続PostEffectは同じCommandList上のBarrier順序で安全なため、フレーム途中のGPU完了待ちは行わない。
	}

	void PostEffectExecutor::RenderPostEffect()
	{
		if (!renderTargetManager_ || renderTargetManager_->Empty())
		{
			return;
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		commandList->RSSetViewports(1, &renderTargetManager_->GetViewport());
		commandList->RSSetScissorRects(1, &renderTargetManager_->GetScissorRect());

		std::vector<PostEffectRenderTarget>& renderTargets = renderTargetManager_->GetRenderTargets();
		if (renderTargets.size() < 2)
		{
			TransitionTo(renderTargets.front(), commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			return;
		}

		uint32_t sourceIndex = 0;
		uint32_t destinationIndex = 1;
		bool appliedAnyEffect = false;

		for (const std::string& name : chain_->GetOrderedEffectNames())
		{
			if (!runtimeState_->IsActive(name))
			{
				continue;
			}

			IPostEffect* effect = registry_->Find(name);
			const PostEffectDefinition* definition = registry_->FindDefinition(name);
			if (!effect || !definition)
			{
				continue;
			}

			appliedAnyEffect = true;
			PostEffectRenderTarget& input = renderTargets[sourceIndex];
			PostEffectRenderTarget& output = renderTargets[destinationIndex];
			assert(input.resource.Get() != output.resource.Get());

			if (definition->executionPath == PostEffectExecutionPath::Compute)
			{
				TransitionTo(input, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				TransitionTo(output, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				UAVManager::GetInstance()->PreDispatch();
				effect->Apply(commandList, input.srvIndexOnUavHeap, output.uavIndex, renderTargetManager_->GetDepthSrvIndex());
				TransitionTo(output, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			else
			{
				SRVManager::GetInstance()->PreDraw();
				TransitionTo(input, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				TransitionTo(output, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
				if (!renderTargetManager_->ValidateForDraw(output, true, "RenderPostEffect"))
				{
					continue;
				}
				const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = renderTargetManager_->GetDsvHandle();
				commandList->OMSetRenderTargets(1, &output.rtvHandle, false, &dsvHandle);
				effect->Apply(commandList, input.srvIndex, output.uavIndex, renderTargetManager_->GetDepthSrvIndex());
				commandList->OMSetRenderTargets(0, nullptr, false, nullptr);
				TransitionTo(output, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}

			std::swap(sourceIndex, destinationIndex);
		}

		PostEffectRenderTarget& finalTarget = renderTargets[sourceIndex];
		PostEffectRenderTarget& gameTarget = renderTargets.front();
		if (sourceIndex != 0)
		{
			CopyRenderTarget(finalTarget, gameTarget, commandList);
		}
		else if (appliedAnyEffect)
		{
			CopyRenderTarget(gameTarget, renderTargets[1], commandList);
			CopyRenderTarget(renderTargets[1], gameTarget, commandList);
		}
		TransitionTo(gameTarget, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void PostEffectExecutor::RenderPostEffectToBackBuffer()
	{
		RenderPostEffect();
		if (!renderTargetManager_ || renderTargetManager_->Empty())
		{
			return;
		}
		CopyRenderTargetToBackBuffer(
			renderTargetManager_->GetGameRenderTarget(),
			dxCommon_->GetCommandManager()->GetCommandList());
	}

	void PostEffectExecutor::BeginGameRenderTargetOverlay()
	{
		if (!renderTargetManager_ || renderTargetManager_->Empty())
		{
			Log("[PostEffectManager] BeginGameRenderTargetOverlay skipped: renderTargets is empty.\n");
			return;
		}
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		PostEffectRenderTarget& gameTarget = renderTargetManager_->GetGameRenderTarget();
		TransitionTo(gameTarget, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (!renderTargetManager_->ValidateForDraw(gameTarget, false, "BeginGameRenderTargetOverlay"))
		{
			return;
		}
		commandList->OMSetRenderTargets(1, &gameTarget.rtvHandle, false, nullptr);
		commandList->RSSetViewports(1, &renderTargetManager_->GetViewport());
		commandList->RSSetScissorRects(1, &renderTargetManager_->GetScissorRect());
	}

	void PostEffectExecutor::EndGameRenderTargetOverlay()
	{
		if (!renderTargetManager_ || renderTargetManager_->Empty()) { return; }
		TransitionTo(
			renderTargetManager_->GetGameRenderTarget(),
			dxCommon_->GetCommandManager()->GetCommandList(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void PostEffectExecutor::BindSceneRenderTarget()
	{
		if (!renderTargetManager_ || renderTargetManager_->Empty())
		{
			Log("[PostEffectManager] BindSceneRenderTarget skipped: renderTargets is empty.\n");
			return;
		}
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		PostEffectRenderTarget& gameTarget = renderTargetManager_->GetGameRenderTarget();
		if (!commandList || !renderTargetManager_->ValidateForDraw(gameTarget, true, "BindSceneRenderTarget"))
		{
			return;
		}
		TransitionTo(gameTarget, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		TransitionDepthTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = renderTargetManager_->GetDsvHandle();
		commandList->OMSetRenderTargets(1, &gameTarget.rtvHandle, false, &dsvHandle);
		commandList->RSSetViewports(1, &renderTargetManager_->GetViewport());
		commandList->RSSetScissorRects(1, &renderTargetManager_->GetScissorRect());
	}
}
