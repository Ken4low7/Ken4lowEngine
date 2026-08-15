#pragma once

#include "../Data/GpuVolumetricFluidTypes.h"
#include <DX12Include.h>

#include <array>
#include <cstdint>
#include <string>

namespace Ken4lowEngine
{

struct GpuVolumetricFluidTexture3D
{
	ComPtr<ID3D12Resource> resource;
	uint32_t srvIndex = UINT32_MAX;
	uint32_t computeSrvIndex = UINT32_MAX;
	uint32_t uavIndex = UINT32_MAX;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;

	[[nodiscard]] bool IsValid() const
	{
		return resource != nullptr &&
			srvIndex != UINT32_MAX &&
			computeSrvIndex != UINT32_MAX &&
			uavIndex != UINT32_MAX;
	}
};

struct GpuVolumetricFluidPingPongField
{
	std::array<GpuVolumetricFluidTexture3D, 2> textures{};
	uint32_t readIndex = 0;

	GpuVolumetricFluidTexture3D& Read() { return textures[readIndex]; }
	const GpuVolumetricFluidTexture3D& Read() const { return textures[readIndex]; }
	GpuVolumetricFluidTexture3D& Write() { return textures[readIndex ^ 1u]; }
	const GpuVolumetricFluidTexture3D& Write() const { return textures[readIndex ^ 1u]; }
	void Swap() { readIndex ^= 1u; }
	void Reset() { readIndex = 0; }
};

/// Phase17 3D solver専用Texture3D field container。Phase16の2D gridとはResource寿命を分離する。
class GpuVolumetricFluidGridResource
{
public:
	GpuVolumetricFluidGridResource() = default;
	~GpuVolumetricFluidGridResource();

	GpuVolumetricFluidGridResource(const GpuVolumetricFluidGridResource&) = delete;
	GpuVolumetricFluidGridResource& operator=(const GpuVolumetricFluidGridResource&) = delete;

	bool Initialize(const GpuVolumetricFluidGridDesc& gridDesc);
	void Finalize();

	[[nodiscard]] bool IsInitialized() const { return initialized_; }
	[[nodiscard]] const GpuVolumetricFluidGridDesc& GetGridDesc() const { return gridDesc_; }
	[[nodiscard]] uint64_t GetApproximateGpuMemoryBytes() const;

	GpuVolumetricFluidPingPongField& GetVelocity() { return velocity_; }
	GpuVolumetricFluidPingPongField& GetPressure() { return pressure_; }
	GpuVolumetricFluidPingPongField& GetDensity() { return density_; }
	GpuVolumetricFluidPingPongField& GetTemperature() { return temperature_; }
	GpuVolumetricFluidTexture3D& GetDivergence() { return divergence_; }
	GpuVolumetricFluidTexture3D& GetVorticity() { return vorticity_; }
	GpuVolumetricFluidTexture3D& GetObstacle() { return obstacle_; }

	const GpuVolumetricFluidPingPongField& GetVelocity() const { return velocity_; }
	const GpuVolumetricFluidPingPongField& GetPressure() const { return pressure_; }
	const GpuVolumetricFluidPingPongField& GetDensity() const { return density_; }
	const GpuVolumetricFluidPingPongField& GetTemperature() const { return temperature_; }
	const GpuVolumetricFluidTexture3D& GetDivergence() const { return divergence_; }
	const GpuVolumetricFluidTexture3D& GetVorticity() const { return vorticity_; }
	const GpuVolumetricFluidTexture3D& GetObstacle() const { return obstacle_; }

	static void Transition(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidTexture3D& texture,
		D3D12_RESOURCE_STATES newState);
	static void InsertUavBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource);

private:
	bool CreateTexture(
		GpuVolumetricFluidTexture3D& texture,
		DXGI_FORMAT format,
		const std::wstring& debugName);
	bool CreatePingPongField(
		GpuVolumetricFluidPingPongField& field,
		DXGI_FORMAT format,
		const std::wstring& debugName);
	void ReleaseTexture(GpuVolumetricFluidTexture3D& texture);
	void ReleasePingPongField(GpuVolumetricFluidPingPongField& field);

private:
	GpuVolumetricFluidGridDesc gridDesc_{};
	GpuVolumetricFluidPingPongField velocity_{};
	GpuVolumetricFluidPingPongField pressure_{};
	GpuVolumetricFluidPingPongField density_{};
	GpuVolumetricFluidPingPongField temperature_{};
	GpuVolumetricFluidTexture3D divergence_{};
	GpuVolumetricFluidTexture3D vorticity_{};
	GpuVolumetricFluidTexture3D obstacle_{};
	bool initialized_ = false;
};

} // namespace Ken4lowEngine
