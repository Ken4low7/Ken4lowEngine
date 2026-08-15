#include "GpuVolumetricFluidGridResource.h"

#include <DirectXCommon.h>
#include <SRVManager.h>
#include <UAVManager.h>

namespace Ken4lowEngine
{

GpuVolumetricFluidGridResource::~GpuVolumetricFluidGridResource()
{
	Finalize();
}

bool GpuVolumetricFluidGridResource::Initialize(const GpuVolumetricFluidGridDesc& gridDesc)
{
	if (!gridDesc.IsValid())
	{
		return false;
	}

	Finalize();
	gridDesc_ = gridDesc;

	const bool created =
		// 3成分velocityはFilter可能なRGBA16Fへ格納し、wは将来の補助量用に予約する。
		CreatePingPongField(velocity_, DXGI_FORMAT_R16G16B16A16_FLOAT, L"GpuVolumetricFluid.Velocity") &&
		CreatePingPongField(pressure_, DXGI_FORMAT_R16_FLOAT, L"GpuVolumetricFluid.Pressure") &&
		CreateTexture(divergence_, DXGI_FORMAT_R16_FLOAT, L"GpuVolumetricFluid.Divergence") &&
		CreatePingPongField(density_, DXGI_FORMAT_R16_FLOAT, L"GpuVolumetricFluid.Density") &&
		CreatePingPongField(temperature_, DXGI_FORMAT_R16_FLOAT, L"GpuVolumetricFluid.Temperature") &&
		CreateTexture(vorticity_, DXGI_FORMAT_R16G16B16A16_FLOAT, L"GpuVolumetricFluid.Vorticity") &&
		CreateTexture(obstacle_, DXGI_FORMAT_R8_UINT, L"GpuVolumetricFluid.Obstacle");

	if (!created)
	{
		Finalize();
		return false;
	}

	initialized_ = true;
	return true;
}

void GpuVolumetricFluidGridResource::Finalize()
{
	ReleasePingPongField(velocity_);
	ReleasePingPongField(pressure_);
	ReleaseTexture(divergence_);
	ReleasePingPongField(density_);
	ReleasePingPongField(temperature_);
	ReleaseTexture(vorticity_);
	ReleaseTexture(obstacle_);
	gridDesc_ = {};
	initialized_ = false;
}

uint64_t GpuVolumetricFluidGridResource::GetApproximateGpuMemoryBytes() const
{
	if (!initialized_)
	{
		return 0;
	}

	constexpr uint64_t kBytesPerVoxel =
		(8ull * 2ull) + // Velocity RGBA16F x2
		(2ull * 2ull) + // Pressure R16F x2
		2ull +          // Divergence R16F
		(2ull * 2ull) + // Density R16F x2
		(2ull * 2ull) + // Temperature R16F x2
		8ull +          // Vorticity RGBA16F
		1ull;           // Obstacle R8_UINT
	return gridDesc_.GetVoxelCount() * kBytesPerVoxel;
}

void GpuVolumetricFluidGridResource::Transition(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidTexture3D& texture,
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

void GpuVolumetricFluidGridResource::InsertUavBarrier(
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

bool GpuVolumetricFluidGridResource::CreateTexture(
	GpuVolumetricFluidTexture3D& texture,
	DXGI_FORMAT format,
	const std::wstring& debugName)
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	if (dxCommon == nullptr || dxCommon->GetDevice() == nullptr)
	{
		return false;
	}

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	resourceDesc.Width = gridDesc_.width;
	resourceDesc.Height = gridDesc_.height;
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(gridDesc_.depth);
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
	SRVManager::GetInstance()->CreateSRVForTexture3D(texture.srvIndex, texture.resource.Get(), format, 1);

	UAVManager* uavManager = UAVManager::GetInstance();
	texture.computeSrvIndex = uavManager->Allocate();
	uavManager->CreateSRVForTexture3DOnThisHeap(texture.computeSrvIndex, texture.resource.Get(), format, 1);

	texture.uavIndex = uavManager->Allocate();
	uavManager->CreateUAVForTexture3D(texture.uavIndex, texture.resource.Get(), format, gridDesc_.depth, 0);
	return true;
}

bool GpuVolumetricFluidGridResource::CreatePingPongField(
	GpuVolumetricFluidPingPongField& field,
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

void GpuVolumetricFluidGridResource::ReleaseTexture(GpuVolumetricFluidTexture3D& texture)
{
	if (texture.srvIndex != UINT32_MAX)
	{
		SRVManager::GetInstance()->Free(texture.srvIndex);
	}

	UAVManager* uavManager = UAVManager::GetInstance();
	if (texture.computeSrvIndex != UINT32_MAX)
	{
		uavManager->Free(texture.computeSrvIndex);
	}
	if (texture.uavIndex != UINT32_MAX)
	{
		uavManager->Free(texture.uavIndex);
	}

	texture.resource.Reset();
	texture.srvIndex = UINT32_MAX;
	texture.computeSrvIndex = UINT32_MAX;
	texture.uavIndex = UINT32_MAX;
	texture.format = DXGI_FORMAT_UNKNOWN;
	texture.currentState = D3D12_RESOURCE_STATE_COMMON;
}

void GpuVolumetricFluidGridResource::ReleasePingPongField(GpuVolumetricFluidPingPongField& field)
{
	ReleaseTexture(field.textures[0]);
	ReleaseTexture(field.textures[1]);
	field.Reset();
}

} // namespace Ken4lowEngine
