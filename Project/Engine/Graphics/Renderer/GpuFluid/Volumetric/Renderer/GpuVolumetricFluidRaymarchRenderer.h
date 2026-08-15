#pragma once

#include "../Data/GpuVolumetricFluidRenderTypes.h"
#include "../Resource/GpuVolumetricFluidGridResource.h"

#include <DX12Include.h>
#include <Matrix4x4.h>
#include <Vector3.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;

/// Density/Temperature Texture3DをWorld-space oriented volumeとしてRaymarch描画するRenderer。
class GpuVolumetricFluidRaymarchRenderer
{
public:
	bool Initialize();
	void Finalize();

	bool Draw(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidDomainMapping& domain,
		const GpuVolumetricFluidRenderDesc& renderDesc);
	bool Draw(
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidDomainMapping& domain,
		const GpuVolumetricFluidRenderDesc& renderDesc,
		const Matrix4x4& viewProjection,
		const Vector3& cameraPosition);

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
