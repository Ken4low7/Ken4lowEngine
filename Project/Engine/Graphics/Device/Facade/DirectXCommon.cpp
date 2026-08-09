#define NOMINMAX
#include "DirectXCommon.h"
#include "WinApp.h"
#include "RTVManager.h"
#include "DSVManager.h"
#include "SRVManager.h"

#include <cassert>
#include <chrono>

namespace Ken4lowEngine
{
#pragma comment(lib,"dxcompiler.lib")

	using namespace Microsoft::WRL;

	namespace
	{
		constexpr uint32_t kMinShadowMapSize = 1;
		using Clock = std::chrono::steady_clock;

		float ToMilliseconds(const Clock::time_point& begin)
		{
			return std::chrono::duration<float, std::milli>(Clock::now() - begin).count();
		}
	}

	DirectXCommon* DirectXCommon::GetInstance()
	{
		static DirectXCommon instance;
		return &instance;
	}

	void DirectXCommon::Initialize(WinApp* winApp, uint32_t width, uint32_t height)
	{
		clientWidth_ = width;
		clientHeight_ = height;
		endDrawPerformanceTiming_ = {};
		framesInFlightEnabled_ = false;

		InitializeCoreObjects();
		InitializeCoreSystems(winApp, width, height);
		InitializeManagers();
		InitializeRenderTargets();
	}

	void DirectXCommon::InitializeCoreObjects()
	{
		device_ = std::make_unique<DX12Device>();
		swapChain_ = std::make_unique<DX12SwapChain>();
		dxcCompilerManager_ = std::make_unique<DXCCompilerManager>();
		commandManager_ = std::make_unique<DX12CommandManager>();
		fenceManager_ = std::make_unique<DX12FenceManager>();
	}

	void DirectXCommon::InitializeCoreSystems(WinApp* winApp, uint32_t width, uint32_t height)
	{
		DebugLayer();
		device_->Initialize();
		ErrorWarning();

		commandManager_->Initialize(GetDevice());
		commandManager_->SetFenceManager(fenceManager_.get());
		fenceManager_->Initialize(GetDevice());

		swapChain_->Initialize(
			winApp,
			device_->GetDXGIFactory(),
			commandManager_->GetCommandQueue(),
			width,
			height);

		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		commandManager_->ConfigureFramesInFlight(
			GetDevice(),
			swapChain_->GetSwapChainDesc().BufferCount,
			backBufferIndex_);

		device_->GetDXGIFactory()->MakeWindowAssociation(winApp->GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
		dxcCompilerManager_->Initialize();
		pipelineFactory_.Initialize(GetDevice());
	}

	void DirectXCommon::InitializeManagers()
	{
		DSVManager::GetInstance()->Initialize(this);
		RTVManager::GetInstance()->Initialize(this);
		SRVManager::GetInstance()->Initialize(this);
	}

	void DirectXCommon::InitializeRenderTargets()
	{
		MainRenderTargetSettings mainSettings{};
		mainRenderTarget_ = std::make_unique<MainRenderTarget>();
		mainRenderTarget_->Initialize(this, mainSettings);

		ShadowMapSettings shadowSettings{};
		shadowSettings.width = 2048;
		shadowSettings.height = 2048;
		shadowMapRenderTarget_ = std::make_unique<ShadowMapRenderTarget>();
		shadowMapRenderTarget_->Initialize(this, shadowSettings);
	}

	void DirectXCommon::BeginDraw()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_) return;
		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
	}

	void DirectXCommon::PrepareBackBufferForImGui()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_) return;
		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = GetBackBuffer(backBufferIndex_);
		const D3D12_RESOURCE_STATES beforeState = swapChain_->GetBackBufferState(backBufferIndex_);
		ResourceTransition(backBuffer.Get(), beforeState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		swapChain_->SetBackBufferState(backBufferIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mainRenderTarget_->Begin(commandManager_->GetCommandList(), backBufferIndex_);
	}

	void DirectXCommon::PrepareBackBufferForGame()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_) return;
		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = GetBackBuffer(backBufferIndex_);
		const D3D12_RESOURCE_STATES beforeState = swapChain_->GetBackBufferState(backBufferIndex_);
		ResourceTransition(backBuffer.Get(), beforeState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		swapChain_->SetBackBufferState(backBufferIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mainRenderTarget_->Begin(commandManager_->GetCommandList(), backBufferIndex_);
	}

	void DirectXCommon::RebindBackBufferForGameOverlay()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_) return;
		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = GetBackBuffer(backBufferIndex_);
		const D3D12_RESOURCE_STATES beforeState = swapChain_->GetBackBufferState(backBufferIndex_);
		ResourceTransition(backBuffer.Get(), beforeState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		swapChain_->SetBackBufferState(backBufferIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mainRenderTarget_->Bind(commandManager_->GetCommandList(), backBufferIndex_);
	}

	void DirectXCommon::EndDraw()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_ || !fenceManager_) return;

		endDrawPerformanceTiming_ = {};
		const auto totalBegin = Clock::now();

		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = GetBackBuffer(backBufferIndex_);

		const auto renderTargetEndBegin = Clock::now();
		mainRenderTarget_->End(commandManager_->GetCommandList());
		endDrawPerformanceTiming_.renderTargetEndMs = ToMilliseconds(renderTargetEndBegin);

		const auto transitionBegin = Clock::now();
		const D3D12_RESOURCE_STATES beforeState = swapChain_->GetBackBufferState(backBufferIndex_);
		ResourceTransition(backBuffer.Get(), beforeState, D3D12_RESOURCE_STATE_PRESENT);
		swapChain_->SetBackBufferState(backBufferIndex_, D3D12_RESOURCE_STATE_PRESENT);
		endDrawPerformanceTiming_.transitionToPresentMs = ToMilliseconds(transitionBegin);

		const uint32_t submittedFrameIndex = commandManager_->GetCurrentFrameIndex();
		commandManager_->Execute();

		const auto presentBegin = Clock::now();
		const HRESULT presentResult = swapChain_->GetSwapChain()->Present(1, 0);
		endDrawPerformanceTiming_.presentMs = ToMilliseconds(presentBegin);

		if (framesInFlightEnabled_)
		{
			commandManager_->SignalCurrentFrame(submittedFrameIndex);
			const uint32_t nextFrameIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
			commandManager_->PrepareFrame(nextFrameIndex);
			backBufferIndex_ = nextFrameIndex;
		}
		else
		{
			commandManager_->WaitAndReset(); // 単一Upload Buffer更新が残る間の安全な互換同期経路。
		}

		const DX12CommandManager::PerformanceTiming& commandTiming = commandManager_->GetPerformanceTiming();
		endDrawPerformanceTiming_.commandListCloseMs = commandTiming.commandListCloseMs;
		endDrawPerformanceTiming_.executeCommandListsMs = commandTiming.executeCommandListsMs;
		endDrawPerformanceTiming_.fenceSignalMs = commandTiming.fenceSignalMs;
		endDrawPerformanceTiming_.fenceWaitMs = commandTiming.fenceWaitMs;
		endDrawPerformanceTiming_.allocatorResetMs = commandTiming.allocatorResetMs;
		endDrawPerformanceTiming_.commandListResetMs = commandTiming.commandListResetMs;
		endDrawPerformanceTiming_.totalMs = ToMilliseconds(totalBegin);
		assert(SUCCEEDED(presentResult));
	}

	void DirectXCommon::BeginShadowMapPass()
	{
		if (!shadowMapRenderTarget_ || !commandManager_) return;
		shadowMapRenderTarget_->Begin(commandManager_->GetCommandList());
	}

	void DirectXCommon::EndShadowMapPass()
	{
		if (!shadowMapRenderTarget_ || !commandManager_) return;
		shadowMapRenderTarget_->End(commandManager_->GetCommandList());
	}

	void DirectXCommon::Finalize()
	{
		if (!commandManager_ || !fenceManager_) return;
		WaitForGpuIdle();

		if (mainRenderTarget_)
		{
			mainRenderTarget_->Finalize();
			mainRenderTarget_.reset();
		}
		if (shadowMapRenderTarget_)
		{
			shadowMapRenderTarget_->Finalize();
			shadowMapRenderTarget_.reset();
		}

		pipelineFactory_.Finalize();
		fenceManager_->Finalize();
		commandManager_->Finalize();
		dxcCompilerManager_->Finalize();
		swapChain_->Finalize();
		RTVManager::GetInstance()->Finalize();
		DSVManager::GetInstance()->Finalize();
		SRVManager::GetInstance()->Finalize();

		fenceManager_.reset();
		commandManager_.reset();
		dxcCompilerManager_.reset();
		swapChain_.reset();
		device_->Finalize();
		device_.reset();
	}

	void DirectXCommon::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;
		WaitForGpuIdle();
		clientWidth_ = width;
		clientHeight_ = height;
		swapChain_->Resize(width, height);

		if (mainRenderTarget_) mainRenderTarget_->Resize(width, height);
		if (shadowMapRenderTarget_)
		{
			const uint32_t shadowWidth = std::max(shadowMapRenderTarget_->GetWidth(), kMinShadowMapSize);
			const uint32_t shadowHeight = std::max(shadowMapRenderTarget_->GetHeight(), kMinShadowMapSize);
			shadowMapRenderTarget_->Resize(shadowWidth, shadowHeight);
		}
	}

	void DirectXCommon::SetShadowMapSize(uint32_t width, uint32_t height)
	{
		width = std::max(width, kMinShadowMapSize);
		height = std::max(height, kMinShadowMapSize);
		if (!shadowMapRenderTarget_) return;
		WaitForGpuIdle();
		shadowMapRenderTarget_->Resize(width, height);
	}

	void DirectXCommon::WaitForGpuIdle()
	{
		if (!fenceManager_ || !commandManager_) return;
		fenceManager_->Signal(commandManager_->GetCommandQueue());
		fenceManager_->Wait();
	}

	ComPtr<ID3D12Resource> DirectXCommon::GetBackBuffer(uint32_t index)
	{
		ComPtr<ID3D12Resource> backBuffer;
		const HRESULT hr = swapChain_->GetSwapChain()->GetBuffer(index, IID_PPV_ARGS(&backBuffer));
		assert(SUCCEEDED(hr));
		return backBuffer;
	}

	void DirectXCommon::DebugLayer()
	{
#ifdef _DEBUG
		ComPtr<ID3D12Debug1> debugController = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			debugController->SetEnableGPUBasedValidation(FALSE);
		}
#endif
	}

	void DirectXCommon::ErrorWarning()
	{
#ifdef _DEBUG
		ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
		if (SUCCEEDED(GetDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			D3D12_MESSAGE_ID denyIds[] = { D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
			D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
			D3D12_INFO_QUEUE_FILTER filter{};
			filter.DenyList.NumIDs = _countof(denyIds);
			filter.DenyList.pIDList = denyIds;
			filter.DenyList.NumSeverities = _countof(severities);
			filter.DenyList.pSeverityList = severities;
			infoQueue->PushStorageFilter(&filter);
		}
#endif
	}

} // namespace Ken4lowEngine
