#include "GpuVolumetricFluidVelocityAdvectionPass.h"

#include <DirectXCommon.h>
#include <GpuVolumetricFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <cmath>
#include <string>

namespace Ken4lowEngine
{

bool GpuVolumetricFluidVelocityAdvectionPass::Initialize()
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

	rootSignature_->SetName(L"GpuVolumetricFluidVelocityAdvection.RootSignature");
	pipelineState_->SetName(L"GpuVolumetricFluidVelocityAdvection.PSO");
	dispatchCount_ = 0;
	return true;
}

void GpuVolumetricFluidVelocityAdvectionPass::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
}

bool GpuVolumetricFluidVelocityAdvectionPass::Dispatch(
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidSimulationDesc& simulationDesc,
	float deltaTime,
	float elapsedTime)
{
	if (!IsInitialized() || !grid.IsInitialized() || !simulationDesc.IsValid() ||
		deltaTime <= 0.0f || !std::isfinite(deltaTime) || !std::isfinite(elapsedTime))
	{
		return false;
	}

	const GpuVolumetricFluidGridDesc& gridDesc = grid.GetGridDesc();
	if (gridDesc.width != simulationDesc.grid.width ||
		gridDesc.height != simulationDesc.grid.height ||
		gridDesc.depth != simulationDesc.grid.depth ||
		gridDesc.cellSize != simulationDesc.grid.cellSize)
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
	}

	const GpuVolumetricFluidSimulationConstants constants =
		BuildGpuVolumetricFluidSimulationConstants(simulationDesc, deltaTime, elapsedTime);
	const FrameUploadArena::Allocation constantAllocation =
		dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!constantAllocation.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidPingPongField& velocity = grid.GetVelocity();
	GpuVolumetricFluidTexture3D& read = velocity.Read();
	GpuVolumetricFluidTexture3D& write = velocity.Write();
	GpuVolumetricFluidTexture3D& obstacle = grid.GetObstacle();
	if (!read.IsValid() || !write.IsValid() || !obstacle.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, read, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptorManager = UAVManager::GetInstance();
	descriptorManager->PreDispatch();

	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantAllocation.gpuAddress);
	commandList->SetComputeRootDescriptorTable(
		1, descriptorManager->GetGPUDescriptorHandle(read.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(
		2, descriptorManager->GetGPUDescriptorHandle(obstacle.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(
		3, descriptorManager->GetGPUDescriptorHandle(write.uavIndex));

	const uint32_t groupCountX = (gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY = (gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	const uint32_t groupCountZ = (gridDesc.depth + kThreadGroupSizeZ - 1u) / kThreadGroupSizeZ;
	commandList->Dispatch(groupCountX, groupCountY, groupCountZ);

	// Solid voxelを跨ぐ逆追跡はShader側で止め、Barrier完了後だけ速度世代を切り替える。
	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, write.resource.Get());
	GpuVolumetricFluidGridResource::Transition(
		commandList, write, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();

	++dispatchCount_;
	return true;
}

bool GpuVolumetricFluidVelocityAdvectionPass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[4]{};

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

bool GpuVolumetricFluidVelocityAdvectionPass::CreatePipelineState()
{
	const ShaderDescriptor& shaderDesc = GpuVolumetricFluidShaderManifest::GetCompute(
		GpuVolumetricFluidComputeShaderId::VelocityAdvection);
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
	pipelineDesc.CS = {
		computeShader->GetBufferPointer(),
		computeShader->GetBufferSize()
	};
	return SUCCEEDED(dxCommon_->GetDevice()->CreateComputePipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&pipelineState_)));
}

} // namespace Ken4lowEngine
