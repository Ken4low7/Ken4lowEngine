#include "RenderDepthContext.h"

#include <CameraManager.h>
#include <DirectXCommon.h>
#include <DSVManager.h>
#include <SRVManager.h>

#include <algorithm>

namespace Ken4lowEngine
{

RenderDepthContext* RenderDepthContext::GetInstance()
{
	static RenderDepthContext instance;
	return &instance;
}

ComPtr<ID3D12Resource> RenderDepthContext::CreateShaderReadableDepth24(
	ID3D12Device* device,
	uint32_t width,
	uint32_t height,
	float clearDepth)
{
	if (device == nullptr || width == 0 || height == 0)
	{
		return nullptr;
	}

	D3D12_RESOURCE_DESC depthDesc{};
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.DepthOrArraySize = 1;
	depthDesc.MipLevels = 1;
	depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = clearDepth;
	clearValue.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	ComPtr<ID3D12Resource> resource;
	const HRESULT result = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&resource));
	return SUCCEEDED(result) ? resource : nullptr;
}

void RenderDepthContext::SetDefaultTarget(const RenderDepthBindingDesc& desc)
{
	if (!desc.IsValid())
	{
		ClearDefaultTarget();
		return;
	}

	if (!EnsureAttachment(desc.resource))
	{
		ClearDefaultTarget();
		return;
	}

	defaultTarget_ = desc;
	RefreshStats();
}

void RenderDepthContext::ClearDefaultTarget(ID3D12Resource* expectedResource)
{
	if (expectedResource != nullptr && defaultTarget_.resource != expectedResource)
	{
		return;
	}
	if (prepared_ && preparedBinding_.resource == defaultTarget_.resource && overrides_.empty())
	{
		RestoreDepthWrite();
	}
	defaultTarget_ = {};
}

bool RenderDepthContext::PushOverride(const RenderDepthBindingDesc& desc)
{
	if (!desc.IsValid() || !EnsureAttachment(desc.resource))
	{
		return false;
	}

	// Transparent実行中に描画先を差し替えるのは危険なので、前Targetを必ずWritableへ戻してから積む。
	if (prepared_)
	{
		RestoreDepthWrite();
	}
	overrides_.push_back(desc);
	++stats_.overridePushCount;
	RefreshStats();
	return true;
}

void RenderDepthContext::PopOverride()
{
	if (overrides_.empty())
	{
		return;
	}
	if (prepared_ && preparedBinding_.resource == overrides_.back().resource)
	{
		RestoreDepthWrite();
	}
	overrides_.pop_back();
}

void RenderDepthContext::ReleaseAttachment(ID3D12Resource* resource)
{
	if (resource == nullptr)
	{
		return;
	}

	if (prepared_ && preparedBinding_.resource == resource)
	{
		RestoreDepthWrite();
	}
	if (defaultTarget_.resource == resource)
	{
		defaultTarget_ = {};
	}
	overrides_.erase(
		std::remove_if(overrides_.begin(), overrides_.end(),
			[resource](const RenderDepthBindingDesc& binding)
			{
				return binding.resource == resource;
			}),
		overrides_.end());

	auto found = attachments_.find(resource);
	if (found == attachments_.end())
	{
		RefreshStats();
		return;
	}

	if (found->second.readOnlyDsvIndex != UINT32_MAX)
	{
		DSVManager::GetInstance()->Free(found->second.readOnlyDsvIndex);
	}
	if (found->second.srvIndex != UINT32_MAX)
	{
		SRVManager::GetInstance()->Free(found->second.srvIndex);
	}
	attachments_.erase(found);
	RefreshStats();
}

bool RenderDepthContext::PrepareForShaderRead()
{
	CameraManager* cameraManager = CameraManager::GetInstance();
	if (cameraManager != nullptr && cameraManager->HasRenderViewOverride() && overrides_.empty())
	{
		// Reflection等の一時Viewに専用Depthが未登録なら、Main Depthを誤バインドせずVolume側だけ安全に抑止する。
		++stats_.failedPrepareCount;
		RefreshStats();
		return false;
	}

	const RenderDepthBindingDesc* binding = GetActiveBinding();
	if (binding == nullptr || !binding->IsValid())
	{
		++stats_.failedPrepareCount;
		RefreshStats();
		return false;
	}

	if (prepared_)
	{
		return preparedBinding_.resource == binding->resource;
	}
	if (!EnsureAttachment(binding->resource))
	{
		++stats_.failedPrepareCount;
		RefreshStats();
		return false;
	}

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	if (dxCommon == nullptr || dxCommon->GetCommandManager() == nullptr)
	{
		++stats_.failedPrepareCount;
		RefreshStats();
		return false;
	}
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		++stats_.failedPrepareCount;
		RefreshStats();
		return false;
	}

	const Attachment* attachment = FindAttachment(binding->resource);
	if (attachment == nullptr)
	{
		++stats_.failedPrepareCount;
		RefreshStats();
		return false;
	}

	const D3D12_RESOURCE_STATES readableState =
		D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon->ResourceTransition(
		binding->resource,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		readableState);

	const D3D12_CPU_DESCRIPTOR_HANDLE readOnlyDsv =
		DSVManager::GetInstance()->GetCPUDescriptorHandle(attachment->readOnlyDsvIndex);
	commandList->OMSetRenderTargets(1, &binding->colorRtv, false, &readOnlyDsv);

	preparedBinding_ = *binding;
	prepared_ = true;
	++stats_.prepareCount;
	RefreshStats();
	return true;
}

void RenderDepthContext::RestoreDepthWrite()
{
	if (!prepared_ || preparedBinding_.resource == nullptr)
	{
		return;
	}

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	if (dxCommon != nullptr && dxCommon->GetCommandManager() != nullptr)
	{
		ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandManager()->GetCommandList();
		if (commandList != nullptr)
		{
			const D3D12_RESOURCE_STATES readableState =
				D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			dxCommon->ResourceTransition(
				preparedBinding_.resource,
				readableState,
				D3D12_RESOURCE_STATE_DEPTH_WRITE);
			commandList->OMSetRenderTargets(
				1,
				&preparedBinding_.colorRtv,
				false,
				&preparedBinding_.writableDsv);
		}
	}

	preparedBinding_ = {};
	prepared_ = false;
	++stats_.restoreCount;
	RefreshStats();
}

uint32_t RenderDepthContext::GetActiveDepthSrvIndex() const
{
	if (!prepared_ || preparedBinding_.resource == nullptr)
	{
		return UINT32_MAX;
	}
	const Attachment* attachment = FindAttachment(preparedBinding_.resource);
	return attachment != nullptr ? attachment->srvIndex : UINT32_MAX;
}

D3D12_VIEWPORT RenderDepthContext::GetActiveViewport() const
{
	return prepared_ ? preparedBinding_.viewport : D3D12_VIEWPORT{};
}

float RenderDepthContext::GetActiveClearDepth() const
{
	return prepared_ ? preparedBinding_.clearDepth : 1.0f;
}

bool RenderDepthContext::EnsureAttachment(ID3D12Resource* resource)
{
	if (resource == nullptr)
	{
		return false;
	}
	if (attachments_.find(resource) != attachments_.end())
	{
		return true;
	}

	const D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
	if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resourceDesc.Format != DXGI_FORMAT_R24G8_TYPELESS)
	{
		return false;
	}

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	if (dxCommon == nullptr || dxCommon->GetDevice() == nullptr)
	{
		return false;
	}

	DSVManager* dsvManager = DSVManager::GetInstance();
	SRVManager* srvManager = SRVManager::GetInstance();
	Attachment attachment{};
	attachment.readOnlyDsvIndex = dsvManager->Allocate();
	try
	{
		attachment.srvIndex = srvManager->Allocate();
	}
	catch (...)
	{
		dsvManager->Free(attachment.readOnlyDsvIndex);
		throw;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
	dsvDesc.Texture2D.MipSlice = 0;
	dxCommon->GetDevice()->CreateDepthStencilView(
		resource,
		&dsvDesc,
		dsvManager->GetCPUDescriptorHandle(attachment.readOnlyDsvIndex));

	// Depth resourceはR24G8_TYPELESSで所有し、SRVだけR24_UNORM_X8_TYPELESSとして読む。
	srvManager->CreateSRVForDepthBuffer(attachment.srvIndex, resource);
	attachments_.emplace(resource, attachment);
	RefreshStats();
	return true;
}

const RenderDepthBindingDesc* RenderDepthContext::GetActiveBinding() const
{
	if (!overrides_.empty())
	{
		return &overrides_.back();
	}
	return defaultTarget_.IsValid() ? &defaultTarget_ : nullptr;
}

RenderDepthContext::Attachment* RenderDepthContext::FindAttachment(ID3D12Resource* resource)
{
	auto found = attachments_.find(resource);
	return found != attachments_.end() ? &found->second : nullptr;
}

const RenderDepthContext::Attachment* RenderDepthContext::FindAttachment(ID3D12Resource* resource) const
{
	auto found = attachments_.find(resource);
	return found != attachments_.end() ? &found->second : nullptr;
}

void RenderDepthContext::RefreshStats()
{
	stats_.attachmentCount = static_cast<uint32_t>(attachments_.size());
	stats_.shaderReadPrepared = prepared_;
}

} // namespace Ken4lowEngine
