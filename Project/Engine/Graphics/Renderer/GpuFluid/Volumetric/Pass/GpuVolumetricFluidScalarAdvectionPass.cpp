#include "GpuVolumetricFluidScalarAdvectionPass.h"

#include <DirectXCommon.h>
#include <GpuVolumetricFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <cmath>
#include <string>

namespace Ken4lowEngine
{

bool GpuVolumetricFluidScalarAdvectionPass::Initialize()
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

	rootSignature_->SetName(L"GpuVolumetricFluid.ScalarAdvection.RootSignature");
	pipelineState_->SetName(L"GpuVolumetricFluid.ScalarAdvection.PSO");
	return true;
}

void GpuVolumetricFluidScalarAdvectionPass::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
	densityDispatchCount_ = 0;
	temperatureDispatchCount_ = 0;
}

bool GpuVolumetricFluidScalarAdvectionPass::Dispatch(
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidSimulationDesc& simulationDesc,
	GpuVolumetricFluidField field,
	float deltaTime,
	float elapsedTime)
{
	if (!ValidateDispatchContext(grid, simulationDesc, deltaTime, elapsedTime) ||
		(field != GpuVolumetricFluidField::Density && field != GpuVolumetricFluidField::Temperature))
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
	}

	const auto constants = BuildGpuVolumetricFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const auto allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!allocation.IsValid())
	{
		return false;
	}

	UAVManager::GetInstance()->PreDispatch();
	return DispatchInternal(commandList, grid, simulationDesc, field, allocation.gpuAddress);
}

bool GpuVolumetricFluidScalarAdvectionPass::DispatchAll(
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidSimulationDesc& simulationDesc,
	float deltaTime,
	float elapsedTime)
{
	if (!ValidateDispatchContext(grid, simulationDesc, deltaTime, elapsedTime))
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
	}

	const auto constants = BuildGpuVolumetricFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const auto allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!allocation.IsValid())
	{
		return false;
	}

	UAVManager::GetInstance()->PreDispatch();

	// Density/Temperatureは同じVelocityとSimulation CBを共有し、1 Step内のUploadを1回にまとめる。
	return DispatchInternal(
		commandList, grid, simulationDesc, GpuVolumetricFluidField::Density, allocation.gpuAddress) &&
		DispatchInternal(
			commandList, grid, simulationDesc, GpuVolumetricFluidField::Temperature, allocation.gpuAddress);
}

bool GpuVolumetricFluidScalarAdvectionPass::ValidateDispatchContext(
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidSimulationDesc& simulationDesc,
	float deltaTime,
	float elapsedTime) const
{
	if (!IsInitialized() || !grid.IsInitialized() || !simulationDesc.IsValid() ||
		deltaTime <= 0.0f || !std::isfinite(deltaTime) || !std::isfinite(elapsedTime))
	{
		return false;
	}

	const GpuVolumetricFluidGridDesc& gridDesc = grid.GetGridDesc();
	return gridDesc.width == simulationDesc.grid.width &&
		gridDesc.height == simulationDesc.grid.height &&
		gridDesc.depth == simulationDesc.grid.depth &&
		gridDesc.cellSize == simulationDesc.grid.cellSize;
}

bool GpuVolumetricFluidScalarAdvectionPass::DispatchInternal(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidSimulationDesc& simulationDesc,
	GpuVolumetricFluidField field,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuVolumetricFluidPingPongField* scalarField = ResolveScalarField(grid, field);
	if (commandList == nullptr || scalarField == nullptr)
	{
		return false;
	}

	GpuVolumetricFluidTexture3D& velocity = grid.GetVelocity().Read();
	GpuVolumetricFluidTexture3D& scalarRead = scalarField->Read();
	GpuVolumetricFluidTexture3D& scalarWrite = scalarField->Write();
	if (!velocity.IsValid() || !scalarRead.IsValid() || !scalarWrite.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, velocity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, scalarRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, scalarWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(
		1, descriptors->GetGPUDescriptorHandle(velocity.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(
		2, descriptors->GetGPUDescriptorHandle(scalarRead.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(
		3, descriptors->GetGPUDescriptorHandle(scalarWrite.uavIndex));

	const float scalarConstants[4] =
	{
		ResolveDissipation(simulationDesc, field),
		0.0f,
		0.0f,
		0.0f
	};
	commandList->SetComputeRoot32BitConstants(4, 4, scalarConstants, 0);
	DispatchGrid(commandList, grid.GetGridDesc());

	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, scalarWrite.resource.Get());
	GpuVolumetricFluidGridResource::Transition(
		commandList, scalarWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	scalarField->Swap();

	++dispatchCount_;
	if (field == GpuVolumetricFluidField::Density)
	{
		++densityDispatchCount_;
	}
	else
	{
		++temperatureDispatchCount_;
	}
	return true;
}

GpuVolumetricFluidPingPongField* GpuVolumetricFluidScalarAdvectionPass::ResolveScalarField(
	GpuVolumetricFluidGridResource& grid,
	GpuVolumetricFluidField field) const
{
	switch (field)
	{
	case GpuVolumetricFluidField::Density:
		return &grid.GetDensity();
	case GpuVolumetricFluidField::Temperature:
		return &grid.GetTemperature();
	default:
		return nullptr;
	}
}

float GpuVolumetricFluidScalarAdvectionPass::ResolveDissipation(
	const GpuVolumetricFluidSimulationDesc& simulationDesc,
	GpuVolumetricFluidField field) const
{
	switch (field)
	{
	case GpuVolumetricFluidField::Density:
		return simulationDesc.densityDissipation;
	case GpuVolumetricFluidField::Temperature:
		return simulationDesc.temperatureDissipation;
	default:
		return 1.0f;
	}
}

bool GpuVolumetricFluidScalarAdvectionPass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[5]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE srvRanges[2]{};
	for (uint32_t i = 0; i < 2; ++i)
	{
		srvRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRanges[i].NumDescriptors = 1;
		srvRanges[i].BaseShaderRegister = i;
		srvRanges[i].RegisterSpace = 0;
		srvRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		rootParameters[i + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[i + 1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[i + 1].DescriptorTable.pDescriptorRanges = &srvRanges[i];
		rootParameters[i + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

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

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[4].Constants.ShaderRegister = 1;
	rootParameters[4].Constants.RegisterSpace = 0;
	rootParameters[4].Constants.Num32BitValues = 4;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = _countof(rootParameters);
	desc.pParameters = rootParameters;
	desc.NumStaticSamplers = 1;
	desc.pStaticSamplers = &sampler;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	const HRESULT serializeResult = D3D12SerializeRootSignature(
		&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(serializeResult))
	{
		if (errorBlob)
		{
			Log(std::string(
				static_cast<const char*>(errorBlob->GetBufferPointer()),
				errorBlob->GetBufferSize()));
		}
		return false;
	}

	return SUCCEEDED(dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_)));
}

bool GpuVolumetricFluidScalarAdvectionPass::CreatePipelineState()
{
	const ShaderDescriptor& shaderDesc = GpuVolumetricFluidShaderManifest::GetCompute(
		GpuVolumetricFluidComputeShaderId::ScalarAdvection);
	if (shaderDesc.stage != ShaderStage::Compute || shaderDesc.rootSignature != RootSignatureType::Compute)
	{
		return false;
	}

	ComPtr<IDxcBlob> shader = ShaderCompiler::CompileShader(
		shaderDesc, dxCommon_->GetDXCCompilerManager());
	if (!shader)
	{
		return false;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
	return SUCCEEDED(dxCommon_->GetDevice()->CreateComputePipelineState(
		&desc, IID_PPV_ARGS(&pipelineState_)));
}

void GpuVolumetricFluidScalarAdvectionPass::DispatchGrid(
	ID3D12GraphicsCommandList* commandList,
	const GpuVolumetricFluidGridDesc& gridDesc) const
{
	const uint32_t groupCountX = (gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY = (gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	const uint32_t groupCountZ = (gridDesc.depth + kThreadGroupSizeZ - 1u) / kThreadGroupSizeZ;
	commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

} // namespace Ken4lowEngine
