#pragma once

#include "../Data/GpuVolumetricFluidTypes.h"
#include "../Resource/GpuVolumetricFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;

/// 3D Texture velocity fieldをsemi-Lagrangianで移流するPhase17.3 Compute Pass。
class GpuVolumetricFluidVelocityAdvectionPass
{
public:
	bool Initialize();
	void Finalize();

	bool Dispatch(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
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
	static constexpr uint32_t kThreadGroupSizeZ = 4;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	uint64_t dispatchCount_ = 0;
};

} // namespace Ken4lowEngine
