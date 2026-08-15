#include "GpuFluidGridResource.h"

#include <DirectXCommon.h>
#include <SRVManager.h>
#include <UAVManager.h>

namespace Ken4lowEngine
{

GpuFluidGridResource::~GpuFluidGridResource()
{
	Finalize();
}

bool GpuFluidGridResource::Initialize(const GpuFluidGridDesc& gridDesc)
{
	if (!gridDesc.IsValid())
	{
		return false;
	}

	Finalize();
	gridDesc_ = gridDesc;

	// 各シミュレーションPassが同じRead/Write契約を使えるよう、移流対象はすべてping-pong化する。
	const bool created =
		CreatePingPongField(velocity_, DXGI_FORMAT_R16G16_FLOAT, L"GpuFluid.Velocity") &&
		CreatePingPongField(pressure_, DXGI_FORMAT_R16_FLOAT, L"GpuFluid.Pressure") &&
		CreateTexture(divergence_, DXGI_FORMAT_R16_FLOAT, L"GpuFluid.Divergence") &&
		CreatePingPongField(density_, DXGI_FORMAT_R16_FLOAT, L"GpuFluid.Density") &&
		CreatePingPongField(temperature_, DXGI_FORMAT_R16_FLOAT, L"GpuFluid.Temperature") &&
		CreateTexture(obstacle_, DXGI_FORMAT_R8_UINT, L"GpuFluid.Obstacle");

	if (!created)
	{
		Finalize();
		return false;
	}

	initialized_ = true;
	return true;
}

void GpuFluidGridResource::Finalize()
{
	ReleasePingPongField(velocity_);
	ReleasePingPongField(pressure_);
	ReleaseTexture(divergence_);
	ReleasePingPongField(density_);
	ReleasePingPongField(temperature_);
	ReleaseTexture(obstacle_);
	gridDesc_ = {};
	initialized_ = false;
}

uint64_t GpuFluidGridResource::GetApproximateGpuMemoryBytes() const
{
	if (!initialized_)
	{
		return 0;
	}

	const uint64_t pixelCount =
		static_cast<uint64_t>(gridDesc_.width) * static_cast<uint64_t>(gridDesc_.height);
	constexpr uint64_t kBytesPerPixel =
		(4ull * 2ull) + // Velocity RG16F x2
		(2ull * 2ull) + // Pressure R16F x2
		2ull +          // Divergence R16F
		(2ull * 2ull) + // Density R16F x2
		(2ull * 2ull) + // Temperature R16F x2
		1ull;           // Obstacle R8_UINT
	return pixelCount * kBytesPerPixel;
}

void GpuFluidGridResource::Transition(
	ID3D12GraphicsCommandList* commandList,
	GpuFluidTexture2D& texture,
	D3D12_RESOURCE_STATES newState)
{
	if (commandList == nullptr || !texture.resource || texture.currentState == newState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = texture.resource.Get();
	barrier.Transition.StateBefore = texture.currentState;
	barrier.Transition.StateAfter = newState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
	texture.currentState = newState;
}

void GpuFluidGridResource::InsertUavBarrier(
	ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* resource)
{
	if (commandList == nullptr || resource == nullptr)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = resource;
	commandList->ResourceBarrier(1, &barrier);
}

bool GpuFluidGridResource::CreateTexture(
	GpuFluidTexture2D& texture,
	DXGI_FORMAT format,
	const std::wstring& debugName)
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	if (dxCommon == nullptr || dxCommon->GetDevice() == nullptr)
	{
		return false;
	}

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = gridDesc_.width;
	resourceDesc.Height = gridDesc_.height;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	const HRESULT hr = dxCommon->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&texture.resource));
	if (FAILED(hr))
	{
		return false;
	}

	texture.resource->SetName(debugName.c_str());
	texture.format = format;
	texture.currentState = D3D12_RESOURCE_STATE_COMMON;

	texture.srvIndex = SRVManager::GetInstance()->Allocate();
	SRVManager::GetInstance()->CreateSRVForTexture2D(
		texture.srvIndex,
		texture.resource.Get(),
		format,
		1);

	texture.uavIndex = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForTexture2D(
		texture.uavIndex,
		texture.resource.Get(),
		format,
		0);
	return true;
}

bool GpuFluidGridResource::CreatePingPongField(
	GpuFluidPingPongField& field,
	DXGI_FORMAT format,
	const std::wstring& debugName)
{
	field.Reset();
	if (!CreateTexture(field.textures[0], format, debugName + L".A"))
	{
		return false;
	}
	if (!CreateTexture(field.textures[1], format, debugName + L".B"))
	{
		ReleaseTexture(field.textures[0]);
		return false;
	}
	return true;
}

void GpuFluidGridResource::ReleaseTexture(GpuFluidTexture2D& texture)
{
	if (texture.srvIndex != UINT32_MAX)
	{
		SRVManager::GetInstance()->Free(texture.srvIndex);
	}
	if (texture.uavIndex != UINT32_MAX)
	{
		UAVManager::GetInstance()->Free(texture.uavIndex);
	}

	texture.resource.Reset();
	texture.srvIndex = UINT32_MAX;
	texture.uavIndex = UINT32_MAX;
	texture.format = DXGI_FORMAT_UNKNOWN;
	texture.currentState = D3D12_RESOURCE_STATE_COMMON;
}

void GpuFluidGridResource::ReleasePingPongField(GpuFluidPingPongField& field)
{
	ReleaseTexture(field.textures[0]);
	ReleaseTexture(field.textures[1]);
	field.Reset();
}

} // namespace Ken4lowEngine
