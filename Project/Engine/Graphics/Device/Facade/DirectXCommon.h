#pragma once
#include "DX12Include.h"
#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "DXCCompilerManager.h"
#include "DX12CommandManager.h"
#include "DX12FenceManager.h"
#include "MainRenderTarget.h"
#include "ShadowMapRenderTarget.h"
#include "PipelineFactory.h"

#include <dxcapi.h>
#include <memory>
#include <algorithm>

namespace Ken4lowEngine
{
	class WinApp;

	class DirectXCommon
	{
	public:
		struct EndDrawPerformanceTiming
		{
			float renderTargetEndMs = 0.0f;
			float transitionToPresentMs = 0.0f;
			float commandListCloseMs = 0.0f;
			float executeCommandListsMs = 0.0f;
			float presentMs = 0.0f;
			float fenceSignalMs = 0.0f;
			float fenceWaitMs = 0.0f;
			float allocatorResetMs = 0.0f;
			float commandListResetMs = 0.0f;
			float totalMs = 0.0f;
		};

		static DirectXCommon* GetInstance();

		void Initialize(WinApp* winApp, uint32_t width, uint32_t height);
		void Finalize();

		void BeginDraw();
		void PrepareBackBufferForImGui();
		void PrepareBackBufferForGame();
		void RebindBackBufferForGameOverlay();
		void EndDraw();

		void BeginShadowMapPass();
		void EndShadowMapPass();

		void ResourceTransition(
			ID3D12Resource* resource,
			D3D12_RESOURCE_STATES stateBefore,
			D3D12_RESOURCE_STATES stateAfter)
		{
			commandManager_->ResourceTransition(resource, stateBefore, stateAfter);
		}

		void Resize(uint32_t width, uint32_t height);
		void SetShadowMapSize(uint32_t width, uint32_t height);

		/// 単一Upload Buffer更新が残る間は既定OFFとし、Per-Frame GPU Buffer移行後に常用する。
		void SetFramesInFlightEnabled(bool enabled) { framesInFlightEnabled_ = enabled; }
		bool IsFramesInFlightEnabled() const { return framesInFlightEnabled_; }

		ID3D12Device* GetDevice() const { return device_->GetDevice(); }
		DX12SwapChain* GetSwapChain() { return swapChain_.get(); }
		DXCCompilerManager* GetDXCCompilerManager() { return dxcCompilerManager_.get(); }
		DX12CommandManager* GetCommandManager() { return commandManager_.get(); }
		DX12FenceManager* GetFenceManager() { return fenceManager_.get(); }
		DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() const { return swapChain_->GetSwapChainDesc(); }
		const EndDrawPerformanceTiming& GetEndDrawPerformanceTiming() const { return endDrawPerformanceTiming_; }

		ComPtr<ID3D12Resource> GetBackBuffer(uint32_t index);

		uint32_t GetBackBufferCount() const
		{
			return swapChain_->GetSwapChainDesc().BufferCount;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32_t index) const
		{
			return mainRenderTarget_->GetRtvHandleCPU(index);
		}

		uint32_t GetClientWidth() const { return clientWidth_; }
		uint32_t GetClientHeight() const { return clientHeight_; }

		MainRenderTarget* GetMainRenderTarget() const { return mainRenderTarget_.get(); }
		ShadowMapRenderTarget* GetShadowMapRenderTarget() const { return shadowMapRenderTarget_.get(); }

		uint32_t GetShadowMapSrvIndex() const
		{
			return shadowMapRenderTarget_ ? shadowMapRenderTarget_->GetSrvIndex() : UINT32_MAX;
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetShadowMapSrvHandleGPU() const
		{
			return shadowMapRenderTarget_->GetSrvHandleGPU();
		}

		PipelineFactory& GetPipelineFactory() { return pipelineFactory_; }
		const PipelineFactory& GetPipelineFactory() const { return pipelineFactory_; }

	private:
		void InitializeCoreObjects();
		void InitializeCoreSystems(WinApp* winApp, uint32_t width, uint32_t height);
		void InitializeManagers();
		void InitializeRenderTargets();
		void WaitForGpuIdle();
		void DebugLayer();
		void ErrorWarning();

	private:
		uint32_t clientWidth_ = 0;
		uint32_t clientHeight_ = 0;

		std::unique_ptr<DX12Device> device_;
		std::unique_ptr<DX12SwapChain> swapChain_;
		std::unique_ptr<DXCCompilerManager> dxcCompilerManager_;
		std::unique_ptr<DX12CommandManager> commandManager_;
		std::unique_ptr<DX12FenceManager> fenceManager_;

		PipelineFactory pipelineFactory_;
		std::unique_ptr<MainRenderTarget> mainRenderTarget_;
		std::unique_ptr<ShadowMapRenderTarget> shadowMapRenderTarget_;

		UINT backBufferIndex_ = 0;
		EndDrawPerformanceTiming endDrawPerformanceTiming_{};
		bool framesInFlightEnabled_ = false;

	private:
		DirectXCommon() = default;
		~DirectXCommon() = default;
		DirectXCommon(const DirectXCommon&) = delete;
		const DirectXCommon& operator=(const DirectXCommon&) = delete;
	};

} // namespace Ken4lowEngine
