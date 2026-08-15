#pragma once

#include "../Data/GpuVolumetricFluidEmitterTypes.h"
#include "../Resource/GpuVolumetricFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

class DirectXCommon;

class GpuVolumetricFluidEmitterInjectionPass
{
public:
	bool Initialize();
	void Finalize();

	bool Dispatch(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		const GpuVolumetricFluidDomainMapping& domain,
		const std::vector<GpuVolumetricFluidEmitterSource>& sources,
		float deltaTime,
		float elapsedTime);

	[[nodiscard]] bool IsInitialized() const
	{
		return dxCommon_ != nullptr && rootSignature_ != nullptr && pipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDispatchCount() const { return dispatchCount_; }
	[[nodiscard]] uint32_t GetLastInjectedSourceCount() const { return lastInjectedSourceCount_; }
	[[nodiscard]] uint32_t GetLastCulledSourceCount() const { return lastCulledSourceCount_; }

private:
	bool CreateRootSignature();
	bool CreatePipelineState();
	bool ValidateDispatchContext(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		const GpuVolumetricFluidDomainMapping& domain,
		float deltaTime,
		float elapsedTime) const;

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;
	static constexpr uint32_t kThreadGroupSizeZ = 4;
	static constexpr uint32_t kMaxSourcesPerDispatch = 256;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	uint64_t dispatchCount_ = 0;
	uint32_t lastInjectedSourceCount_ = 0;
	uint32_t lastCulledSourceCount_ = 0;
};

} // namespace Ken4lowEngine
