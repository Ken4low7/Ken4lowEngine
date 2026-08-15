#include "GpuVolumetricFluidEmitterInjectionPass.h"

#include <DirectXCommon.h>
#include <GpuVolumetricFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Ken4lowEngine
{

bool GpuVolumetricFluidEmitterInjectionPass::Initialize()
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

	rootSignature_->SetName(L"GpuVolumetricFluid.EmitterInjection.RootSignature");
	pipelineState_->SetName(L"GpuVolumetricFluid.EmitterInjection.PSO");
	return true;
}

void GpuVolumetricFluidEmitterInjectionPass::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
	lastInjectedSourceCount_ = 0;
	lastCulledSourceCount_ = 0;
}

bool GpuVolumetricFluidEmitterInjectionPass::Dispatch(
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidSimulationDesc& simulationDesc,
	const GpuVolumetricFluidDomainMapping& domain,
	const std::vector<GpuVolumetricFluidEmitterSource>& sources,
	float deltaTime,
	float elapsedTime)
{
	lastInjectedSourceCount_ = 0;
	lastCulledSourceCount_ = 0;
	if (!ValidateDispatchContext(grid, simulationDesc, domain, deltaTime, elapsedTime))
	{
		return false;
	}

	std::vector<GpuVolumetricFluidEmitterGpuData> activeSources;
	activeSources.reserve((sources.size() < kMaxSourcesPerDispatch) ? sources.size() : kMaxSourcesPerDispatch);
	for (const GpuVolumetricFluidEmitterSource& source : sources)
	{
		GpuVolumetricFluidEmitterGpuData gpuData{};
		if (!BuildGpuVolumetricFluidEmitterGpuData(source, domain, grid.GetGridDesc(), gpuData))
		{
			++lastCulledSourceCount_;
			continue;
		}

		if (activeSources.size() >= kMaxSourcesPerDispatch)
		{
			++lastCulledSourceCount_;
			continue;
		}
		activeSources.push_back(gpuData);
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
	const GpuVolumetricFluidSimulationConstants simulationConstants =
		BuildGpuVolumetricFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const FrameUploadArena::Allocation simulationAllocation =
		uploadArena.AllocateConstant(simulationConstants);
	if (!simulationAllocation.IsValid())
	{
		return false;
	}

	const std::size_t emitterBytes = activeSources.size() * sizeof(GpuVolumetricFluidEmitterGpuData);
	const FrameUploadArena::Allocation emitterAllocation =
		uploadArena.Allocate(emitterBytes, alignof(GpuVolumetricFluidEmitterGpuData));
	if (!emitterAllocation.IsValid())
	{
		return false;
	}
	std::memcpy(emitterAllocation.cpuAddress, activeSources.data(), emitterBytes);

	GpuVolumetricFluidPingPongField& velocity = grid.GetVelocity();
	GpuVolumetricFluidPingPongField& density = grid.GetDensity();
	GpuVolumetricFluidPingPongField& temperature = grid.GetTemperature();
	GpuVolumetricFluidTexture3D& velocityRead = velocity.Read();
	GpuVolumetricFluidTexture3D& velocityWrite = velocity.Write();
	GpuVolumetricFluidTexture3D& densityRead = density.Read();
	GpuVolumetricFluidTexture3D& densityWrite = density.Write();
	GpuVolumetricFluidTexture3D& temperatureRead = temperature.Read();
	GpuVolumetricFluidTexture3D& temperatureWrite = temperature.Write();
	GpuVolumetricFluidTexture3D& obstacle = grid.GetObstacle();
	if (!velocityRead.IsValid() || !velocityWrite.IsValid() ||
		!densityRead.IsValid() || !densityWrite.IsValid() ||
		!temperatureRead.IsValid() || !temperatureWrite.IsValid() || !obstacle.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, velocityRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, densityRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, temperatureRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, velocityWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	GpuVolumetricFluidGridResource::Transition(
		commandList, densityWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	GpuVolumetricFluidGridResource::Transition(
		commandList, temperatureWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	descriptors->PreDispatch();
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
	commandList->SetComputeRootDescriptorTable(
		2, descriptors->GetGPUDescriptorHandle(velocityRead.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(
		3, descriptors->GetGPUDescriptorHandle(densityRead.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(
		4, descriptors->GetGPUDescriptorHandle(temperatureRead.computeSrvIndex));
	commandList->SetComputeRootShaderResourceView(5, emitterAllocation.gpuAddress);
	commandList->SetComputeRootDescriptorTable(
		6, descriptors->GetGPUDescriptorHandle(velocityWrite.uavIndex));
	commandList->SetComputeRootDescriptorTable(
		7, descriptors->GetGPUDescriptorHandle(densityWrite.uavIndex));
	commandList->SetComputeRootDescriptorTable(
		8, descriptors->GetGPUDescriptorHandle(temperatureWrite.uavIndex));
	commandList->SetComputeRootDescriptorTable(
		9, descriptors->GetGPUDescriptorHandle(obstacle.computeSrvIndex));

	const GpuVolumetricFluidGridDesc& gridDesc = grid.GetGridDesc();
	const uint32_t groupCountX = (gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY = (gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	const uint32_t groupCountZ = (gridDesc.depth + kThreadGroupSizeZ - 1u) / kThreadGroupSizeZ;
	commandList->Dispatch(groupCountX, groupCountY, groupCountZ);

	// Solid voxelはShader側で3 fieldとも0化し、全UAV完了後に世代をまとめて切り替える。
	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, velocityWrite.resource.Get());
	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, densityWrite.resource.Get());
	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, temperatureWrite.resource.Get());
	GpuVolumetricFluidGridResource::Transition(
		commandList, velocityWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, densityWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, temperatureWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();
	density.Swap();
	temperature.Swap();

	++dispatchCount_;
	lastInjectedSourceCount_ = static_cast<uint32_t>(activeSources.size());
	return true;
}

bool GpuVolumetricFluidEmitterInjectionPass::ValidateDispatchContext(
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidSimulationDesc& simulationDesc,
	const GpuVolumetricFluidDomainMapping& domain,
	float deltaTime,
	float elapsedTime) const
{
	if (!IsInitialized() || !grid.IsInitialized() || !simulationDesc.IsValid() || !domain.IsValid() ||
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

bool GpuVolumetricFluidEmitterInjectionPass::CreateRootSignature()
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
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
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

bool GpuVolumetricFluidEmitterInjectionPass::CreatePipelineState()
{
	const ShaderDescriptor& shaderDesc = GpuVolumetricFluidShaderManifest::GetCompute(
		GpuVolumetricFluidComputeShaderId::EmitterInjection);
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

	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
	return SUCCEEDED(dxCommon_->GetDevice()->CreateComputePipelineState(
		&pipelineDesc, IID_PPV_ARGS(&pipelineState_)));
}

} // namespace Ken4lowEngine
