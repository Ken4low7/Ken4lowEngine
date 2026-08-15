#include "GpuFluidScalarAdvectionPass.h"

#include <DirectXCommon.h>
#include <GpuFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <string>

namespace Ken4lowEngine
{

bool GpuFluidScalarAdvectionPass::Initialize()
{
	Finalize();

	dxCommon_ = DirectXCommon::GetInstance();
	if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr)
	{
		Finalize();
		return false;
	}

	if (!CreateRootSignature() || !CreatePipelineState())
	{
		Finalize();
		return false;
	}

	rootSignature_->SetName(L"GpuFluid.ScalarAdvection.RootSignature");
	pipelineState_->SetName(L"GpuFluid.ScalarAdvection.PSO");
	dispatchCount_ = 0;
	densityDispatchCount_ = 0;
	temperatureDispatchCount_ = 0;
	return true;
}

void GpuFluidScalarAdvectionPass::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
	densityDispatchCount_ = 0;
	temperatureDispatchCount_ = 0;
}

bool GpuFluidScalarAdvectionPass::Dispatch(
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	GpuFluidField field,
	float deltaTime,
	float elapsedTime)
{
	if (!ValidateDispatchContext(grid, simulationDesc, deltaTime))
	{
		return false;
	}

	if (field != GpuFluidField::Density && field != GpuFluidField::Temperature)
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
	}

	const GpuFluidSimulationConstants constants =
		BuildGpuFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const FrameUploadArena::Allocation constantAllocation =
		dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!constantAllocation.IsValid())
	{
		return false;
	}

	UAVManager::GetInstance()->PreDispatch();
	return DispatchInternal(
		commandList,
		grid,
		simulationDesc,
		field,
		constantAllocation.gpuAddress);
}

bool GpuFluidScalarAdvectionPass::DispatchAll(
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	float deltaTime,
	float elapsedTime)
{
	if (!ValidateDispatchContext(grid, simulationDesc, deltaTime))
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
	}

	const GpuFluidSimulationConstants constants =
		BuildGpuFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const FrameUploadArena::Allocation constantAllocation =
		dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!constantAllocation.IsValid())
	{
		return false;
	}

	UAVManager::GetInstance()->PreDispatch();

	// DensityとTemperatureは同じ速度場・Obstacle・CBを共有し、1Step内の重複Uploadを最小化する。
	return DispatchInternal(
		commandList,
		grid,
		simulationDesc,
		GpuFluidField::Density,
		constantAllocation.gpuAddress) &&
		DispatchInternal(
			commandList,
			grid,
			simulationDesc,
			GpuFluidField::Temperature,
			constantAllocation.gpuAddress);
}

bool GpuFluidScalarAdvectionPass::ValidateDispatchContext(
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	float deltaTime) const
{
	if (!IsInitialized() || !grid.IsInitialized() || !simulationDesc.IsValid() || deltaTime <= 0.0f)
	{
		return false;
	}

	const GpuFluidGridDesc& gridDesc = grid.GetGridDesc();
	return gridDesc.width == simulationDesc.grid.width &&
		gridDesc.height == simulationDesc.grid.height &&
		gridDesc.cellSize == simulationDesc.grid.cellSize;
}

bool GpuFluidScalarAdvectionPass::DispatchInternal(
	ID3D12GraphicsCommandList* commandList,
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	GpuFluidField field,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuFluidPingPongField* scalarField = ResolveScalarField(grid, field);
	if (commandList == nullptr || scalarField == nullptr)
	{
		return false;
	}

	GpuFluidTexture2D& velocity = grid.GetVelocity().Read();
	GpuFluidTexture2D& scalarRead = scalarField->Read();
	GpuFluidTexture2D& scalarWrite = scalarField->Write();
	GpuFluidTexture2D& obstacle = grid.GetObstacle();
	if (!velocity.IsValid() || !scalarRead.IsValid() || !scalarWrite.IsValid() || !obstacle.IsValid())
	{
		return false;
	}

	GpuFluidGridResource::Transition(commandList, velocity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, scalarRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, scalarWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptorManager = UAVManager::GetInstance();
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(1, descriptorManager->GetGPUDescriptorHandle(velocity.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(2, descriptorManager->GetGPUDescriptorHandle(scalarRead.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(3, descriptorManager->GetGPUDescriptorHandle(scalarWrite.uavIndex));

	const float scalarConstants[4] =
	{
		ResolveDissipation(simulationDesc, field),
		0.0f,
		0.0f,
		0.0f
	};
	commandList->SetComputeRoot32BitConstants(4, 4, scalarConstants, 0);
	commandList->SetComputeRootDescriptorTable(5, descriptorManager->GetGPUDescriptorHandle(obstacle.computeSrvIndex));
	DispatchGrid(commandList, grid.GetGridDesc());

	GpuFluidGridResource::InsertUavBarrier(commandList, scalarWrite.resource.Get());
	GpuFluidGridResource::Transition(commandList, scalarWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	scalarField->Swap();

	++dispatchCount_;
	if (field == GpuFluidField::Density)
	{
		++densityDispatchCount_;
	}
	else
	{
		++temperatureDispatchCount_;
	}
	return true;
}

GpuFluidPingPongField* GpuFluidScalarAdvectionPass::ResolveScalarField(
	GpuFluidGridResource& grid,
	GpuFluidField field) const
{
	switch (field)
	{
	case GpuFluidField::Density:
		return &grid.GetDensity();
	case GpuFluidField::Temperature:
		return &grid.GetTemperature();
	default:
		return nullptr;
	}
}

float GpuFluidScalarAdvectionPass::ResolveDissipation(
	const GpuFluidSimulationDesc& simulationDesc,
	GpuFluidField field) const
{
	switch (field)
	{
	case GpuFluidField::Density:
		return simulationDesc.densityDissipation;
	case GpuFluidField::Temperature:
		return simulationDesc.temperatureDissipation;
	default:
		return 1.0f;
	}
}

bool GpuFluidScalarAdvectionPass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[6]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE velocitySrvRange{};
	velocitySrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	velocitySrvRange.NumDescriptors = 1;
	velocitySrvRange.BaseShaderRegister = 0;
	velocitySrvRange.RegisterSpace = 0;
	velocitySrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &velocitySrvRange;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE scalarSrvRange{};
	scalarSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	scalarSrvRange.NumDescriptors = 1;
	scalarSrvRange.BaseShaderRegister = 1;
	scalarSrvRange.RegisterSpace = 0;
	scalarSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &scalarSrvRange;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE scalarUavRange{};
	scalarUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	scalarUavRange.NumDescriptors = 1;
	scalarUavRange.BaseShaderRegister = 0;
	scalarUavRange.RegisterSpace = 0;
	scalarUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].DescriptorTable.pDescriptorRanges = &scalarUavRange;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[4].Constants.ShaderRegister = 1;
	rootParameters[4].Constants.RegisterSpace = 0;
	rootParameters[4].Constants.Num32BitValues = 4;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE obstacleSrvRange{};
	obstacleSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	obstacleSrvRange.NumDescriptors = 1;
	obstacleSrvRange.BaseShaderRegister = 2;
	obstacleSrvRange.RegisterSpace = 0;
	obstacleSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[5].DescriptorTable.pDescriptorRanges = &obstacleSrvRange;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 1;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &sampler;
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

bool GpuFluidScalarAdvectionPass::CreatePipelineState()
{
	const ShaderDescriptor& shaderDesc =
		GpuFluidShaderManifest::GetCompute(GpuFluidComputeShaderId::ScalarAdvection);
	if (shaderDesc.stage != ShaderStage::Compute || shaderDesc.rootSignature != RootSignatureType::Compute)
	{
		return false;
	}

	ComPtr<IDxcBlob> computeShader = ShaderCompiler::CompileShader(
		shaderDesc,
		dxCommon_->GetDXCCompilerManager());
	if (!computeShader)
	{
		return false;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.CS = { computeShader->GetBufferPointer(), computeShader->GetBufferSize() };
	return SUCCEEDED(dxCommon_->GetDevice()->CreateComputePipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&pipelineState_)));
}

void GpuFluidScalarAdvectionPass::DispatchGrid(
	ID3D12GraphicsCommandList* commandList,
	const GpuFluidGridDesc& gridDesc) const
{
	const uint32_t groupCountX = (gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY = (gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	commandList->Dispatch(groupCountX, groupCountY, 1);
}

} // namespace Ken4lowEngine
