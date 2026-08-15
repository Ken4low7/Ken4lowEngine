#pragma once

#include "../Data/GpuFluidEmitterTypes.h"
#include "../Resource/GpuFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

class DirectXCommon;

class GpuFluidEmitterInjectionPass
{
public:
	bool Initialize();
	void Finalize();

	bool Dispatch(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		const GpuFluidDomainMapping& domain,
		const std::vector<GpuFluidEmitterSource>& sources,
		float deltaTime,
		float elapsedTime);

	[[nodiscard]] bool IsInitialized() const
	{
		return dxCommon_ != nullptr && rootSignature_ != nullptr && pipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDispatchCount() const { return dispatchCount_; }
	[[nodiscard]] uint32_t GetLastInjectedSourceCount() const { return lastInjectedSourceCount_; }

private:
	bool CreateRootSignature();
	bool CreatePipelineState();
	bool ValidateDispatchContext(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		const GpuFluidDomainMapping& domain,
		float deltaTime) const;

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	uint64_t dispatchCount_ = 0;
	uint32_t lastInjectedSourceCount_ = 0;
};

} // namespace Ken4lowEngine
