#pragma once

#include "../Data/GpuVolumetricFluidTypes.h"
#include "../Resource/GpuVolumetricFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;
enum class GpuVolumetricFluidComputeShaderId : uint32_t;

/// 3D速度場を非圧縮へ戻すDivergence / Jacobi Pressure / Projection複合Pass。
class GpuVolumetricFluidPressureProjectionPass
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
		GpuVolumetricFluidComputeShaderId shaderId,
		ComPtr<ID3D12PipelineState>& pipelineState,
		const wchar_t* debugName);
	bool ClearPressure(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidGridResource& grid);
	bool DispatchDivergence(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
	bool DispatchPressureJacobi(
		ID3D12GraphicsCommandList* commandList,
		GpuVolumetricFluidGridResource& grid,
		D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
		uint32_t iterationCount);
	bool DispatchProjection(
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
	ComPtr<ID3D12PipelineState> divergencePipelineState_;
	ComPtr<ID3D12PipelineState> pressureJacobiPipelineState_;
	ComPtr<ID3D12PipelineState> projectionPipelineState_;
	uint64_t dispatchCount_ = 0;
	uint32_t lastPressureIterationCount_ = 0;
};

} // namespace Ken4lowEngine
