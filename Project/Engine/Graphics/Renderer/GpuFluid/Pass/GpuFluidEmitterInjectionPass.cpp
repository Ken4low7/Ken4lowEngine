#include "GpuFluidEmitterInjectionPass.h"

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

bool GpuFluidEmitterInjectionPass::Initialize()
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

	rootSignature_->SetName(L"GpuFluid.EmitterInjection.RootSignature");
	pipelineState_->SetName(L"GpuFluid.EmitterInjection.PSO");
	return true;
}

void GpuFluidEmitterInjectionPass::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
	lastInjectedSourceCount_ = 0;
}

bool GpuFluidEmitterInjectionPass::Dispatch(
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	const GpuFluidDomainMapping& domain,
	const std::vector<GpuFluidEmitterSource>& sources,
	float deltaTime,
	float elapsedTime)
{
	lastInjectedSourceCount_ = 0;
	if (!ValidateDispatchContext(grid, simulationDesc, domain, deltaTime))
	{
		return false;
	}

	std::vector<GpuFluidEmitterGpuData> activeSources;
	activeSources.reserve(sources.size());
	for (const GpuFluidEmitterSource& source : sources)
	{
		GpuFluidEmitterGpuData gpuData{};
		if (BuildGpuFluidEmitterGpuData(source, domain, grid.GetGridDesc(), gpuData))
		{
			activeSources.push_back(gpuData);
		}
	}
	if (activeSources.empty())
	{
		return true;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
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

	const std::size_t emitterBytes = activeSources.size() * sizeof(GpuFluidEmitterGpuData);
	const FrameUploadArena::Allocation emitterAllocation =
		uploadArena.Allocate(emitterBytes, alignof(GpuFluidEmitterGpuData));
	if (!emitterAllocation.IsValid())
	{
		return false;
	}
	std::memcpy(emitterAllocation.cpuAddress, activeSources.data(), emitterBytes);

	GpuFluidPingPongField& velocity = grid.GetVelocity();
	GpuFluidPingPongField& density = grid.GetDensity();
	GpuFluidPingPongField& temperature = grid.GetTemperature();
	GpuFluidTexture2D& velocityRead = velocity.Read();
	GpuFluidTexture2D& velocityWrite = velocity.Write();
	GpuFluidTexture2D& densityRead = density.Read();
	GpuFluidTexture2D& densityWrite = density.Write();
	GpuFluidTexture2D& temperatureRead = temperature.Read();
	GpuFluidTexture2D& temperatureWrite = temperature.Write();
	GpuFluidTexture2D& obstacle = grid.GetObstacle();
	if (!velocityRead.IsValid() || !velocityWrite.IsValid() ||
		!densityRead.IsValid() || !densityWrite.IsValid() ||
		!temperatureRead.IsValid() || !temperatureWrite.IsValid() ||
		!obstacle.IsValid())
	{
		return false;
	}

	GpuFluidGridResource::Transition(commandList, velocityRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, densityRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, temperatureRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, velocityWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	GpuFluidGridResource::Transition(commandList, densityWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	GpuFluidGridResource::Transition(commandList, temperatureWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptorManager = UAVManager::GetInstance();
	descriptorManager->PreDispatch();
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, simulationAllocation.gpuAddress);

	const uint32_t batchConstants[4] =
	{
		static_cast<uint32_t>(activeSources.size()),
		0u,
		0u,
		0u
	};
	commandList->SetComputeRoot32BitConstants(1, 4, batchConstants, 0);
	commandList->SetComputeRootDescriptorTable(2, descriptorManager->GetGPUDescriptorHandle(velocityRead.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(3, descriptorManager->GetGPUDescriptorHandle(densityRead.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(4, descriptorManager->GetGPUDescriptorHandle(temperatureRead.computeSrvIndex));
	commandList->SetComputeRootShaderResourceView(5, emitterAllocation.gpuAddress);
	commandList->SetComputeRootDescriptorTable(6, descriptorManager->GetGPUDescriptorHandle(velocityWrite.uavIndex));
	commandList->SetComputeRootDescriptorTable(7, descriptorManager->GetGPUDescriptorHandle(densityWrite.uavIndex));
	commandList->SetComputeRootDescriptorTable(8, descriptorManager->GetGPUDescriptorHandle(temperatureWrite.uavIndex));
	commandList->SetComputeRootDescriptorTable(9, descriptorManager->GetGPUDescriptorHandle(obstacle.computeSrvIndex));

	const GpuFluidGridDesc& gridDesc = grid.GetGridDesc();
	const uint32_t groupCountX = (gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY = (gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	commandList->Dispatch(groupCountX, groupCountY, 1);

	// Obstacle内へのSource注入をShader側で0化しつつ、3フィールドを同じ世代へ揃えてSwapする。
	GpuFluidGridResource::InsertUavBarrier(commandList, velocityWrite.resource.Get());
	GpuFluidGridResource::InsertUavBarrier(commandList, densityWrite.resource.Get());
	GpuFluidGridResource::InsertUavBarrier(commandList, temperatureWrite.resource.Get());
	GpuFluidGridResource::Transition(commandList, velocityWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, densityWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, temperatureWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();
	density.Swap();
	temperature.Swap();

	++dispatchCount_;
	lastInjectedSourceCount_ = static_cast<uint32_t>(activeSources.size());
	return true;
}

bool GpuFluidEmitterInjectionPass::ValidateDispatchContext(
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

bool GpuFluidEmitterInjectionPass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[10]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[1].Constants.ShaderRegister = 1;
	rootParameters[1].Constants.RegisterSpace = 0;
	rootParameters[1].Constants.Num32BitValues = 4;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE srvRanges[3]{};
	for (uint32_t i = 0; i < 3; ++i)
	{
		srvRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRanges[i].NumDescriptors = 1;
		srvRanges[i].BaseShaderRegister = i;
		srvRanges[i].RegisterSpace = 0;
		srvRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		rootParameters[i + 2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[i + 2].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[i + 2].DescriptorTable.pDescriptorRanges = &srvRanges[i];
		rootParameters[i + 2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[5].Descriptor.ShaderRegister = 3;
	rootParameters[5].Descriptor.RegisterSpace = 0;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE uavRanges[3]{};
	for (uint32_t i = 0; i < 3; ++i)
	{
		uavRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRanges[i].NumDescriptors = 1;
		uavRanges[i].BaseShaderRegister = i;
		uavRanges[i].RegisterSpace = 0;
		uavRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		rootParameters[i + 6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[i + 6].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[i + 6].DescriptorTable.pDescriptorRanges = &uavRanges[i];
		rootParameters[i + 6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	D3D12_DESCRIPTOR_RANGE obstacleSrvRange{};
	obstacleSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	obstacleSrvRange.NumDescriptors = 1;
	obstacleSrvRange.BaseShaderRegister = 4;
	obstacleSrvRange.RegisterSpace = 0;
	obstacleSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[9].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[9].DescriptorTable.pDescriptorRanges = &obstacleSrvRange;
	rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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

bool GpuFluidEmitterInjectionPass::CreatePipelineState()
{
	const ShaderDescriptor& shaderDesc =
		GpuFluidShaderManifest::GetCompute(GpuFluidComputeShaderId::EmitterInjection);
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
