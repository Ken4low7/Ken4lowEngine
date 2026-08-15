#include "GpuFluidObstacleRasterPass.h"

#include <DirectXCommon.h>
#include <GpuFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <cstring>
#include <string>
#include <vector>

namespace Ken4lowEngine
{

bool GpuFluidObstacleRasterPass::Initialize()
{
	Finalize();
	dxCommon_ = DirectXCommon::GetInstance();
	if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr)
	{
		return false;
	}

	if (!CreateRootSignature() || !CreatePipelineState())
	{
		Finalize();
		return false;
	}

	rootSignature_->SetName(L"GpuFluid.ObstacleRaster.RootSignature");
	pipelineState_->SetName(L"GpuFluid.ObstacleRaster.PSO");
	return true;
}

void GpuFluidObstacleRasterPass::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
	lastObstacleCount_ = 0;
}

bool GpuFluidObstacleRasterPass::Dispatch(
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	const GpuFluidDomainMapping& domain,
	const std::vector<GpuFluidObstacleSource>& sources,
	float deltaTime,
	float elapsedTime)
{
	lastObstacleCount_ = 0;
	if (!ValidateDispatchContext(grid, simulationDesc, domain, deltaTime))
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
	}

	std::vector<GpuFluidObstacleGpuData> activeObstacles;
	activeObstacles.reserve(sources.size());
	for (const GpuFluidObstacleSource& source : sources)
	{
		GpuFluidObstacleGpuData gpuData{};
		if (BuildGpuFluidObstacleGpuData(source, gpuData))
		{
			activeObstacles.push_back(gpuData);
		}
	}

	UAVManager::GetInstance()->PreDispatch();
	if (activeObstacles.empty())
	{
		return ClearObstacle(commandList, grid); // 動的Colliderが消えたフレームでも古いSolid Cellを残さない。
	}

	FrameUploadArena& uploadArena = dxCommon_->GetFrameUploadArena();
	const GpuFluidSimulationConstants simulationConstants =
		BuildGpuFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const FrameUploadArena::Allocation simulationAllocation =
		uploadArena.AllocateConstant(simulationConstants);
	if (!simulationAllocation.IsValid())
	{
		return false;
	}

	const GpuFluidObstacleRasterConstants rasterConstants =
		BuildGpuFluidObstacleRasterConstants(
			domain,
			grid.GetGridDesc(),
			static_cast<uint32_t>(activeObstacles.size()));
	const FrameUploadArena::Allocation rasterAllocation =
		uploadArena.AllocateConstant(rasterConstants);
	if (!rasterAllocation.IsValid())
	{
		return false;
	}

	const std::size_t obstacleBytes = activeObstacles.size() * sizeof(GpuFluidObstacleGpuData);
	const FrameUploadArena::Allocation obstacleAllocation =
		uploadArena.Allocate(obstacleBytes, alignof(GpuFluidObstacleGpuData));
	if (!obstacleAllocation.IsValid())
	{
		return false;
	}
	std::memcpy(obstacleAllocation.cpuAddress, activeObstacles.data(), obstacleBytes);

	GpuFluidTexture2D& obstacle = grid.GetObstacle();
	if (!obstacle.IsValid())
	{
		return false;
	}
	GpuFluidGridResource::Transition(commandList, obstacle, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, simulationAllocation.gpuAddress);
	commandList->SetComputeRootConstantBufferView(1, rasterAllocation.gpuAddress);
	commandList->SetComputeRootShaderResourceView(2, obstacleAllocation.gpuAddress);
	commandList->SetComputeRootDescriptorTable(3, descriptors->GetGPUDescriptorHandle(obstacle.uavIndex));

	const GpuFluidGridDesc& gridDesc = grid.GetGridDesc();
	const uint32_t groupCountX = (gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY = (gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	commandList->Dispatch(groupCountX, groupCountY, 1);

	GpuFluidGridResource::InsertUavBarrier(commandList, obstacle.resource.Get());
	GpuFluidGridResource::Transition(commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	++dispatchCount_;
	lastObstacleCount_ = static_cast<uint32_t>(activeObstacles.size());
	return true;
}

bool GpuFluidObstacleRasterPass::ValidateDispatchContext(
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	const GpuFluidDomainMapping& domain,
	float deltaTime) const
{
	if (!IsInitialized() || !grid.IsInitialized() || !simulationDesc.IsValid() || !domain.IsValid() || deltaTime <= 0.0f)
	{
		return false;
	}

	const GpuFluidGridDesc& gridDesc = grid.GetGridDesc();
	return gridDesc.width == simulationDesc.grid.width &&
		gridDesc.height == simulationDesc.grid.height &&
		gridDesc.cellSize == simulationDesc.grid.cellSize;
}

bool GpuFluidObstacleRasterPass::ClearObstacle(
	ID3D12GraphicsCommandList* commandList,
	GpuFluidGridResource& grid)
{
	GpuFluidTexture2D& obstacle = grid.GetObstacle();
	if (commandList == nullptr || !obstacle.IsValid())
	{
		return false;
	}

	UAVManager* descriptors = UAVManager::GetInstance();
	constexpr uint32_t kClearValue[4] = { 0u, 0u, 0u, 0u };
	GpuFluidGridResource::Transition(commandList, obstacle, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ClearUnorderedAccessViewUint(
		descriptors->GetGPUDescriptorHandle(obstacle.uavIndex),
		descriptors->GetClearCPUDescriptorHandle(obstacle.uavIndex),
		obstacle.resource.Get(),
		kClearValue,
		0,
		nullptr);
	GpuFluidGridResource::InsertUavBarrier(commandList, obstacle.resource.Get());
	GpuFluidGridResource::Transition(commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	return true;
}

bool GpuFluidObstacleRasterPass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[4]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].Descriptor.ShaderRegister = 1;
	rootParameters[1].Descriptor.RegisterSpace = 0;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[2].Descriptor.ShaderRegister = 0;
	rootParameters[2].Descriptor.RegisterSpace = 0;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 1;
	uavRange.BaseShaderRegister = 0;
	uavRange.RegisterSpace = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	const HRESULT serializeResult = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);
	if (FAILED(serializeResult))
	{
		if (errorBlob)
		{
			Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize()));
		}
		return false;
	}

	return SUCCEEDED(dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_)));
}

bool GpuFluidObstacleRasterPass::CreatePipelineState()
{
	const ShaderDescriptor& shaderDesc =
		GpuFluidShaderManifest::GetCompute(GpuFluidComputeShaderId::ObstacleRaster);
	if (shaderDesc.stage != ShaderStage::Compute || shaderDesc.rootSignature != RootSignatureType::Compute)
	{
		return false;
	}

	ComPtr<IDxcBlob> shader = ShaderCompiler::CompileShader(shaderDesc, dxCommon_->GetDXCCompilerManager());
	if (!shader)
	{
		return false;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
	return SUCCEEDED(dxCommon_->GetDevice()->CreateComputePipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&pipelineState_)));
}

} // namespace Ken4lowEngine
