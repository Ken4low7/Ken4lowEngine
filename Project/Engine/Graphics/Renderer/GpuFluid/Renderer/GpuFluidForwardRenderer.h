#pragma once

#include "../Data/GpuFluidEmitterTypes.h"
#include "../Data/GpuFluidRenderTypes.h"
#include "../Resource/GpuFluidGridResource.h"

#include <DX12Include.h>
#include <Matrix4x4.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;

/// Density/Temperature/Obstacle fieldをWorld-space Domain面へForward描画するRenderer。
class GpuFluidForwardRenderer
{
public:
	bool Initialize();
	void Finalize();

	bool Draw(
		GpuFluidGridResource& grid,
		const GpuFluidDomainMapping& domain,
		const GpuFluidRenderDesc& renderDesc);
	bool Draw(
		GpuFluidGridResource& grid,
		const GpuFluidDomainMapping& domain,
		const GpuFluidRenderDesc& renderDesc,
		const Matrix4x4& viewProjection);

	[[nodiscard]] bool IsInitialized() const
	{
		return dxCommon_ != nullptr && rootSignature_ != nullptr && pipelineState_ != nullptr;
	}
	[[nodiscard]] uint64_t GetDrawCount() const { return drawCount_; }

private:
	bool CreateRootSignature();
	bool CreatePipelineState();

private:
	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	uint64_t drawCount_ = 0;
};

} // namespace Ken4lowEngine
