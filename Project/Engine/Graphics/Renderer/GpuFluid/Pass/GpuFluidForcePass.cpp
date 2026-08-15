#include "GpuFluidForcePass.h"

#include <DirectXCommon.h>
#include <GpuFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <string>

namespace Ken4lowEngine
{

bool GpuFluidForcePass::Initialize()
{
	Finalize();
	dxCommon_ = DirectXCommon::GetInstance();
	if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr)
	{
		return false;
	}

	if (!CreateRootSignature() ||
		!CreatePipelineState(GpuFluidComputeShaderId::VorticityCurl, curlPipelineState_, L"GpuFluid.VorticityCurl.PSO") ||
		!CreatePipelineState(GpuFluidComputeShaderId::VorticityConfinement, vorticityPipelineState_, L"GpuFluid.VorticityConfinement.PSO") ||
		!CreatePipelineState(GpuFluidComputeShaderId::Buoyancy, buoyancyPipelineState_, L"GpuFluid.Buoyancy.PSO"))
	{
		Finalize();
		return false;
	}

	rootSignature_->SetName(L"GpuFluid.Force.RootSignature");
	return true;
}

void GpuFluidForcePass::Finalize()
{
	buoyancyPipelineState_.Reset();
	vorticityPipelineState_.Reset();
	curlPipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
}

bool GpuFluidForcePass::DispatchVorticity(
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

	const auto constants = BuildGpuFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const auto allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!allocation.IsValid())
	{
		return false;
	}

	UAVManager::GetInstance()->PreDispatch();
	return DispatchCurl(commandList, grid, allocation.gpuAddress) &&
		DispatchVorticityConfinement(commandList, grid, allocation.gpuAddress);
}

bool GpuFluidForcePass::DispatchBuoyancy(
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

	const auto constants = BuildGpuFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const auto allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!allocation.IsValid())
	{
		return false;
	}

	UAVManager::GetInstance()->PreDispatch();
	return DispatchBuoyancyInternal(commandList, grid, allocation.gpuAddress);
}

bool GpuFluidForcePass::DispatchAll(
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

	const auto constants = BuildGpuFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const auto allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!allocation.IsValid())
	{
		return false;
	}

	UAVManager::GetInstance()->PreDispatch();

	// Curl→Vorticity→Buoyancyを1つのCBで連結し、力適用後のVelocityを再Projectionできるようにする。
	return DispatchCurl(commandList, grid, allocation.gpuAddress) &&
		DispatchVorticityConfinement(commandList, grid, allocation.gpuAddress) &&
		DispatchBuoyancyInternal(commandList, grid, allocation.gpuAddress);
}

bool GpuFluidForcePass::ValidateDispatchContext(
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	float deltaTime) const
{
	if (!IsInitialized() || !grid.IsInitialized() || !simulationDesc.IsValid() || deltaTime <= 0.0f)
	{
		return false;
	}

	const GpuFluidGridDesc& desc = grid.GetGridDesc();
	return desc.width == simulationDesc.grid.width &&
		desc.height == simulationDesc.grid.height &&
		desc.cellSize == simulationDesc.grid.cellSize;
}

bool GpuFluidForcePass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[5]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
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

	// 3種類のForce Shaderで同じRootSignatureを共有し、PSO切替だけで連続Dispatchできるようにする。
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = _countof(rootParameters);
	desc.pParameters = rootParameters;
	desc.NumStaticSamplers = 0;
	desc.pStaticSamplers = nullptr;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob)))
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

bool GpuFluidForcePass::CreatePipelineState(
	GpuFluidComputeShaderId shaderId,
	ComPtr<ID3D12PipelineState>& pipelineState,
	const wchar_t* debugName)
{
	const ShaderDescriptor& shaderDesc = GpuFluidShaderManifest::GetCompute(shaderId);
	if (shaderDesc.stage != ShaderStage::Compute || shaderDesc.rootSignature != RootSignatureType::Compute)
	{
		return false;
	}

	ComPtr<IDxcBlob> shader = ShaderCompiler::CompileShader(shaderDesc, dxCommon_->GetDXCCompilerManager());
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

bool GpuFluidForcePass::DispatchCurl(
	ID3D12GraphicsCommandList* commandList,
	GpuFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuFluidTexture2D& velocity = grid.GetVelocity().Read();
	GpuFluidTexture2D& vorticity = grid.GetVorticity();
	if (!velocity.IsValid() || !vorticity.IsValid())
	{
		return false;
	}

	GpuFluidGridResource::Transition(commandList, velocity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, vorticity, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

	GpuFluidGridResource::InsertUavBarrier(commandList, vorticity.resource.Get());
	GpuFluidGridResource::Transition(commandList, vorticity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	++dispatchCount_;
	return true;
}

bool GpuFluidForcePass::DispatchVorticityConfinement(
	ID3D12GraphicsCommandList* commandList,
	GpuFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuFluidPingPongField& velocity = grid.GetVelocity();
	GpuFluidTexture2D& read = velocity.Read();
	GpuFluidTexture2D& write = velocity.Write();
	GpuFluidTexture2D& vorticity = grid.GetVorticity();
	if (!read.IsValid() || !write.IsValid() || !vorticity.IsValid())
	{
		return false;
	}

	GpuFluidGridResource::Transition(commandList, read, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, vorticity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

	GpuFluidGridResource::InsertUavBarrier(commandList, write.resource.Get());
	GpuFluidGridResource::Transition(commandList, write, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();
	++dispatchCount_;
	return true;
}

bool GpuFluidForcePass::DispatchBuoyancyInternal(
	ID3D12GraphicsCommandList* commandList,
	GpuFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuFluidPingPongField& velocity = grid.GetVelocity();
	GpuFluidTexture2D& read = velocity.Read();
	GpuFluidTexture2D& write = velocity.Write();
	GpuFluidTexture2D& density = grid.GetDensity().Read();
	GpuFluidTexture2D& temperature = grid.GetTemperature().Read();
	if (!read.IsValid() || !write.IsValid() || !density.IsValid() || !temperature.IsValid())
	{
		return false;
	}

	GpuFluidGridResource::Transition(commandList, read, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, density, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, temperature, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(commandList, write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(buoyancyPipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(1, descriptors->GetGPUDescriptorHandle(read.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(2, descriptors->GetGPUDescriptorHandle(density.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(3, descriptors->GetGPUDescriptorHandle(temperature.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(4, descriptors->GetGPUDescriptorHandle(write.uavIndex));
	DispatchGrid(commandList, grid.GetGridDesc());

	GpuFluidGridResource::InsertUavBarrier(commandList, write.resource.Get());
	GpuFluidGridResource::Transition(commandList, write, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();
	++dispatchCount_;
	return true;
}

void GpuFluidForcePass::DispatchGrid(
	ID3D12GraphicsCommandList* commandList,
	const GpuFluidGridDesc& gridDesc) const
{
	const uint32_t groupCountX = (gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY = (gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	commandList->Dispatch(groupCountX, groupCountY, 1);
}

} // namespace Ken4lowEngine
