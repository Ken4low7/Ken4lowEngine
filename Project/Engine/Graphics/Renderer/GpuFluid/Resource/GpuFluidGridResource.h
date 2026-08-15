#pragma once

#include "../Data/GpuFluidTypes.h"
#include <DX12Include.h>

#include <array>
#include <cstdint>
#include <string>

namespace Ken4lowEngine
{

struct GpuFluidTexture2D
{
	ComPtr<ID3D12Resource> resource;
	uint32_t srvIndex = UINT32_MAX;
	uint32_t uavIndex = UINT32_MAX;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;

	[[nodiscard]] bool IsValid() const
	{
		return resource != nullptr && srvIndex != UINT32_MAX && uavIndex != UINT32_MAX;
	}
};

struct GpuFluidPingPongField
{
	std::array<GpuFluidTexture2D, 2> textures{};
	uint32_t readIndex = 0;

	GpuFluidTexture2D& Read() { return textures[readIndex]; }
	const GpuFluidTexture2D& Read() const { return textures[readIndex]; }
	GpuFluidTexture2D& Write() { return textures[readIndex ^ 1u]; }
	const GpuFluidTexture2D& Write() const { return textures[readIndex ^ 1u]; }
	void Swap() { readIndex ^= 1u; }
	void Reset() { readIndex = 0; }
};

class GpuFluidGridResource
{
public:
	GpuFluidGridResource() = default;
	~GpuFluidGridResource();

	GpuFluidGridResource(const GpuFluidGridResource&) = delete;
	GpuFluidGridResource& operator=(const GpuFluidGridResource&) = delete;

	bool Initialize(const GpuFluidGridDesc& gridDesc);
	void Finalize();

	[[nodiscard]] bool IsInitialized() const { return initialized_; }
	[[nodiscard]] const GpuFluidGridDesc& GetGridDesc() const { return gridDesc_; }
	[[nodiscard]] uint64_t GetApproximateGpuMemoryBytes() const;

	GpuFluidPingPongField& GetVelocity() { return velocity_; }
	GpuFluidPingPongField& GetPressure() { return pressure_; }
	GpuFluidPingPongField& GetDensity() { return density_; }
	GpuFluidPingPongField& GetTemperature() { return temperature_; }
	GpuFluidTexture2D& GetDivergence() { return divergence_; }
	GpuFluidTexture2D& GetObstacle() { return obstacle_; }

	const GpuFluidPingPongField& GetVelocity() const { return velocity_; }
	const GpuFluidPingPongField& GetPressure() const { return pressure_; }
	const GpuFluidPingPongField& GetDensity() const { return density_; }
	const GpuFluidPingPongField& GetTemperature() const { return temperature_; }
	const GpuFluidTexture2D& GetDivergence() const { return divergence_; }
	const GpuFluidTexture2D& GetObstacle() const { return obstacle_; }

	static void Transition(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidTexture2D& texture,
		D3D12_RESOURCE_STATES newState);
	static void InsertUavBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource);

private:
	bool CreateTexture(
		GpuFluidTexture2D& texture,
		DXGI_FORMAT format,
		const std::wstring& debugName);
	bool CreatePingPongField(
		GpuFluidPingPongField& field,
		DXGI_FORMAT format,
		const std::wstring& debugName);
	void ReleaseTexture(GpuFluidTexture2D& texture);
	void ReleasePingPongField(GpuFluidPingPongField& field);

private:
	GpuFluidGridDesc gridDesc_{};
	GpuFluidPingPongField velocity_{};
	GpuFluidPingPongField pressure_{};
	GpuFluidPingPongField density_{};
	GpuFluidPingPongField temperature_{};
	GpuFluidTexture2D divergence_{};
	GpuFluidTexture2D obstacle_{};
	bool initialized_ = false;
};

} // namespace Ken4lowEngine
