#pragma once

#include "../Data/GpuFluidTypes.h"
#include "../Resource/GpuFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;
enum class GpuFluidComputeShaderId : uint32_t;

class GpuFluidPressureProjectionPass
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
		return dxCommon_ != nullptr &&
			rootSignature_ != nullptr &&
			divergencePipelineState_ != nullptr &&
			pressureJacobiPipelineState_ != nullptr &&
			projectionPipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDispatchCount() const { return dispatchCount_; }
	[[nodiscard]] uint32_t GetLastPressureIterationCount() const { return lastPressureIterationCount_; }

private:
	bool CreateRootSignature();
	bool CreatePipelineState(
		GpuFluidComputeShaderId shaderId,
		ComPtr<ID3D12PipelineState>& pipelineState,
		const wchar_t* debugName);
	bool ClearPressure(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidGridResource& grid);
	bool DispatchDivergence(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	bool DispatchPressureJacobi(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
		uint32_t iterationCount);
	bool DispatchProjection(
		ID3D12GraphicsCommandList* commandList,
		GpuFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	void DispatchGrid(ID3D12GraphicsCommandList* commandList, const GpuFluidGridDesc& gridDesc) const;

private:
	static constexpr uint32_t kThreadGroupSizeX = 8;
	static constexpr uint32_t kThreadGroupSizeY = 8;

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> divergencePipelineState_;
	ComPtr<ID3D12PipelineState> pressureJacobiPipelineState_;
	ComPtr<ID3D12PipelineState> projectionPipelineState_;
	uint64_t dispatchCount_ = 0;
	uint32_t lastPressureIterationCount_ = 0;
};

} // namespace Ken4lowEngine
