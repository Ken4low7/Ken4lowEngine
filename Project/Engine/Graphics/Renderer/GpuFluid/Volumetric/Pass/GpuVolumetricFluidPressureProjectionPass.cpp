#include "GpuVolumetricFluidPressureProjectionPass.h"

#include <DirectXCommon.h>
#include <GpuVolumetricFluidShaderManifest.h>
#include <LogString.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <cmath>
#include <string>

namespace Ken4lowEngine
{

bool GpuVolumetricFluidPressureProjectionPass::Initialize()
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
			GpuVolumetricFluidComputeShaderId::Divergence,
			divergencePipelineState_,
			L"GpuVolumetricFluid.Divergence.PSO") ||
		!CreatePipelineState(
			GpuVolumetricFluidComputeShaderId::PressureJacobi,
			pressureJacobiPipelineState_,
			L"GpuVolumetricFluid.PressureJacobi.PSO") ||
		!CreatePipelineState(
			GpuVolumetricFluidComputeShaderId::Projection,
			projectionPipelineState_,
			L"GpuVolumetricFluid.Projection.PSO"))
	{
		Finalize();
		return false;
	}

	rootSignature_->SetName(L"GpuVolumetricFluid.PressureProjection.RootSignature");
	dispatchCount_ = 0;
	lastPressureIterationCount_ = 0;
	return true;
}

void GpuVolumetricFluidPressureProjectionPass::Finalize()
{
	projectionPipelineState_.Reset();
	pressureJacobiPipelineState_.Reset();
	divergencePipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	dispatchCount_ = 0;
	lastPressureIterationCount_ = 0;
}

bool GpuVolumetricFluidPressureProjectionPass::Dispatch(
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

	UAVManager::GetInstance()->PreDispatch();
	if (!ClearPressure(commandList, grid) ||
		!DispatchDivergence(commandList, grid, constantAllocation.gpuAddress) ||
		!DispatchPressureJacobi(
			commandList,
			grid,
			constantAllocation.gpuAddress,
			simulationDesc.pressureIterations) ||
		!DispatchProjection(commandList, grid, constantAllocation.gpuAddress))
	{
		return false;
	}

	lastPressureIterationCount_ = simulationDesc.pressureIterations;
	return true;
}

bool GpuVolumetricFluidPressureProjectionPass::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[5]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE srvRanges[3]{};
	for (uint32_t index = 0; index < 3; ++index)
	{
		srvRanges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRanges[index].NumDescriptors = 1;
		srvRanges[index].BaseShaderRegister = index;
		srvRanges[index].RegisterSpace = 0;
		srvRanges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		rootParameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[index + 1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[index + 1].DescriptorTable.pDescriptorRanges = &srvRanges[index];
		rootParameters[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
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

	// Divergence/Jacobi/Projectionでt2の3D Obstacle Maskまで共有し、Solid境界契約を一箇所へ揃える。
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

bool GpuVolumetricFluidPressureProjectionPass::CreatePipelineState(
	GpuVolumetricFluidComputeShaderId shaderId,
	ComPtr<ID3D12PipelineState>& pipelineState,
	const wchar_t* debugName)
{
	const ShaderDescriptor& shaderDesc = GpuVolumetricFluidShaderManifest::GetCompute(shaderId);
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
	if (FAILED(dxCommon_->GetDevice()->CreateComputePipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&pipelineState))))
	{
		return false;
	}

	pipelineState->SetName(debugName);
	return true;
}

bool GpuVolumetricFluidPressureProjectionPass::ClearPressure(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidGridResource& grid)
{
	if (commandList == nullptr)
	{
		return false;
	}

	GpuVolumetricFluidPingPongField& pressure = grid.GetPressure();
	pressure.Reset();
	UAVManager* descriptors = UAVManager::GetInstance();
	constexpr float kClearValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	for (GpuVolumetricFluidTexture3D& texture : pressure.textures)
	{
		if (!texture.IsValid())
		{
			return false;
		}

		GpuVolumetricFluidGridResource::Transition(
			commandList,
			texture,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ClearUnorderedAccessViewFloat(
			descriptors->GetGPUDescriptorHandle(texture.uavIndex),
			descriptors->GetClearCPUDescriptorHandle(texture.uavIndex),
			texture.resource.Get(),
			kClearValue,
			0,
			nullptr);
		GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, texture.resource.Get());
	}

	return true;
}

bool GpuVolumetricFluidPressureProjectionPass::DispatchDivergence(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuVolumetricFluidTexture3D& velocity = grid.GetVelocity().Read();
	GpuVolumetricFluidTexture3D& divergence = grid.GetDivergence();
	GpuVolumetricFluidTexture3D& obstacle = grid.GetObstacle();
	if (!velocity.IsValid() || !divergence.IsValid() || !obstacle.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, velocity, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, divergence, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	const auto velocitySrv = descriptors->GetGPUDescriptorHandle(velocity.computeSrvIndex);
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(divergencePipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(1, velocitySrv);
	commandList->SetComputeRootDescriptorTable(2, velocitySrv); // Divergenceではt1未使用なので有効SRVで埋める。
	commandList->SetComputeRootDescriptorTable(3, descriptors->GetGPUDescriptorHandle(obstacle.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(4, descriptors->GetGPUDescriptorHandle(divergence.uavIndex));
	DispatchGrid(commandList, grid.GetGridDesc());

	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, divergence.resource.Get());
	GpuVolumetricFluidGridResource::Transition(
		commandList, divergence, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	++dispatchCount_;
	return true;
}

bool GpuVolumetricFluidPressureProjectionPass::DispatchPressureJacobi(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
	uint32_t iterationCount)
{
	if (iterationCount == 0)
	{
		return false;
	}

	GpuVolumetricFluidTexture3D& divergence = grid.GetDivergence();
	GpuVolumetricFluidTexture3D& obstacle = grid.GetObstacle();
	GpuVolumetricFluidPingPongField& pressure = grid.GetPressure();
	if (!divergence.IsValid() || !obstacle.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, divergence, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	UAVManager* descriptors = UAVManager::GetInstance();
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pressureJacobiPipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(1, descriptors->GetGPUDescriptorHandle(divergence.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(3, descriptors->GetGPUDescriptorHandle(obstacle.computeSrvIndex));

	for (uint32_t iteration = 0; iteration < iterationCount; ++iteration)
	{
		GpuVolumetricFluidTexture3D& read = pressure.Read();
		GpuVolumetricFluidTexture3D& write = pressure.Write();
		if (!read.IsValid() || !write.IsValid())
		{
			return false;
		}

		GpuVolumetricFluidGridResource::Transition(
			commandList, read, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		GpuVolumetricFluidGridResource::Transition(
			commandList, write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->SetComputeRootDescriptorTable(2, descriptors->GetGPUDescriptorHandle(read.computeSrvIndex));
		commandList->SetComputeRootDescriptorTable(4, descriptors->GetGPUDescriptorHandle(write.uavIndex));
		DispatchGrid(commandList, grid.GetGridDesc());

		GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, write.resource.Get());
		GpuVolumetricFluidGridResource::Transition(
			commandList, write, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		pressure.Swap();
		++dispatchCount_;
	}

	return true;
}

bool GpuVolumetricFluidPressureProjectionPass::DispatchProjection(
	ID3D12GraphicsCommandList* commandList,
	GpuVolumetricFluidGridResource& grid,
	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
	GpuVolumetricFluidPingPongField& velocity = grid.GetVelocity();
	GpuVolumetricFluidTexture3D& velocityRead = velocity.Read();
	GpuVolumetricFluidTexture3D& velocityWrite = velocity.Write();
	GpuVolumetricFluidTexture3D& pressure = grid.GetPressure().Read();
	GpuVolumetricFluidTexture3D& obstacle = grid.GetObstacle();
	if (!velocityRead.IsValid() || !velocityWrite.IsValid() || !pressure.IsValid() || !obstacle.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, velocityRead, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, pressure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, obstacle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, velocityWrite, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	UAVManager* descriptors = UAVManager::GetInstance();
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(projectionPipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
	commandList->SetComputeRootDescriptorTable(1, descriptors->GetGPUDescriptorHandle(velocityRead.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(2, descriptors->GetGPUDescriptorHandle(pressure.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(3, descriptors->GetGPUDescriptorHandle(obstacle.computeSrvIndex));
	commandList->SetComputeRootDescriptorTable(4, descriptors->GetGPUDescriptorHandle(velocityWrite.uavIndex));
	DispatchGrid(commandList, grid.GetGridDesc());

	GpuVolumetricFluidGridResource::InsertUavBarrier(commandList, velocityWrite.resource.Get());
	GpuVolumetricFluidGridResource::Transition(
		commandList, velocityWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	velocity.Swap();
	++dispatchCount_;
	return true;
}

void GpuVolumetricFluidPressureProjectionPass::DispatchGrid(
	ID3D12GraphicsCommandList* commandList,
	const GpuVolumetricFluidGridDesc& gridDesc) const
{
	const uint32_t groupCountX =
		(gridDesc.width + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
	const uint32_t groupCountY =
		(gridDesc.height + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;
	const uint32_t groupCountZ =
		(gridDesc.depth + kThreadGroupSizeZ - 1u) / kThreadGroupSizeZ;
	commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

} // namespace Ken4lowEngine
