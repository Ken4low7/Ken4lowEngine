#include "LuminanceOutlineEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <WinApp.h>
#include <PostEffectManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
	void LuminanceOutlineEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;

		// ルートシグネチャの生成（コンピュート用）
		computeRootSignature_ = builder->CreateComputeRootSignature();

		// パイプラインの生成（コンピュート用）
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::LuminanceOutlineCS, computeRootSignature_.Get());

		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LuminanceOutlineSetting));

		// データの設定
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&luminanceOutlineSetting_));

		// アウトラインの設定
		luminanceOutlineSetting_->color = Vector4(0.0f, 0.0f, 0.0f, 1.0f); // アウトラインの色（黒色）
		luminanceOutlineSetting_->texelSize = Vector2(1.0f / static_cast<float>(WinApp::kClientWidth), 1.0f / static_cast<float>(WinApp::kClientHeight));
		luminanceOutlineSetting_->edgeStrength = 1.0f; // エッジ強度
		luminanceOutlineSetting_->threshold = 0.5f; // 閾値

		// 名前の設定
		constantBuffer_->SetName(L"LuminanceOutlineEffect ConstantBuffer");
		computePipelineState_->SetName(L"LuminanceOutlineEffect ComputePipelineState");
		computeRootSignature_->SetName(L"LuminanceOutlineEffect ComputeRootSignature");
	}

	void LuminanceOutlineEffect::Finalize()
	{
		// Mapして保持している生ポインタを無効化（Unmapは安全のため）
		if (constantBuffer_ && luminanceOutlineSetting_) {
			constantBuffer_->Unmap(0, nullptr);
			luminanceOutlineSetting_ = nullptr;
		}

		// D3Dリソース解放
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();

		// 借り物参照を切る（所有してない）
		dxCommon_ = nullptr;
	}


	/// -------------------------------------------------------------
	///						　適用処理
	/// -------------------------------------------------------------
	void LuminanceOutlineEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex; // 未使用

		// コンピュート用のルートシグネチャとPSOを設定
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());

		// SRVとUAVを設定（ディスクリプタテーブル）
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex)); // t0
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex)); // u0

		// CBVを設定（b0）
		commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress()); // b0

		// スレッドグループの数を計算して Dispatch
		const uint32_t threadGroupSizeX = 8;
		const uint32_t threadGroupSizeY = 8;

		// Compute Dispatch範囲を現在のGameViewportRenderTargetサイズに合わせる。
		uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();

		uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

		// コンピュートシェーダの実行
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}


	/// -------------------------------------------------------------
	///						　ImGui描画処理
	/// -------------------------------------------------------------
	void LuminanceOutlineEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::Text("Luminance Outline Effect Settings");
		// BloomEffectやDissolveEffectとThreshold/Edge系ラベルが重なるため、内部IDだけをEffect別に分ける。
		ImGui::ColorEdit4("Outline Color##LuminanceOutlineEffect", &luminanceOutlineSetting_->color.x);
		ImGui::SliderFloat("Edge Strength##LuminanceOutlineEffect", &luminanceOutlineSetting_->edgeStrength, 0.0f, 5.0f);
		ImGui::SliderFloat("Threshold##LuminanceOutlineEffect", &luminanceOutlineSetting_->threshold, 0.0f, 1.0f);
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
