#include "PixelateEffect.h"
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
	///				　			初期化処理
	/// -------------------------------------------------------------
	void PixelateEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;

		// CS 用ルートシグネチャ & パイプライン 
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::PixelateCS, computeRootSignature_.Get());

		// 定数バッファ
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(PixelateSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&pixelateSetting_));

		// デフォルト設定
		pixelateSetting_->screenSize = { (float)WinApp::kClientWidth,(float)WinApp::kClientHeight };
		pixelateSetting_->blockSize = 8.0f;  // 8x8 ピクセルブロック
		pixelateSetting_->strength = 1.0f;  // 最初はフルピクセル化

		// 名前の設定
		constantBuffer_->SetName(L"PixelateEffect ConstantBuffer");
		computePipelineState_->SetName(L"PixelateEffect PipelineState");
		computeRootSignature_->SetName(L"PixelateEffect RootSignature");
	}

	void PixelateEffect::Finalize()
	{
		// Mapしているポインタを無効化（Unmapは安全のため）
		if (constantBuffer_) {
			constantBuffer_->Unmap(0, nullptr);
		}
		pixelateSetting_ = nullptr;

		// D3Dリソース解放
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();

		// 借り物参照を切る
		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///				　		SRV/UAV/DSV適用処理
	/// -------------------------------------------------------------
	void PixelateEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;

		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());

		// t0: 入力 (SRV), u0: 出力 (UAV), b0: CBV
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress());

		// 画面サイズに合わせて Dispatch（GrayScale と同じ）
		const uint32_t threadGroupSizeX = 8;
		const uint32_t threadGroupSizeY = 8;

		// Compute Dispatch範囲を現在のGameViewportRenderTargetサイズに合わせる。
		uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();

		uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	/// -------------------------------------------------------------
	///				　		　　ImGui描画処理
	/// -------------------------------------------------------------
	void PixelateEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		// PostEffect Settings内で同名Strengthが増えても警告が出ないよう、内部IDをEffect名で分ける。
		ImGui::SliderFloat("Block Size##PixelateEffect", &pixelateSetting_->blockSize, 1.0f, 128.0f);
		ImGui::SliderFloat("Strength##PixelateEffect", &pixelateSetting_->strength, 0.0f, 1.0f);
#endif
	}

} // namespace Ken4lowEngine
