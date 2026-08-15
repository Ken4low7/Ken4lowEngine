#include "GpuVolumetricFluidRaymarchRenderer.h"

#include <BlendStateFactory.h>
#include <CameraManager.h>
#include <DirectXCommon.h>
#include <GpuVolumetricFluidShaderManifest.h>
#include <LogString.h>
#include <SRVManager.h>
#include <ShaderCompiler.h>

#include <string>

namespace Ken4lowEngine
{

bool GpuVolumetricFluidRaymarchRenderer::Initialize()
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

	rootSignature_->SetName(L"GpuVolumetricFluid.Raymarch.RootSignature");
	pipelineState_->SetName(L"GpuVolumetricFluid.Raymarch.PSO");
	return true;
}

void GpuVolumetricFluidRaymarchRenderer::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
	drawCount_ = 0;
}

bool GpuVolumetricFluidRaymarchRenderer::Draw(
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidDomainMapping& domain,
	const GpuVolumetricFluidRenderDesc& renderDesc)
{
	CameraManager* cameraManager = CameraManager::GetInstance();
	if (cameraManager == nullptr)
	{
		return false;
	}

	// Queue実行時のActive View/Positionを解決し、ReflectionなどのRenderViewOverrideにも追従する。
	return Draw(
		grid,
		domain,
		renderDesc,
		cameraManager->GetActiveViewProjectionMatrix(),
		cameraManager->GetActiveCameraPosition());
}

bool GpuVolumetricFluidRaymarchRenderer::Draw(
	GpuVolumetricFluidGridResource& grid,
	const GpuVolumetricFluidDomainMapping& domain,
	const GpuVolumetricFluidRenderDesc& renderDesc,
	const Matrix4x4& viewProjection,
	const Vector3& cameraPosition)
{
	if (!IsInitialized() || !grid.IsInitialized() || !domain.IsValid() || !renderDesc.IsValid())
	{
		return false;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	if (commandList == nullptr)
	{
		return false;
	}

	GpuVolumetricFluidTexture3D& density = grid.GetDensity().Read();
	GpuVolumetricFluidTexture3D& temperature = grid.GetTemperature().Read();
	GpuVolumetricFluidTexture3D& obstacle = grid.GetObstacle();
	if (!density.IsValid() || !temperature.IsValid() || !obstacle.IsValid())
	{
		return false;
	}

	const GpuVolumetricFluidRenderConstants constants = BuildGpuVolumetricFluidRenderConstants(
		renderDesc,
		domain,
		grid.GetGridDesc(),
		viewProjection,
		cameraPosition);
	const FrameUploadArena::Allocation constantAllocation =
		dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
	if (!constantAllocation.IsValid())
	{
		return false;
	}

	GpuVolumetricFluidGridResource::Transition(
		commandList, density, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, temperature, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GpuVolumetricFluidGridResource::Transition(
		commandList, obstacle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	SRVManager* descriptors = SRVManager::GetInstance();
	descriptors->PreDraw();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootConstantBufferView(0, constantAllocation.gpuAddress);
	commandList->SetGraphicsRootDescriptorTable(
		1, descriptors->GetGPUDescriptorHandle(density.srvIndex));
	commandList->SetGraphicsRootDescriptorTable(
		2, descriptors->GetGPUDescriptorHandle(temperature.srvIndex));
	commandList->SetGraphicsRootDescriptorTable(
		3, descriptors->GetGPUDescriptorHandle(obstacle.srvIndex));
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Unit cubeの12 trianglesをSV_VertexIDで生成し、専用Vertex BufferなしでVolume proxyを描く。
	commandList->DrawInstanced(36, 1, 0, 0);
	++drawCount_;
	return true;
}

bool GpuVolumetricFluidRaymarchRenderer::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[4]{};
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
		rootParameters[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = _countof(rootParameters);
	desc.pParameters = rootParameters;
	desc.NumStaticSamplers = 1;
	desc.pStaticSamplers = &sampler;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	const HRESULT serializeResult = D3D12SerializeRootSignature(
		&desc,
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

bool GpuVolumetricFluidRaymarchRenderer::CreatePipelineState()
{
	const ShaderDescriptor& vertexDesc = GpuVolumetricFluidShaderManifest::GetGraphics(
		GpuVolumetricFluidGraphicsShaderId::RaymarchVS);
	const ShaderDescriptor& pixelDesc = GpuVolumetricFluidShaderManifest::GetGraphics(
		GpuVolumetricFluidGraphicsShaderId::RaymarchPS);
	if (vertexDesc.stage != ShaderStage::Vertex || pixelDesc.stage != ShaderStage::Pixel ||
		vertexDesc.rootSignature != RootSignatureType::GpuVolumetricFluid ||
		pixelDesc.rootSignature != RootSignatureType::GpuVolumetricFluid)
	{
		return false;
	}

	ComPtr<IDxcBlob> vertexShader = ShaderCompiler::CompileShader(
		vertexDesc, dxCommon_->GetDXCCompilerManager());
	ComPtr<IDxcBlob> pixelShader = ShaderCompiler::CompileShader(
		pixelDesc, dxCommon_->GetDXCCompilerManager());
	if (!vertexShader || !pixelShader)
	{
		return false;
	}

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.DepthClipEnable = TRUE;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.InputLayout = { nullptr, 0 };
	pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	pipelineDesc.BlendState.RenderTarget[0] =
		BlendStateFactory::GetInstance()->GetBlendDesc(BlendMode::kBlendModeNormal);
	pipelineDesc.RasterizerState = rasterizerDesc;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineDesc.SampleDesc.Count = 1;

	return SUCCEEDED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&pipelineState_)));
}

} // namespace Ken4lowEngine
