#pragma once

#include "../Data/GpuVolumetricFluidTypes.h"
#include "../Resource/GpuVolumetricFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;

class GpuVolumetricFluidScalarAdvectionPass
{
public:
	bool Initialize();
	void Finalize();

	bool Dispatch(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		GpuVolumetricFluidField field,
		float deltaTime,
		float elapsedTime);
	bool DispatchAll(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
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
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime) const;
	bool DispatchInternal(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		GpuVolumetricFluidField field,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	GpuVolumetricFluidPingPongField* ResolveScalarField(
		GpuVolumetricFluidGridResource& grid,
		GpuVolumetricFluidField field) const;
	float ResolveDissipation(
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		GpuVolumetricFluidField field) const;
	bool CreateRootSignature();
	bool CreatePipelineState();
	void DispatchGrid(
		ID3D12GraphicsCommandList* commandList,
		const GpuVolumetricFluidGridDesc& gridDesc) const;

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;
	static constexpr uint32_t kThreadGroupSizeZ = 4;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	uint64_t dispatchCount_ = 0;
	uint64_t densityDispatchCount_ = 0;
	uint64_t temperatureDispatchCount_ = 0;
};

} // namespace Ken4lowEngine
