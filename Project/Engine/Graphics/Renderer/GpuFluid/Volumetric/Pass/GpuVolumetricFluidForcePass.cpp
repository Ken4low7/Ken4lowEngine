#include "GpuVolumetricFluidForcePass.h"

#include <DirectXCommon.h>
#include <GpuVolumetricFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <cmath>
#include <string>

namespace Ken4lowEngine
{

bool GpuVolumetricFluidForcePass::Initialize()
{
	Finalize();

	dxCommon_ = DirectXCommon::GetInstance();
	if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr)
	{
		Finalize();
		return false;
	}

	if (!CreateRootSignature() ||
		!CreatePipelineState(
			GpuVolumetricFluidComputeShaderId::VorticityCurl,
			curlPipelineState_,
			L"GpuVolumetricFluid.VorticityCurl.PSO") ||
		!CreatePipelineState(
			GpuVolumetricFluidComputeShaderId::VorticityConfinement,
			vorticityPipelineState_,
			L"GpuVolumetricFluid.VorticityConfinement.PSO") ||
		!CreatePipelineState(
			GpuVolumetricFluidComputeShaderId::Buoyancy,
			buoyancyPipelineState_,
			L"GpuVolumetricFluid.Buoyancy.PSO"))
	{
		Finalize();
		return false;
	}

	rootSignature_->SetName(L"GpuVolumetricFluid.Force.RootSignature");
	return true;
}

void GpuVolumetricFluidForcePass::Finalize()
{
	buoyancyPipelineState_.Reset();
	vorticityPipelineState_.Reset();
	curlPipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
}

bool GpuVolumetricFluidForcePass::DispatchVorticity(
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
	return DispatchCurl(commandList, grid, allocation.gpuAddress) &&
		DispatchVorticityConfinement(commandList, grid, allocation.gpuAddress);
}

bool GpuVolumetricFluidForcePass::DispatchBuoyancy(
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
	return DispatchBuoyancyInternal(commandList, grid, allocation.gpuAddress);
}

bool GpuVolumetricFluidForcePass::DispatchAll(
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

	// 3D Curl→Confinement→Buoyancyを同じSimulation CBで連結し、Force適用後の再ProjectionはRuntime側へ任せる。
	return DispatchCurl(commandList, grid, allocation.gpuAddress) &&
		DispatchVorticityConfinement(commandList, grid, allocation.gpuAddress) &&
		DispatchBuoyancyInternal(commandList, grid, allocation.gpuAddress);
}

bool GpuVolumetricFluidForcePass::ValidateDispatchContext(
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

bool GpuVolumetricFluidForcePass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[5]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE srvRanges[3]{};
	for (uint32_t i = 0; i < 3; ++i)
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
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[4].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = _countof(rootParameters);
	desc.pParameters = rootParameters;
	desc.NumStaticSamplers = 0;
	desc.pStaticSamplers = nullptr;
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

bool GpuVolumetricFluidForcePass::CreatePipelineState(
	GpuVolumetricFluidComputeShaderId shaderId,
	ComPtr<ID3D12PipelineState>& pipelineState,
	const wchar_t* debugName)
{
	const ShaderDescriptor& shaderDesc = GpuVolumetricFluidShaderManifest::GetCompute(shaderId);
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
	if (FAILED(dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipelineState))))
	{
		return false;
	}

	pipelineState->SetName(debugName);
	return true;
}

bool GpuVolumetricFluidForcePass::DispatchCurl(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuVolumetricFluidTexture3D& velocity = grid.GetVelocity().Read();
	GpuVolumetricFluidTexture3D& vorticity = grid.GetVorticity();
	if (!velocity.IsValid() || !vorticity.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, velocity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, vorticity, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	const auto velocitySrv = descriptors->GetGPUDescriptorHandle(velocity.computeSrvIndex);
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(curlPipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(1, velocitySrv);
	commandList->SetComputeRootDescriptorTable(2, velocitySrv);
	commandList->SetComputeRootDescriptorTable(3, velocitySrv);
	commandList->SetComputeRootDescriptorTable(4, descriptors->GetGPUDescriptorHandle(vorticity.uavIndex));
	DispatchGrid(commandList, grid.GetGridDesc());

	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, vorticity.resource.Get());
	GpuVolumetricFluidGridResource::Transition(
		commandList, vorticity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	++dispatchCount_;
	return true;
}

bool GpuVolumetricFluidForcePass::DispatchVorticityConfinement(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuVolumetricFluidPingPongField& velocity = grid.GetVelocity();
	GpuVolumetricFluidTexture3D& read = velocity.Read();
	GpuVolumetricFluidTexture3D& write = velocity.Write();
	GpuVolumetricFluidTexture3D& vorticity = grid.GetVorticity();
	if (!read.IsValid() || !write.IsValid() || !vorticity.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, read, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, vorticity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	const auto vorticitySrv = descriptors->GetGPUDescriptorHandle(vorticity.computeSrvIndex);
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(vorticityPipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(1, descriptors->GetGPUDescriptorHandle(read.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(2, vorticitySrv);
	commandList->SetComputeRootDescriptorTable(3, vorticitySrv);
	commandList->SetComputeRootDescriptorTable(4, descriptors->GetGPUDescriptorHandle(write.uavIndex));
	DispatchGrid(commandList, grid.GetGridDesc());

	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, write.resource.Get());
	GpuVolumetricFluidGridResource::Transition(
		commandList, write, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();
	++dispatchCount_;
	return true;
}

bool GpuVolumetricFluidForcePass::DispatchBuoyancyInternal(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuVolumetricFluidPingPongField& velocity = grid.GetVelocity();
	GpuVolumetricFluidTexture3D& read = velocity.Read();
	GpuVolumetricFluidTexture3D& write = velocity.Write();
	GpuVolumetricFluidTexture3D& density = grid.GetDensity().Read();
	GpuVolumetricFluidTexture3D& temperature = grid.GetTemperature().Read();
	if (!read.IsValid() || !write.IsValid() || !density.IsValid() || !temperature.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, read, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, density, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, temperature, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(buoyancyPipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(1, descriptors->GetGPUDescriptorHandle(read.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(2, descriptors->GetGPUDescriptorHandle(density.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(3, descriptors->GetGPUDescriptorHandle(temperature.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(4, descriptors->GetGPUDescriptorHandle(write.uavIndex));
	DispatchGrid(commandList, grid.GetGridDesc());

	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, write.resource.Get());
	GpuVolumetricFluidGridResource::Transition(
		commandList, write, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();
	++dispatchCount_;
	return true;
}

void GpuVolumetricFluidForcePass::DispatchGrid(
	ID3D12GraphicsCommandList* commandList,
	const GpuVolumetricFluidGridDesc& gridDesc) const
{
	const uint32_t groupCountX = (gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY = (gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	const uint32_t groupCountZ = (gridDesc.depth + kThreadGroupSizeZ - 1u) / kThreadGroupSizeZ;
	commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

} // namespace Ken4lowEngine
