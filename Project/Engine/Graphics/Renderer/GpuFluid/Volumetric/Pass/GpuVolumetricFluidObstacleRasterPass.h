#pragma once

#include "../Data/GpuVolumetricFluidObstacleTypes.h"
#include "../Resource/GpuVolumetricFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

class DirectXCommon;

class GpuVolumetricFluidObstacleRasterPass
{
public:
	bool Initialize();
	void Finalize();

	bool Dispatch(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		const GpuVolumetricFluidDomainMapping& domain,
		const std::vector<GpuVolumetricFluidObstacleSource>& sources,
		float deltaTime,
		float elapsedTime);

	[[nodiscard]] bool IsInitialized() const
	{
		return dxCommon_ != nullptr && rootSignature_ != nullptr && pipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDispatchCount() const { return dispatchCount_; }
	[[nodiscard]] uint32_t GetLastObstacleCount() const { return lastObstacleCount_; }
	[[nodiscard]] uint32_t GetLastCulledObstacleCount() const { return lastCulledObstacleCount_; }

private:
	bool ValidateDispatchContext(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		const GpuVolumetricFluidDomainMapping& domain,
		float deltaTime,
		float elapsedTime) const;
	bool ClearObstacle(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidGridResource& grid);
	bool CreateRootSignature();
	bool CreatePipelineState();

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;
	static constexpr uint32_t kThreadGroupSizeZ = 4;
	static constexpr std::size_t kMaxObstaclesPerDispatch = 256;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	uint64_t dispatchCount_ = 0;
	uint32_t lastObstacleCount_ = 0;
	uint32_t lastCulledObstacleCount_ = 0;
};

} // namespace Ken4lowEngine
