#define NOMINMAX
#include "ShadowMapArrayRenderTarget.h"

#include "DirectXCommon.h"
#include "DSVManager.h"
#include "SRVManager.h"

#include <algorithm>
#include <cassert>

namespace Ken4lowEngine
{
	void ShadowMapArrayRenderTarget::Initialize(DirectXCommon* dxCommon, uint32_t width, uint32_t height, uint32_t arraySize, ShadowArrayViewType viewType, const wchar_t* debugName)
	{
		dxCommon_ = dxCommon;
		width_ = std::max(width, 1u);
		height_ = std::max(height, 1u);
		arraySize_ = std::max(arraySize, 1u);
		viewType_ = viewType;
		debugName_ = debugName != nullptr ? debugName : L"ShadowMapArrayRenderTarget";
		CreateResource();
		CreateDescriptors();
		UpdateViewport();
	}

	void ShadowMapArrayRenderTarget::Finalize()
	{
		ReleaseDescriptors();
		resource_.Reset();
		dxCommon_ = nullptr;
	}

	void ShadowMapArrayRenderTarget::Resize(uint32_t width, uint32_t height)
	{
		width = std::max(width, 1u);
		height = std::max(height, 1u);
		if (width_ == width && height_ == height && resource_)
		{
			return;
		}
		width_ = width;
		height_ = height;
		resource_.Reset();
		CreateResource();
		CreateDescriptors();
		UpdateViewport();
	}

	void ShadowMapArrayRenderTarget::BeginSlice(ID3D12GraphicsCommandList* commandList, uint32_t sliceIndex)
	{
		assert(commandList != nullptr);
		assert(sliceIndex < dsvIndices_.size());
		TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->RSSetViewports(1, &viewport_);
		commandList->RSSetScissorRects(1, &scissorRect_);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(dsvIndices_[sliceIndex]);
		commandList->OMSetRenderTargets(0, nullptr, false, &dsvHandle);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}

	void ShadowMapArrayRenderTarget::End(ID3D12GraphicsCommandList*)
	{
		TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE ShadowMapArrayRenderTarget::GetSrvHandleGPU() const
	{
		return SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
	}

	void ShadowMapArrayRenderTarget::CreateResource()
	{
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = width_;
		desc.Height = height_;
		desc.DepthOrArraySize = static_cast<UINT16>(arraySize_);
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil = { 1.0f, 0 };
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
		const HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&resource_));
		assert(SUCCEEDED(hr));
		resource_->SetName(debugName_);
		resourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	void ShadowMapArrayRenderTarget::CreateDescriptors()
	{
		auto* dsvManager = DSVManager::GetInstance();
		auto* srvManager = SRVManager::GetInstance();
		if (dsvIndices_.empty())
		{
			dsvIndices_.resize(arraySize_);
			for (uint32_t& index : dsvIndices_) { index = dsvManager->Allocate(); }
		}
		for (uint32_t slice = 0; slice < arraySize_; ++slice)
		{
			dsvManager->CreateDSVForShadowMapArraySlice(dsvIndices_[slice], resource_.Get(), slice);
		}
		if (srvIndex_ == UINT32_MAX) { srvIndex_ = srvManager->Allocate(); }
		if (viewType_ == ShadowArrayViewType::TextureCube)
		{
			srvManager->CreateSRVForShadowCube(srvIndex_, resource_.Get());
		}
		else
		{
			srvManager->CreateSRVForShadowMapArray(srvIndex_, resource_.Get(), arraySize_);
		}
	}

	void ShadowMapArrayRenderTarget::ReleaseDescriptors()
	{
		for (uint32_t index : dsvIndices_) { DSVManager::GetInstance()->Free(index); }
		dsvIndices_.clear();
		if (srvIndex_ != UINT32_MAX)
		{
			SRVManager::GetInstance()->Free(srvIndex_);
			srvIndex_ = UINT32_MAX;
		}
	}

	void ShadowMapArrayRenderTarget::TransitionTo(D3D12_RESOURCE_STATES nextState)
	{
		if (!resource_ || resourceState_ == nextState) { return; }
		dxCommon_->ResourceTransition(resource_.Get(), resourceState_, nextState);
		resourceState_ = nextState;
	}

	void ShadowMapArrayRenderTarget::UpdateViewport()
	{
		viewport_ = D3D12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f);
		scissorRect_ = { 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
	}
}
