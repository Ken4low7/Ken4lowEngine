#pragma once

#include "../Data/GpuFluidTypes.h"
#include "../Resource/GpuFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;

class GpuFluidVelocityAdvectionPass
{
public:
	bool Initialize();
	void Finalize();

	bool Dispatch(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime);

	[[nodiscard]] bool IsInitialized() const
	{
		return dxCommon_ != nullptr && rootSignature_ != nullptr && pipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDispatchCount() const { return dispatchCount_; }

private:
	bool CreateRootSignature();
	bool CreatePipelineState();

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	uint64_t dispatchCount_ = 0;
};

} // namespace Ken4lowEngine
