#include "GrayScaleEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <PostEffectShaderManifest.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <PostEffectManager.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                         初期化処理
	/// -------------------------------------------------------------
	void GrayScaleEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		assert(dxCommon != nullptr);
		assert(builder != nullptr);

		dxCommon_ = dxCommon;

		// ルートシグネチャの生成（コンピュート用）
		computeRootSignature_ = builder->CreateComputeRootSignature();
		assert(computeRootSignature_ != nullptr);

		// パイプラインの生成（コンピュート用）
		computePipelineState_ = builder->CreateComputePipeline(
			PostEffectComputeShaderId::GrayScaleCS,
			computeRootSignature_.Get());
		assert(computePipelineState_ != nullptr);

		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(
			dxCommon_->GetDevice(),
			sizeof(GrayScaleSetting));
		assert(constantBuffer_ != nullptr);

		// データの設定
		HRESULT hr = constantBuffer_->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&grayScaleSetting_));
		assert(SUCCEEDED(hr));
		assert(grayScaleSetting_ != nullptr);

		// グレイスケールエフェクトの設定
		grayScaleSetting_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

		// 名前の設定
		constantBuffer_->SetName(L"GrayScaleEffect ConstantBuffer");
		computePipelineState_->SetName(L"GrayScaleEffect PipelineState");
		computeRootSignature_->SetName(L"GrayScaleEffect RootSignature");
	}

	void GrayScaleEffect::Finalize()
	{
		if (constantBuffer_ && grayScaleSetting_)
		{
			constantBuffer_->Unmap(0, nullptr);
			grayScaleSetting_ = nullptr;
		}

		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///             コンピュートシェーダーによる適用処理
	/// -------------------------------------------------------------
	void GrayScaleEffect::Apply(
		ID3D12GraphicsCommandList* commandList,
		uint32_t srvIndex,
		uint32_t uavIndex,
		uint32_t dsvIndex)
	{
		(void)dsvIndex;

		assert(commandList != nullptr);
		assert(dxCommon_ != nullptr);
		assert(computeRootSignature_ != nullptr);
		assert(computePipelineState_ != nullptr);
		assert(constantBuffer_ != nullptr);
		assert(grayScaleSetting_ != nullptr);

		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());

		// t0 : 入力SRV
		commandList->SetComputeRootDescriptorTable(
			0,
			UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));

		// u0 : 出力UAV
		commandList->SetComputeRootDescriptorTable(
			1,
			UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));

		// b0 : 定数バッファ
		commandList->SetComputeRootConstantBufferView(
			2,
			constantBuffer_->GetGPUVirtualAddress());

		const uint32_t threadGroupSizeX = 8;
		const uint32_t threadGroupSizeY = 8;

		// Compute Dispatch範囲を現在のGameViewportRenderTargetサイズに合わせる。
		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();

		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	/// -------------------------------------------------------------
	///                         ImGui描画処理
	/// -------------------------------------------------------------
	void GrayScaleEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (grayScaleSetting_ == nullptr)
		{
			return;
		}

		ImGui::ColorEdit4("GrayScale Color", &grayScaleSetting_->color.x);
		ImGui::Text("GrayScale Effect");
		ImGui::Separator();
		ImGui::Text("Intensity: %f", grayScaleSetting_->color.x);
		ImGui::Separator();
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
