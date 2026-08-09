#define NOMINMAX
#include "PostEffectRenderTargetManager.h"

#include "DirectXCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "RTVManager.h"
#include "DSVManager.h"
#include "SRVManager.h"
#include "UAVManager.h"
#include "LogString.h"

#include <cassert>
#include <string>

namespace Ken4lowEngine
{
	void PostEffectRenderTargetManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;
		width_ = kDefaultWidth;
		height_ = kDefaultHeight;
		renderTargets_.resize(2); // RT0をGameViewport、RT1をping-pong出力として従来構成を維持する。
		renderTargets_[0].debugName = L"SceneRenderTarget_GameViewportRenderTarget";
		renderTargets_[1].debugName = L"PostEffectRenderTarget";

		AllocateDescriptorsAndResources();
		SetViewportAndScissorRect(width_, height_);
		UpdateCameraAspectRatio();
	}

	void PostEffectRenderTargetManager::Finalize()
	{
		for (PostEffectRenderTarget& renderTarget : renderTargets_)
		{
			renderTarget.resource.Reset();
			renderTarget.rtvHandle = {};
			renderTarget.currentState = PostEffectRenderTarget::kInitialState;
		}
		renderTargets_.clear();

		// Descriptor indexの解放可否は各Manager全体の寿命設計に依存するため、
		// Phase 4では旧PostEffectManagerと同じくResourceだけを破棄する。
		depthResource_.Reset();
		dsvHandle_ = {};
		depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		depthDsvIndex_ = UINT32_MAX;
		depthSrvIndex_ = UINT32_MAX;
		dxCommon_ = nullptr;
	}

	void PostEffectRenderTargetManager::Resize(uint32_t, uint32_t)
	{
		const uint32_t width = kDefaultWidth;
		const uint32_t height = kDefaultHeight;
		if (width_ == width && height_ == height && depthResource_)
		{
			return; // Main Viewportの表示サイズに関係なく内部解像度は従来どおり固定する。
		}

		width_ = width;
		height_ = height;
		SetViewportAndScissorRect(width_, height_);
		UpdateCameraAspectRatio();

		for (PostEffectRenderTarget& renderTarget : renderTargets_)
		{
			renderTarget.resource.Reset();
			renderTarget.resource = CreateRenderTextureResource(width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor_);
			renderTarget.resource->SetName(renderTarget.debugName);
			RTVManager::GetInstance()->CreateRTVForTexture2D(renderTarget.rtvIndex, renderTarget.resource.Get());
			renderTarget.rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(renderTarget.rtvIndex);
			SRVManager::GetInstance()->CreateSRVForTexture2D(renderTarget.srvIndex, renderTarget.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
			UAVManager::GetInstance()->CreateUAVForTexture2D(renderTarget.uavIndex, renderTarget.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 0);
			UAVManager::GetInstance()->CreateSRVForTexture2DOnThisHeap(renderTarget.srvIndexOnUavHeap, renderTarget.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
			renderTarget.currentState = PostEffectRenderTarget::kInitialState;
		}

		depthResource_.Reset();
		depthResource_ = CreateDepthBufferResource(width_, height_);
		depthResource_->SetName(L"PostEffectManager DepthBuffer");
		DSVManager::GetInstance()->CreateDSVForTexture2D(depthDsvIndex_, depthResource_.Get());
		dsvHandle_ = DSVManager::GetInstance()->GetCPUDescriptorHandle(depthDsvIndex_);
		SRVManager::GetInstance()->CreateSRVForTexture2D(depthSrvIndex_, depthResource_.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
		depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	uint32_t PostEffectRenderTargetManager::GetGameRenderTargetSrvIndex() const
	{
		return renderTargets_.empty() ? UINT32_MAX : renderTargets_.front().srvIndex;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE PostEffectRenderTargetManager::GetGameRenderTargetSrvHandleGPU() const
	{
		const uint32_t srvIndex = GetGameRenderTargetSrvIndex();
		return srvIndex == UINT32_MAX ? D3D12_GPU_DESCRIPTOR_HANDLE{} : SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);
	}

	bool PostEffectRenderTargetManager::ValidateForDraw(const PostEffectRenderTarget& renderTarget, bool requireDepth, const char* caller) const
	{
		bool valid = true;
		std::string message = "[PostEffectManager] Invalid render target before ";
		message += caller != nullptr ? caller : "unknown";
		message += ": ";

		if (!renderTarget.resource) { message += "resource=null "; valid = false; }
		if (renderTarget.rtvHandle.ptr == 0) { message += "rtvHandle=0 "; valid = false; }
		if (requireDepth && !depthResource_) { message += "depthResource=null "; valid = false; }
		if (requireDepth && dsvHandle_.ptr == 0) { message += "dsvHandle=0 "; valid = false; }

		if (!valid)
		{
			message += "rtvIndex=" + std::to_string(renderTarget.rtvIndex);
			message += " srvIndex=" + std::to_string(renderTarget.srvIndex);
			message += " uavIndex=" + std::to_string(renderTarget.uavIndex);
			message += " dsvSrvIndex=" + std::to_string(depthSrvIndex_) + "\n";
			Log(message);
		}
		return valid;
	}

	ComPtr<ID3D12Resource> PostEffectRenderTargetManager::CreateRenderTextureResource(
		uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
	{
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = format;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = format;
		clearValue.Color[0] = clearColor.x;
		clearValue.Color[1] = clearColor.y;
		clearValue.Color[2] = clearColor.z;
		clearValue.Color[3] = clearColor.w;

		ComPtr<ID3D12Resource> resource;
		const HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			PostEffectRenderTarget::kInitialState, &clearValue, IID_PPV_ARGS(&resource));
		assert(SUCCEEDED(hr));
		return resource;
	}

	ComPtr<ID3D12Resource> PostEffectRenderTargetManager::CreateDepthBufferResource(uint32_t width, uint32_t height)
	{
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = width;
		desc.Height = height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.SampleDesc.Count = 1;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clearValue.DepthStencil = { 1.0f, 0 };
		D3D12_HEAP_PROPERTIES heapProperties = {
			D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

		ComPtr<ID3D12Resource> depthResource;
		const HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&depthResource));
		assert(SUCCEEDED(hr));
		return depthResource;
	}

	void PostEffectRenderTargetManager::AllocateDescriptorsAndResources()
	{
		for (PostEffectRenderTarget& renderTarget : renderTargets_)
		{
			renderTarget.resource = CreateRenderTextureResource(width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor_);
			renderTarget.resource->SetName(renderTarget.debugName);
			renderTarget.currentState = PostEffectRenderTarget::kInitialState;

			renderTarget.rtvIndex = RTVManager::GetInstance()->Allocate();
			RTVManager::GetInstance()->CreateRTVForTexture2D(renderTarget.rtvIndex, renderTarget.resource.Get());
			renderTarget.rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(renderTarget.rtvIndex);
			renderTarget.srvIndex = SRVManager::GetInstance()->Allocate();
			SRVManager::GetInstance()->CreateSRVForTexture2D(renderTarget.srvIndex, renderTarget.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
			renderTarget.uavIndex = UAVManager::GetInstance()->Allocate();
			UAVManager::GetInstance()->CreateUAVForTexture2D(renderTarget.uavIndex, renderTarget.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 0);
			renderTarget.srvIndexOnUavHeap = UAVManager::GetInstance()->Allocate();
			UAVManager::GetInstance()->CreateSRVForTexture2DOnThisHeap(renderTarget.srvIndexOnUavHeap, renderTarget.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
		}

		depthResource_ = CreateDepthBufferResource(width_, height_);
		depthResource_->SetName(L"PostEffectManager DepthBuffer");
		depthDsvIndex_ = DSVManager::GetInstance()->Allocate();
		DSVManager::GetInstance()->CreateDSVForTexture2D(depthDsvIndex_, depthResource_.Get());
		dsvHandle_ = DSVManager::GetInstance()->GetCPUDescriptorHandle(depthDsvIndex_);
		depthSrvIndex_ = SRVManager::GetInstance()->Allocate();
		SRVManager::GetInstance()->CreateSRVForTexture2D(depthSrvIndex_, depthResource_.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
		depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	void PostEffectRenderTargetManager::SetViewportAndScissorRect(uint32_t width, uint32_t height)
	{
		viewport_ = D3D12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		scissorRect_ = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	}

	void PostEffectRenderTargetManager::UpdateCameraAspectRatio()
	{
		CameraManager* cameraManager = CameraManager::GetInstance();
		if (Camera* mainCamera = cameraManager->GetMainCamera()) { mainCamera->SetAspectRatio(GameViewportConstants::Aspect); }
		if (DebugCamera* debugCamera = cameraManager->GetDebugCamera()) { debugCamera->SetAspectRatio(GameViewportConstants::Aspect); }
	}
}
