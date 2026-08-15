#pragma once

#include "../Data/GpuVolumetricFluidTypes.h"
#include "../Resource/GpuVolumetricFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;
enum class GpuVolumetricFluidComputeShaderId : uint32_t;

class GpuVolumetricFluidForcePass
{
public:
	bool Initialize();
	void Finalize();

	bool DispatchVorticity(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime);
	bool DispatchBuoyancy(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime);
	bool DispatchAll(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime);

	[[nodiscard]] bool IsInitialized() const
	{
		return dxCommon_ != nullptr && rootSignature_ != nullptr &&
			curlPipelineState_ != nullptr &&
			vorticityPipelineState_ != nullptr &&
			buoyancyPipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDispatchCount() const { return dispatchCount_; }

private:
	bool ValidateDispatchContext(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime) const;
	bool CreateRootSignature();
	bool CreatePipelineState(
		GpuVolumetricFluidComputeShaderId shaderId,
		ComPtr<ID3D12PipelineState>& pipelineState,
		const wchar_t* debugName);
	bool DispatchCurl(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	bool DispatchVorticityConfinement(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	bool DispatchBuoyancyInternal(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	void DispatchGrid(
		ID3D12GraphicsCommandList* commandList,
		const GpuVolumetricFluidGridDesc& gridDesc) const;

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;
	static constexpr uint32_t kThreadGroupSizeZ = 4;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> curlPipelineState_;
	ComPtr<ID3D12PipelineState> vorticityPipelineState_;
	ComPtr<ID3D12PipelineState> buoyancyPipelineState_;
	uint64_t dispatchCount_ = 0;
};

} // namespace Ken4lowEngine
