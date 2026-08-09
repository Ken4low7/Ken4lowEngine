#include "SmoothingEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <PostEffectManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
	void SmoothingEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;

		// ルートシグネチャの生成（コンピュート用）
		computeRootSignature_ = builder->CreateComputeRootSignature();

		// パイプラインの生成（コンピュート用）
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::SmoothingCS, computeRootSignature_.Get());

		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(SmoothingSetting));

		// データの設定
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&smoothingSetting_));

		// スムージングの設定
		smoothingSetting_->kernelType = 0; // 0: none, 1: box3x3, ...

		constantBuffer_->SetName(L"SmoothingEffect_ConstantBuffer");
		computeRootSignature_->SetName(L"SmoothingEffect_RootSignature");
		computePipelineState_->SetName(L"SmoothingEffect_PipelineState");
	}

	void SmoothingEffect::Finalize()
	{
		// Mapして保持している生ポインタを無効化（Unmapは安全のため）
		if (constantBuffer_ && smoothingSetting_) {
			constantBuffer_->Unmap(0, nullptr);
			smoothingSetting_ = nullptr;
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
	void SmoothingEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
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

		// スレッドグループの数を計算
		uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

		// Dispatch の実行
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}


	/// -------------------------------------------------------------
	///						　ImGui描画処理
	/// -------------------------------------------------------------
	void SmoothingEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		const char* kernelOptions[] =
		{
			"None",          // 0
			"Box 3x3",       // 1
			"Box 5x5",       // 2
			"Gaussian 5x5",  // 3
			"Box 7x7",       // 4
			"Gaussian 7x7",  // 5
			"Box 9x9",       // 6
			"Gaussian 9x9"   // 7
		};

		// GaussianFilterEffectにもKernel系UIがあるため、PostEffectごとの内部IDを付けて衝突を避ける。
		ImGui::Combo("Kernel Type##SmoothingEffect", &smoothingSetting_->kernelType, kernelOptions, IM_ARRAYSIZE(kernelOptions));
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
