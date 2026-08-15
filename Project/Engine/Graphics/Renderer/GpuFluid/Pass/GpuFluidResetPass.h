#pragma once

#include "../Resource/GpuFluidGridResource.h"
#include <DX12Include.h>

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;

/// GPU Fluidの全Simulation Fieldを0へ戻すClear専用Pass。
class GpuFluidResetPass
{
public:
	bool Initialize();
	void Finalize();

	bool Reset(GpuFluidGridResource& grid);

	[[nodiscard]] bool IsInitialized() const { return dxCommon_ != nullptr; }
	[[nodiscard]] uint64_t GetResetCount() const { return resetCount_; }

private:
	bool ClearFloatTexture(ID3D12GraphicsCommandList* commandList, GpuFluidTexture2D& texture);
	bool ClearUintTexture(ID3D12GraphicsCommandList* commandList, GpuFluidTexture2D& texture);

private:
	DirectXCommon* dxCommon_ = nullptr;
	uint64_t resetCount_ = 0;
};

} // namespace Ken4lowEngine
