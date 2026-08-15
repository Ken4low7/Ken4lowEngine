#include "GpuFluidResetPass.h"

#include <DirectXCommon.h>
#include <UAVManager.h>

namespace Ken4lowEngine
{

bool GpuFluidResetPass::Initialize()
{
	Finalize();
	dxCommon_ = DirectXCommon::GetInstance();
	return dxCommon_ != nullptr && dxCommon_->GetDevice() != nullptr;
}

void GpuFluidResetPass::Finalize()
{
	dxCommon_ = nullptr;
	resetCount_ = 0;
}

bool GpuFluidResetPass::Reset(GpuFluidGridResource& grid)
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

	auto clearPingPong = [this, commandList](GpuFluidPingPongField& field)
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

	// Clear後は全ping-pongをreadIndex=0へ戻し、Reset後のResource世代を決定的にする。
	grid.GetVelocity().Reset();
	grid.GetPressure().Reset();
	grid.GetDensity().Reset();
	grid.GetTemperature().Reset();
	++resetCount_;
	return true;
}

bool GpuFluidResetPass::ClearFloatTexture(
	ID3D12GraphicsCommandList* commandList,
	GpuFluidTexture2D& texture)
{
	if (commandList == nullptr || !texture.IsValid())
	{
		return false;
	}

	UAVManager* descriptors = UAVManager::GetInstance();
	constexpr float kClearValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	GpuFluidGridResource::Transition(commandList, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ClearUnorderedAccessViewFloat(
		descriptors->GetGPUDescriptorHandle(texture.uavIndex),
		descriptors->GetClearCPUDescriptorHandle(texture.uavIndex),
		texture.resource.Get(),
		kClearValue,
		0,
		nullptr);
	GpuFluidGridResource::InsertUavBarrier(commandList, texture.resource.Get());
	GpuFluidGridResource::Transition(commandList, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	return true;
}

bool GpuFluidResetPass::ClearUintTexture(
	ID3D12GraphicsCommandList* commandList,
	GpuFluidTexture2D& texture)
{
	if (commandList == nullptr || !texture.IsValid())
	{
		return false;
	}

	UAVManager* descriptors = UAVManager::GetInstance();
	constexpr uint32_t kClearValue[4] = { 0u, 0u, 0u, 0u };
	GpuFluidGridResource::Transition(commandList, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ClearUnorderedAccessViewUint(
		descriptors->GetGPUDescriptorHandle(texture.uavIndex),
		descriptors->GetClearCPUDescriptorHandle(texture.uavIndex),
		texture.resource.Get(),
		kClearValue,
		0,
		nullptr);
	GpuFluidGridResource::InsertUavBarrier(commandList, texture.resource.Get());
	GpuFluidGridResource::Transition(commandList, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	return true;
}

} // namespace Ken4lowEngine
