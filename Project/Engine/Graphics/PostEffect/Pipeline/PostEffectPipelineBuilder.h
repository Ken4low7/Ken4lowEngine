#pragma once
#include "DX12Include.h"
#include "PostEffectShaderManifest.h"

namespace Ken4lowEngine
{

	class DirectXCommon;

	/// -------------------------------------------------------------
	///                 ポストエフェクトパイプラインビルダー
	/// -------------------------------------------------------------
	class PostEffectPipelineBuilder
	{
	public:
		void Initialize(DirectXCommon* dxCommon);
		void Finalize();

		ComPtr<ID3D12RootSignature> CreateRootSignature();

		ComPtr<ID3D12PipelineState> CreateGraphicsPipeline(
			PostEffectGraphicsShaderId pixelShaderId,
			ID3D12RootSignature* rootSignature,
			bool enableDepth = false);

		ComPtr<ID3D12RootSignature> CreateComputeRootSignature();

		ComPtr<ID3D12PipelineState> CreateComputePipeline(
			PostEffectComputeShaderId computeShaderId,
			ID3D12RootSignature* rootSignature);

		void BuildCopyPipeline();

		ComPtr<ID3D12RootSignature> GetCopyRootSignature() const { return copyRootSignature_; }
		ComPtr<ID3D12PipelineState> GetCopyPipelineState() const { return copyPipelineState_; }

	private:
		static void ValidatePostEffectGraphicsContracts(
			const ShaderDescriptor& vsDesc,
			const ShaderDescriptor& psDesc,
			ID3D12RootSignature* rootSignature);

		static void ValidatePostEffectComputeContracts(
			const ShaderDescriptor& csDesc,
			ID3D12RootSignature* rootSignature);

	private:
		DirectXCommon* dxCommon_ = nullptr;

		ComPtr<ID3D12RootSignature> copyRootSignature_;
		ComPtr<ID3D12PipelineState> copyPipelineState_;
	};

} // namespace Ken4lowEngine