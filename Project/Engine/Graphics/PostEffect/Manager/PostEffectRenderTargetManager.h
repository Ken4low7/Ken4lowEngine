#pragma once

#include "DX12Include.h"
#include "GameViewportConstants.h"
#include "Vector4.h"

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// <summary>PostEffectのRTV/SRV/UAVと現在ResourceStateをまとめたRenderTargetです。</summary>
	struct PostEffectRenderTarget
	{
		static constexpr D3D12_RESOURCE_STATES kInitialState = D3D12_RESOURCE_STATE_COMMON;

		ComPtr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
		const wchar_t* debugName = L"PostEffectRenderTarget";
		D3D12_RESOURCE_STATES currentState = kInitialState;
		uint32_t rtvIndex = UINT32_MAX;
		uint32_t srvIndex = UINT32_MAX;
		uint32_t uavIndex = UINT32_MAX;
		uint32_t srvIndexOnUavHeap = UINT32_MAX;
		Vector4 clearColor{ 0.08f, 0.08f, 0.18f, 1.0f };
	};

	/// <summary>
	/// PostEffect用RenderTarget・DepthBuffer・Descriptor・Viewportの生成と寿命だけを管理します。<br/>
	/// ResourceBarrier、Effect Apply、BackBufferコピーはPostEffectExecutorへ委譲します。
	/// </summary>
	class PostEffectRenderTargetManager
	{
	public:
		static constexpr uint32_t kDefaultWidth = GameViewportConstants::Width;
		static constexpr uint32_t kDefaultHeight = GameViewportConstants::Height;

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();
		void Resize(uint32_t width, uint32_t height);

		bool Empty() const { return renderTargets_.empty(); }
		std::vector<PostEffectRenderTarget>& GetRenderTargets() { return renderTargets_; }
		const std::vector<PostEffectRenderTarget>& GetRenderTargets() const { return renderTargets_; }
		PostEffectRenderTarget& GetGameRenderTarget() { return renderTargets_.front(); }
		const PostEffectRenderTarget& GetGameRenderTarget() const { return renderTargets_.front(); }

		ID3D12Resource* GetDepthResource() const { return depthResource_.Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const { return dsvHandle_; }
		uint32_t GetDepthSrvIndex() const { return depthSrvIndex_; }
		D3D12_RESOURCE_STATES GetDepthState() const { return depthState_; }
		void SetDepthState(D3D12_RESOURCE_STATES state) { depthState_ = state; }

		const D3D12_VIEWPORT& GetViewport() const { return viewport_; }
		const D3D12_RECT& GetScissorRect() const { return scissorRect_; }
		uint32_t GetWidth() const { return width_; }
		uint32_t GetHeight() const { return height_; }
		uint32_t GetGameRenderTargetSrvIndex() const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGameRenderTargetSrvHandleGPU() const;

		bool ValidateForDraw(const PostEffectRenderTarget& renderTarget, bool requireDepth, const char* caller) const;

	private:
		ComPtr<ID3D12Resource> CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);
		ComPtr<ID3D12Resource> CreateDepthBufferResource(uint32_t width, uint32_t height);
		void AllocateDescriptorsAndResources();
		void SetViewportAndScissorRect(uint32_t width, uint32_t height);
		void UpdateCameraAspectRatio();

		DirectXCommon* dxCommon_ = nullptr;
		std::vector<PostEffectRenderTarget> renderTargets_;
		ComPtr<ID3D12Resource> depthResource_;
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
		D3D12_RESOURCE_STATES depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		uint32_t depthDsvIndex_ = UINT32_MAX;
		uint32_t depthSrvIndex_ = UINT32_MAX;
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissorRect_{};
		uint32_t width_ = kDefaultWidth;
		uint32_t height_ = kDefaultHeight;
		const Vector4 clearColor_{ 0.08f, 0.08f, 0.18f, 1.0f };
	};
}
