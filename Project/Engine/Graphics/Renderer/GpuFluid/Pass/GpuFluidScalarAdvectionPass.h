#pragma once

#include "../Data/GpuFluidTypes.h"
#include "../Resource/GpuFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;

class GpuFluidScalarAdvectionPass
{
public:
	bool Initialize();
	void Finalize();

	bool Dispatch(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		GpuFluidField field,
		float deltaTime,
		float elapsedTime);
	bool DispatchAll(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime);

	[[nodiscard]] bool IsInitialized() const
	{
		return dxCommon_ != nullptr && rootSignature_ != nullptr && pipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDispatchCount() const { return dispatchCount_; }
	[[nodiscard]] uint64_t GetDensityDispatchCount() const { return densityDispatchCount_; }
	[[nodiscard]] uint64_t GetTemperatureDispatchCount() const { return temperatureDispatchCount_; }

private:
	bool ValidateDispatchContext(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		float deltaTime) const;
	bool DispatchInternal(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		GpuFluidField field,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	GpuFluidPingPongField* ResolveScalarField(GpuFluidGridResource& grid, GpuFluidField field) const;
	float ResolveDissipation(const GpuFluidSimulationDesc& simulationDesc, GpuFluidField field) const;
	bool CreateRootSignature();
	bool CreatePipelineState();
	void DispatchGrid(ID3D12GraphicsCommandList* commandList, const GpuFluidGridDesc& gridDesc) const;

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	uint64_t dispatchCount_ = 0;
	uint64_t densityDispatchCount_ = 0;
	uint64_t temperatureDispatchCount_ = 0;
};

} // namespace Ken4lowEngine
