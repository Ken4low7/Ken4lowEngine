#pragma once

#include "../Data/GpuFluidTypes.h"
#include "../Resource/GpuFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;
enum class GpuFluidComputeShaderId : uint32_t;

class GpuFluidForcePass
{
public:
	bool Initialize();
	void Finalize();

	bool DispatchVorticity(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime);
	bool DispatchBuoyancy(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime);
	bool DispatchAll(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		float deltaTime,
		float elapsedTime);

	[[nodiscard]] bool IsInitialized() const
	{
		return dxCommon_ != nullptr &&
			rootSignature_ != nullptr &&
			curlPipelineState_ != nullptr &&
			vorticityPipelineState_ != nullptr &&
			buoyancyPipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDispatchCount() const { return dispatchCount_; }

private:
	bool ValidateDispatchContext(
		GpuFluidGridResource& grid,
		const GpuFluidSimulationDesc& simulationDesc,
		float deltaTime) const;
	bool CreateRootSignature();
	bool CreatePipelineState(
		GpuFluidComputeShaderId shaderId,
		ComPtr<ID3D12PipelineState>& pipelineState,
		const wchar_t* debugName);
	bool DispatchCurl(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	bool DispatchVorticityConfinement(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	bool DispatchBuoyancyInternal(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	void DispatchGrid(ID3D12GraphicsCommandList* commandList, const GpuFluidGridDesc& gridDesc) const;

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> curlPipelineState_;
	ComPtr<ID3D12PipelineState> vorticityPipelineState_;
	ComPtr<ID3D12PipelineState> buoyancyPipelineState_;
	uint64_t dispatchCount_ = 0;
};

} // namespace Ken4lowEngine
