// Wireframe の RootSignature と PSO 生成をまとめる。
#include "Wireframe.h"
#include "LogString.h"
#include "DirectXCommon.h"
#include "BlendStateFactory.h"
#include "ShaderCompiler.h"
#include "WireframeShaderManifest.h"

namespace Ken4lowEngine
{

	void Wireframe::CreateRootSignature(ComPtr<ID3D12RootSignature>& rootSignature)
	{
		HRESULT hr{};
		// RootSignatureの生成
		D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
		descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		// RootParameterの設定
		D3D12_ROOT_PARAMETER rootParameters[1] = {};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // CBVを使う
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // VSでも PSでもGSでも使う
		rootParameters[0].Descriptor.ShaderRegister = 0;				  // レジスタ番号０とバインド
		descriptionRootSignature.pParameters = rootParameters;			   // ルートパラメータ配列へのポインタ
		descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ
		Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
		Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
		hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
		if (FAILED(hr))
		{
			Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
			assert(false);
		}
		hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}

	void Wireframe::CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12PipelineState>& pipelineState)
	{
		HRESULT hr{};
		// ルートシグネチャの生成
		CreateRootSignature(rootSignature);
		// InputLayout
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
		inputLayoutDesc.pInputElementDescs = inputElementDescs;
		inputLayoutDesc.NumElements = _countof(inputElementDescs);
		// BlendStateの設定
		const D3D12_RENDER_TARGET_BLEND_DESC blendDesc = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);
		// RasterizerStateの設定
		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 裏面を非表示にしない
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 三角形の中を塗りつぶす
		// Shaderをコンパイルする
		const ShaderDescriptor& vsDesc = WireframeShaderManifest::Get(WireframeShaderId::WireframeVS);
		const ShaderDescriptor& psDesc = WireframeShaderManifest::Get(WireframeShaderId::WireframePS);
		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);
		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(vsDesc, dxCommon_->GetDXCCompilerManager());
		assert(vertexShaderBlob != nullptr);
		ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(psDesc, dxCommon_->GetDXCCompilerManager());
		assert(pixelShaderBlob != nullptr);
		// DepthStencilStateの設定
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = TRUE;                     // 深度テストを有効にする
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度値の書き込みを無効化
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 小さいか等しい場合に描画
		// PSOの生成
		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
		graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
		graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
		graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };		// 頂点シェーダーを設定
		//graphicsPipelineStateDesc.GS = { geometryShaderBlob->GetBufferPointer(), geometryShaderBlob->GetBufferSize() }; // ジオメトリシェーダーを設定
		graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };		// ピクセルシェーダーを設定
		graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;
		graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
		// レンダーターゲットの設定
		graphicsPipelineStateDesc.NumRenderTargets = 1;
		graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		// プリミティブの種類（指定されたものを設定）
		graphicsPipelineStateDesc.PrimitiveTopologyType = primitiveTopologyType;
		// サンプルマスクとサンプル記述子の設定
		graphicsPipelineStateDesc.SampleDesc.Count = 1;
		graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		// DepthStencilステートの設定
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
		graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		// パイプラインステートオブジェクトの生成
		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState));
		assert(SUCCEEDED(hr));
	}

	void Wireframe::CreateInstancedWirePSO()
	{
		CreateRootSignature(instancedWireRootSignature_);

		// Slot 0は形状ごとの共有頂点、Slot 1は各インスタンスのworld行列と色を受け取る。
		const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
			{ "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};

		const ShaderDescriptor& vsDesc = WireframeShaderManifest::Get(WireframeShaderId::WireframeAABBInstancedVS);
		const ShaderDescriptor& psDesc = WireframeShaderManifest::Get(WireframeShaderId::WireframePS);
		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(vsDesc, dxCommon_->GetDXCCompilerManager());
		ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(psDesc, dxCommon_->GetDXCCompilerManager());
		assert(vertexShaderBlob != nullptr);
		assert(pixelShaderBlob != nullptr);

		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = instancedWireRootSignature_.Get();
		desc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
		desc.BlendState.RenderTarget[0] = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);
		desc.RasterizerState = rasterizerDesc;
		desc.DepthStencilState = depthStencilDesc;
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.SampleDesc.Count = 1;

		const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
			&desc, IID_PPV_ARGS(instancedWirePipelineState_.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}

} // namespace Ken4lowEngine
