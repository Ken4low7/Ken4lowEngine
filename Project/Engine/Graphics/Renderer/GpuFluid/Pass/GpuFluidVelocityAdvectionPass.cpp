#include "GpuFluidVelocityAdvectionPass.h"

#include <DirectXCommon.h>
#include <GpuFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <string>

namespace Ken4lowEngine
{

bool GpuFluidVelocityAdvectionPass::Initialize()
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

	rootSignature_->SetName(L"GpuFluidVelocityAdvection.RootSignature");
	pipelineState_->SetName(L"GpuFluidVelocityAdvection.PSO");
	dispatchCount_ = 0;
	return true;
}

void GpuFluidVelocityAdvectionPass::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
}

bool GpuFluidVelocityAdvectionPass::Dispatch(
	GpuFluidGridResource& grid,
	const GpuFluidSimulationDesc& simulationDesc,
	float deltaTime,
	float elapsedTime)
{
	if (!IsInitialized() || !grid.IsInitialized() || !simulationDesc.IsValid() || deltaTime <= 0.0f)
	{
		return false;
	}

	const GpuFluidGridDesc& gridDesc = grid.GetGridDesc();
	if (gridDesc.width != simulationDesc.grid.width ||
		gridDesc.height != simulationDesc.grid.height)
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

	GpuFluidPingPongField& velocity = grid.GetVelocity();
	GpuFluidTexture2D& read = velocity.Read();
	GpuFluidTexture2D& write = velocity.Write();
	if (!read.IsValid() || !write.IsValid())
	{
		return false;
	}

	GpuFluidGridResource::Transition(
		commandList,
		read,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuFluidGridResource::Transition(
		commandList,
		write,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptorManager = UAVManager::GetInstance();
	descriptorManager->PreDispatch();

	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantAllocation.gpuAddress);
	commandList->SetComputeRootDescriptorTable(
		1,
		descriptorManager->GetGPUDescriptorHandle(read.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(
		2,
		descriptorManager->GetGPUDescriptorHandle(write.uavIndex));

	const uint32_t groupCountX =
		(gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY =
		(gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	commandList->Dispatch(groupCountX, groupCountY, 1);

	// 書き込み完了を次Passへ可視化してから、出力Textureを新しいRead側へ昇格させる。
	GpuFluidGridResource::InsertUavBarrier(commandList, write.resource.Get());
	GpuFluidGridResource::Transition(
		commandList,
		write,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();

	++dispatchCount_;
	return true;
}

bool GpuFluidVelocityAdvectionPass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[3]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE srvRange{};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 1;
	uavRange.BaseShaderRegister = 0;
	uavRange.RegisterSpace = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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

	const HRESULT createResult = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	return SUCCEEDED(createResult);
}

bool GpuFluidVelocityAdvectionPass::CreatePipelineState()
{
	const ShaderDescriptor& shaderDesc =
		GpuFluidShaderManifest::GetCompute(GpuFluidComputeShaderId::VelocityAdvection);
	if (shaderDesc.stage != ShaderStage::Compute ||
		shaderDesc.rootSignature != RootSignatureType::Compute)
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
	pipelineDesc.CS =
	{
		computeShader->GetBufferPointer(),
		computeShader->GetBufferSize()
	};

	const HRESULT createResult = dxCommon_->GetDevice()->CreateComputePipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&pipelineState_));
	return SUCCEEDED(createResult);
}

} // namespace Ken4lowEngine
