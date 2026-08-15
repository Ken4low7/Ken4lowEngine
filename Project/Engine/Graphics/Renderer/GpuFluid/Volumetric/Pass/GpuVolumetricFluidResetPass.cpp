#include "GpuVolumetricFluidResetPass.h"

#include <DirectXCommon.h>
#include <UAVManager.h>

namespace Ken4lowEngine
{

bool GpuVolumetricFluidResetPass::Initialize()
{
	Finalize();
	dxCommon_ = DirectXCommon::GetInstance();
	return dxCommon_ != nullptr && dxCommon_->GetDevice() != nullptr;
}

void GpuVolumetricFluidResetPass::Finalize()
{
	dxCommon_ = nullptr;
	resetCount_ = 0;
}

bool GpuVolumetricFluidResetPass::Reset(GpuVolumetricFluidGridResource& grid)
{
	if (!IsInitialized() || !grid.IsInitialized())
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
	}

	UAVManager::GetInstance()->PreDispatch();

	auto clearPingPong = [this, commandList](GpuVolumetricFluidPingPongField& field)
	{
		return ClearFloatTexture(commandList, field.textures[0]) &&
			ClearFloatTexture(commandList, field.textures[1]);
	};

	if (!clearPingPong(grid.GetVelocity()) ||
		!clearPingPong(grid.GetPressure()) ||
		!clearPingPong(grid.GetDensity()) ||
		!clearPingPong(grid.GetTemperature()) ||
		!ClearFloatTexture(commandList, grid.GetDivergence()) ||
		!ClearFloatTexture(commandList, grid.GetVorticity()) ||
		!ClearUintTexture(commandList, grid.GetObstacle()))
	{
		return false;
	}

	// Phase16で後付けになった初期化問題を繰り返さず、3D solverは最初から全Fieldをzero generationへ戻す。
	grid.GetVelocity().Reset();
	grid.GetPressure().Reset();
	grid.GetDensity().Reset();
	grid.GetTemperature().Reset();
	++resetCount_;
	return true;
}

bool GpuVolumetricFluidResetPass::ClearFloatTexture(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidTexture3D& texture)
{
	if (commandList == nullptr || !texture.IsValid())
	{
		return false;
	}

	UAVManager* descriptors = UAVManager::GetInstance();
	constexpr float kClearValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	GpuVolumetricFluidGridResource::Transition(commandList, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ClearUnorderedAccessViewFloat(
		descriptors->GetGPUDescriptorHandle(texture.uavIndex),
		descriptors->GetClearCPUDescriptorHandle(texture.uavIndex),
		texture.resource.Get(),
		kClearValue,
		0,
		nullptr);
	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, texture.resource.Get());
	GpuVolumetricFluidGridResource::Transition(commandList, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	return true;
}

bool GpuVolumetricFluidResetPass::ClearUintTexture(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidTexture3D& texture)
{
	if (commandList == nullptr || !texture.IsValid())
	{
		return false;
	}

	UAVManager* descriptors = UAVManager::GetInstance();
	constexpr uint32_t kClearValue[4] = { 0u, 0u, 0u, 0u };
	GpuVolumetricFluidGridResource::Transition(commandList, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ClearUnorderedAccessViewUint(
		descriptors->GetGPUDescriptorHandle(texture.uavIndex),
		descriptors->GetClearCPUDescriptorHandle(texture.uavIndex),
		texture.resource.Get(),
		kClearValue,
		0,
		nullptr);
	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, texture.resource.Get());
	GpuVolumetricFluidGridResource::Transition(commandList, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	return true;
}

} // namespace Ken4lowEngine
