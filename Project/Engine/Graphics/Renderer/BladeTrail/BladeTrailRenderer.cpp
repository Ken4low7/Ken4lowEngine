#include "BladeTrailRenderer.h"

#include "BladeTrailShaderManifest.h"
#include "BlendStateFactory.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "PostEffectManager.h"
#include "ShaderCompiler.h"
#include "SRVManager.h"
#include "TextureManager.h"

#include <cassert>
#include <cstring>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr const char* kFallbackBladeTrailTexture = "Effects/white.dds";

		struct BladeTrailViewConstants
		{
			Matrix4x4 viewProjection = Matrix4x4::MakeIdentity();
		};
	}

	BladeTrailRenderer* BladeTrailRenderer::GetInstance()
	{
		static BladeTrailRenderer instance;
		return &instance;
	}

	void BladeTrailRenderer::Acquire()
	{
		++referenceCount_;
		if (!initialized_)
		{
			Initialize();
		}
	}

	void BladeTrailRenderer::Release()
	{
		if (referenceCount_ == 0)
		{
			return;
		}

		--referenceCount_;
		if (referenceCount_ == 0)
		{
			Finalize();
		}
	}

	void BladeTrailRenderer::Initialize()
	{
		if (initialized_)
		{
			return;
		}

		dxCommon_ = DirectXCommon::GetInstance();
		if (!dxCommon_ || !dxCommon_->GetDevice())
		{
			return;
		}

		CreateRootSignature();
		for (uint32_t modeIndex = 0; modeIndex < static_cast<uint32_t>(BlendMode::kcountOfBlendMode); ++modeIndex)
		{
			CreatePipelineState(static_cast<BlendMode>(modeIndex));
		}
		rootSignature_->SetName(L"BladeTrailRenderer_RootSignature");
		initialized_ = true;
	}

	void BladeTrailRenderer::Finalize()
	{
		for (auto& pipelineState : pipelineStates_)
		{
			pipelineState.Reset();
		}
		rootSignature_.Reset();
		dxCommon_ = nullptr;
		initialized_ = false;
	}

	void BladeTrailRenderer::CreateRootSignature()
	{
		D3D12_DESCRIPTOR_RANGE textureRange{};
		textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureRange.NumDescriptors = 1;
		textureRange.BaseShaderRegister = 0;
		textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER parameters[2]{};
		parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameters[0].Descriptor.ShaderRegister = 0;
		parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameters[1].DescriptorTable.NumDescriptorRanges = 1;
		parameters[1].DescriptorTable.pDescriptorRanges = &textureRange;
		parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC sampler{};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		desc.pParameters = parameters;
		desc.NumParameters = _countof(parameters);
		desc.pStaticSamplers = &sampler;
		desc.NumStaticSamplers = 1;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		const HRESULT serializeResult = D3D12SerializeRootSignature(
			&desc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signature,
			&error);
		assert(SUCCEEDED(serializeResult));
		assert(signature != nullptr);

		const HRESULT createResult = dxCommon_->GetDevice()->CreateRootSignature(
			0,
			signature->GetBufferPointer(),
			signature->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature_));
		assert(SUCCEEDED(createResult));
	}

	void BladeTrailRenderer::CreatePipelineState(BlendMode blendMode)
	{
		D3D12_INPUT_ELEMENT_DESC inputElements[3]{};
		inputElements[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElements[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElements[2] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

		D3D12_INPUT_LAYOUT_DESC inputLayout{};
		inputLayout.pInputElementDescs = inputElements;
		inputLayout.NumElements = _countof(inputElements);

		const ShaderDescriptor& vertexDesc = BladeTrailShaderManifest::GetGraphics(BladeTrailGraphicsShaderId::Vertex);
		const ShaderDescriptor& pixelDesc = BladeTrailShaderManifest::GetGraphics(BladeTrailGraphicsShaderId::Pixel);
		ComPtr<IDxcBlob> vertexShader = ShaderCompiler::CompileShader(vertexDesc, dxCommon_->GetDXCCompilerManager());
		ComPtr<IDxcBlob> pixelShader = ShaderCompiler::CompileShader(pixelDesc, dxCommon_->GetDXCCompilerManager());
		assert(vertexShader != nullptr);
		assert(pixelShader != nullptr);

		D3D12_RASTERIZER_DESC rasterizer{};
		rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizer.CullMode = D3D12_CULL_MODE_NONE;
		rasterizer.DepthClipEnable = TRUE;

		D3D12_DEPTH_STENCIL_DESC depthStencil{};
		depthStencil.DepthEnable = TRUE;
		depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = rootSignature_.Get();
		pipelineDesc.InputLayout = inputLayout;
		pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		pipelineDesc.BlendState.RenderTarget[0] = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode);
		pipelineDesc.RasterizerState = rasterizer;
		pipelineDesc.DepthStencilState = depthStencil;
		pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = 1;

		const size_t index = static_cast<size_t>(blendMode);
		const HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
			&pipelineDesc,
			IID_PPV_ARGS(&pipelineStates_[index]));
		assert(SUCCEEDED(result));
		const std::wstring name = L"BladeTrailRenderer_PSO_" + std::to_wstring(index);
		pipelineStates_[index]->SetName(name.c_str());
	}

	ID3D12PipelineState* BladeTrailRenderer::GetPipelineState(BlendMode blendMode) const
	{
		const size_t index = static_cast<size_t>(blendMode);
		if (index < pipelineStates_.size() && pipelineStates_[index])
		{
			return pipelineStates_[index].Get();
		}
		return pipelineStates_[static_cast<size_t>(BlendMode::kBlendModeAdd)].Get();
	}

	void BladeTrailRenderer::Draw(std::span<const BladeTrailVertex> vertices, const std::string& texturePath, BlendMode blendMode)
	{
		if (!initialized_ || !dxCommon_ || vertices.size() < 6)
		{
			return;
		}

		const std::string resolvedTexture = texturePath.empty() ? kFallbackBladeTrailTexture : texturePath;
		TextureManager::GetInstance()->LoadTexture(resolvedTexture);

		FrameUploadArena& uploadArena = dxCommon_->GetFrameUploadArena();
		const size_t vertexBytes = vertices.size_bytes();
		const FrameUploadArena::Allocation vertexAllocation = uploadArena.Allocate(vertexBytes, alignof(BladeTrailVertex));
		if (!vertexAllocation.IsValid())
		{
			return;
		}
		std::memcpy(vertexAllocation.cpuAddress, vertices.data(), vertexBytes);

		BladeTrailViewConstants viewConstants{};
		viewConstants.viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const FrameUploadArena::Allocation viewAllocation = uploadArena.AllocateConstant(viewConstants);
		if (!viewAllocation.IsValid())
		{
			return;
		}

		D3D12_VERTEX_BUFFER_VIEW vertexView{};
		vertexView.BufferLocation = vertexAllocation.gpuAddress;
		vertexView.SizeInBytes = static_cast<UINT>(vertexBytes);
		vertexView.StrideInBytes = sizeof(BladeTrailVertex);

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		PostEffectManager::GetInstance()->BindSceneRenderTarget();
		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(GetPipelineState(blendMode));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vertexView);
		commandList->SetGraphicsRootConstantBufferView(0, viewAllocation.gpuAddress);

		SRVManager::GetInstance()->PreDraw();
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(
			commandList,
			1,
			TextureManager::GetInstance()->GetSrvHandleGPU(resolvedTexture));

		// N-point履歴はCPU側で三角形列へ展開済みなので1 drawだけで帯全体を描画する。
		commandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
	}
}
