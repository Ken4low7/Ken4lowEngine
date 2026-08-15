#pragma once

#include "../Resource/GpuVolumetricFluidGridResource.h"

#include <cstdint>

namespace Ken4lowEngine
{

class DirectXCommon;

/// Phase17開始時点からVolume fieldを未初期化のまま読まないためのdeterministic reset pass。
class GpuVolumetricFluidResetPass
{
public:
	bool Initialize();
	void Finalize();

	bool Reset(GpuVolumetricFluidGridResource& grid);

	[[nodiscard]] bool IsInitialized() const { return dxCommon_ != nullptr; }
	[[nodiscard]] uint64_t GetResetCount() const { return resetCount_; }

private:
	bool ClearFloatTexture(ID3D12GraphicsCommandList* commandList, GpuVolumetricFluidTexture3D& texture);
	bool ClearUintTexture(ID3D12GraphicsCommandList* commandList, GpuVolumetricFluidTexture3D& texture);

private:
	DirectXCommon* dxCommon_ = nullptr;
	uint64_t resetCount_ = 0;
};

} // namespace Ken4lowEngine
