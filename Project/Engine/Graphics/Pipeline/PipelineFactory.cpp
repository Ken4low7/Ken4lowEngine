#include "PipelineFactory.h"

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace Ken4lowEngine
{
	namespace
	{
		template <class T>
		void AppendScalar(std::string& key, const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			key.append(reinterpret_cast<const char*>(&value), sizeof(T));
		}

		void AppendBytes(std::string& key, const void* data, std::size_t size)
		{
			const uint64_t length = static_cast<uint64_t>(size);
			AppendScalar(key, length);
			if (data && size > 0)
			{
				key.append(static_cast<const char*>(data), size);
			}
		}

		void AppendCString(std::string& key, const char* value)
		{
			if (!value)
			{
				AppendBytes(key, nullptr, 0);
				return;
			}
			AppendBytes(key, value, std::strlen(value));
		}

		void AppendShaderBlob(std::string& key, const ShaderBinary& shader)
		{
			if (!shader.blob)
			{
				AppendBytes(key, nullptr, 0);
				return;
			}
			AppendBytes(key, shader.blob->GetBufferPointer(), shader.blob->GetBufferSize());
		}

		void AppendRenderTargetBlend(std::string& key, const D3D12_RENDER_TARGET_BLEND_DESC& desc)
		{
			AppendScalar(key, desc.BlendEnable);
			AppendScalar(key, desc.LogicOpEnable);
			AppendScalar(key, desc.SrcBlend);
			AppendScalar(key, desc.DestBlend);
			AppendScalar(key, desc.BlendOp);
			AppendScalar(key, desc.SrcBlendAlpha);
			AppendScalar(key, desc.DestBlendAlpha);
			AppendScalar(key, desc.BlendOpAlpha);
			AppendScalar(key, desc.LogicOp);
			AppendScalar(key, desc.RenderTargetWriteMask);
		}

		void AppendDepthStencilOp(std::string& key, const D3D12_DEPTH_STENCILOP_DESC& desc)
		{
			AppendScalar(key, desc.StencilFailOp);
			AppendScalar(key, desc.StencilDepthFailOp);
			AppendScalar(key, desc.StencilPassOp);
			AppendScalar(key, desc.StencilFunc);
		}
	}

	PipelineFactory::PipelineFactory(ID3D12Device* device)
		: device_(device)
	{}

	void PipelineFactory::Initialize(ID3D12Device* device)
	{
		{
			std::scoped_lock lock(cacheMutex_);
			graphicsPipelineCache_.clear();
			cacheStats_ = {};
		}
		// Device世代が変わるとPSOを共有できないためInitialize境界でCacheを初期化する。
		device_ = device;
	}

	void PipelineFactory::Finalize()
	{
		ClearCache();
		device_ = nullptr;
	}

	Microsoft::WRL::ComPtr<ID3DBlob> PipelineFactory::SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const
	{
		assert(device_);

		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

		// RootSignature の定義をバイナリ化する
		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

		if (FAILED(hr))
		{
			// 失敗時はデバッグ出力へエラー内容を流す
			if (errorBlob)
			{
				OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
			}
			throw std::runtime_error("Failed to serialize root signature.");
		}

		return signatureBlob;
	}

	std::string PipelineFactory::BuildGraphicsPipelineCacheKey(
		const GraphicsPipelineDesc& desc,
		ID3DBlob* serializedRootSignature) const
	{
		std::string key("Ken4lowGraphicsPSO-v1", sizeof("Ken4lowGraphicsPSO-v1") - 1);
		AppendBytes(
			key,
			serializedRootSignature ? serializedRootSignature->GetBufferPointer() : nullptr,
			serializedRootSignature ? serializedRootSignature->GetBufferSize() : 0);

		AppendShaderBlob(key, desc.shaders.vertexShader);
		AppendShaderBlob(key, desc.shaders.pixelShader);
		AppendShaderBlob(key, desc.shaders.geometryShader);
		AppendShaderBlob(key, desc.shaders.hullShader);
		AppendShaderBlob(key, desc.shaders.domainShader);

		AppendScalar(key, desc.inputLayout.NumElements);
		for (UINT i = 0; i < desc.inputLayout.NumElements; ++i)
		{
			const D3D12_INPUT_ELEMENT_DESC& element = desc.inputLayout.pInputElementDescs[i];
			AppendCString(key, element.SemanticName);
			AppendScalar(key, element.SemanticIndex);
			AppendScalar(key, element.Format);
			AppendScalar(key, element.InputSlot);
			AppendScalar(key, element.AlignedByteOffset);
			AppendScalar(key, element.InputSlotClass);
			AppendScalar(key, element.InstanceDataStepRate);
		}

		AppendScalar(key, desc.blendState.AlphaToCoverageEnable);
		AppendScalar(key, desc.blendState.IndependentBlendEnable);
		for (const D3D12_RENDER_TARGET_BLEND_DESC& renderTarget : desc.blendState.RenderTarget)
		{
			AppendRenderTargetBlend(key, renderTarget);
		}

		AppendScalar(key, desc.rasterizerState.FillMode);
		AppendScalar(key, desc.rasterizerState.CullMode);
		AppendScalar(key, desc.rasterizerState.FrontCounterClockwise);
		AppendScalar(key, desc.rasterizerState.DepthBias);
		AppendScalar(key, desc.rasterizerState.DepthBiasClamp);
		AppendScalar(key, desc.rasterizerState.SlopeScaledDepthBias);
		AppendScalar(key, desc.rasterizerState.DepthClipEnable);
		AppendScalar(key, desc.rasterizerState.MultisampleEnable);
		AppendScalar(key, desc.rasterizerState.AntialiasedLineEnable);
		AppendScalar(key, desc.rasterizerState.ForcedSampleCount);
		AppendScalar(key, desc.rasterizerState.ConservativeRaster);

		AppendScalar(key, desc.depthStencilState.DepthEnable);
		AppendScalar(key, desc.depthStencilState.DepthWriteMask);
		AppendScalar(key, desc.depthStencilState.DepthFunc);
		AppendScalar(key, desc.depthStencilState.StencilEnable);
		AppendScalar(key, desc.depthStencilState.StencilReadMask);
		AppendScalar(key, desc.depthStencilState.StencilWriteMask);
		AppendDepthStencilOp(key, desc.depthStencilState.FrontFace);
		AppendDepthStencilOp(key, desc.depthStencilState.BackFace);

		AppendScalar(key, desc.numRenderTargets);
		for (DXGI_FORMAT format : desc.rtvFormats)
		{
			AppendScalar(key, format);
		}
		AppendScalar(key, desc.dsvFormat);
		AppendScalar(key, desc.primitiveTopologyType);
		AppendScalar(key, desc.sampleMask);
		AppendScalar(key, desc.sampleCount);
		return key;
	}

	PipelineBundle PipelineFactory::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const
	{
		assert(device_);

		const Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = SerializeRootSignature(rootSignatureDesc);
		const std::string cacheKey = BuildGraphicsPipelineCacheKey(desc, signatureBlob.Get());

		std::scoped_lock lock(cacheMutex_);
		++cacheStats_.requestCount;
		if (const auto cached = graphicsPipelineCache_.find(cacheKey); cached != graphicsPipelineCache_.end())
		{
			++cacheStats_.hitCount;
			return cached->second;
		}
		++cacheStats_.missCount;

		PipelineBundle out{};

		// Serialized RootSignature内容もCache Keyに含めるためPointer addressは再利用判定へ入らない。
		HRESULT hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&out.rootSignature));
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create root signature.");
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = out.rootSignature.Get();

		if (desc.shaders.vertexShader.blob)
		{
			psoDesc.VS = {
				desc.shaders.vertexShader.blob->GetBufferPointer(),
				desc.shaders.vertexShader.blob->GetBufferSize()
			};
		}

		if (desc.shaders.pixelShader.blob)
		{
			psoDesc.PS = {
				desc.shaders.pixelShader.blob->GetBufferPointer(),
				desc.shaders.pixelShader.blob->GetBufferSize()
			};
		}

		if (desc.shaders.geometryShader.blob)
		{
			psoDesc.GS = {
				desc.shaders.geometryShader.blob->GetBufferPointer(),
				desc.shaders.geometryShader.blob->GetBufferSize()
			};
		}

		if (desc.shaders.hullShader.blob)
		{
			psoDesc.HS = {
				desc.shaders.hullShader.blob->GetBufferPointer(),
				desc.shaders.hullShader.blob->GetBufferSize()
			};
		}

		if (desc.shaders.domainShader.blob)
		{
			psoDesc.DS = {
				desc.shaders.domainShader.blob->GetBufferPointer(),
				desc.shaders.domainShader.blob->GetBufferSize()
			};
		}

		psoDesc.BlendState = desc.blendState;
		psoDesc.RasterizerState = desc.rasterizerState;
		psoDesc.DepthStencilState = desc.depthStencilState;
		psoDesc.InputLayout = desc.inputLayout;
		psoDesc.PrimitiveTopologyType = desc.primitiveTopologyType;
		psoDesc.NumRenderTargets = desc.numRenderTargets;
		psoDesc.SampleMask = desc.sampleMask;
		psoDesc.SampleDesc.Count = desc.sampleCount;
		psoDesc.DSVFormat = desc.dsvFormat;

		for (UINT i = 0; i < desc.numRenderTargets; ++i)
		{
			psoDesc.RTVFormats[i] = desc.rtvFormats[i];
		}

		hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&out.pipelineState));
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create graphics pipeline state.");
		}

		++cacheStats_.createCount;
		graphicsPipelineCache_.emplace(cacheKey, out);
		cacheStats_.entryCount = static_cast<uint32_t>(graphicsPipelineCache_.size());
		return out;
	}

	void PipelineFactory::ClearCache()
	{
		std::scoped_lock lock(cacheMutex_);
		graphicsPipelineCache_.clear();
		cacheStats_.entryCount = 0;
		++cacheStats_.clearCount;
	}

	PipelineFactory::PipelineCacheStats PipelineFactory::GetCacheStats() const
	{
		std::scoped_lock lock(cacheMutex_);
		PipelineCacheStats stats = cacheStats_;
		stats.entryCount = static_cast<uint32_t>(graphicsPipelineCache_.size());
		return stats;
	}
}