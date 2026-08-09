#include "DissolveEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <TextureManager.h>
#include <PostEffectManager.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
	void DissolveEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;

		// ルートシグネイチャ（コンピュート用）
		computeRootSignature_ = builder->CreateComputeRootSignature();

		// パイプライン生成（コンピュート用）
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::DissolveCS, computeRootSignature_.Get());

		// ディゾルブの設定
		std::string filePath = "Effects/Masks/Noise.dds";
		TextureManager::GetInstance()->LoadTexture(filePath);

		// UAVヒープ側のインデックスを確保
		dissolveMaskSrvIndexOnUAV_ = UAVManager::GetInstance()->Allocate();

		// リソース＆メタデータを取得
		ID3D12Resource* texture = TextureManager::GetInstance()->GetResource(filePath);
		const auto& metaData = TextureManager::GetInstance()->GetMetaData(filePath);

		// UAVを作成（ディゾルブマスク用）
		UAVManager::GetInstance()->CreateSRVForTexture2DOnThisHeap(dissolveMaskSrvIndexOnUAV_, texture, metaData.format, static_cast<UINT>(metaData.mipLevels));

		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DissolveSetting));

		// データの設定
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveSetting_));

		// ディゾルブの設定
		dissolveSetting_->threshold = 0.5f;
		dissolveSetting_->edgeThickness = 0.05f;
		dissolveSetting_->edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		// 名前の設定
		constantBuffer_->SetName(L"DissolveEffect::ConstantBuffer");
		computePipelineState_->SetName(L"DissolveEffect::ComputePipelineState");
		computeRootSignature_->SetName(L"DissolveEffect::ComputeRootSignature");
	}

	void DissolveEffect::Finalize()
	{
		// UAVヒープ側に確保したSRVインデックスを返却（超重要）
		if (dissolveMaskSrvIndexOnUAV_ != UINT32_MAX) {
			UAVManager::GetInstance()->Free(dissolveMaskSrvIndexOnUAV_);
			dissolveMaskSrvIndexOnUAV_ = UINT32_MAX;
		}

		// Mapしているポインタを無効化（Unmapは安全のため）
		if (constantBuffer_) {
			constantBuffer_->Unmap(0, nullptr);
		}
		dissolveSetting_ = nullptr;

		// D3Dリソース解放
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();

		// 借り物参照を切る
		dxCommon_ = nullptr;
	}


	/// -------------------------------------------------------------
	///						　適用処理
	/// -------------------------------------------------------------
	void DissolveEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
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
		commandList->SetComputeRootDescriptorTable(3, UAVManager::GetInstance()->GetGPUDescriptorHandle(dissolveMaskSrvIndexOnUAV_)); // t1

		// スレッドグループの数を計算して Dispatch
		const uint32_t threadGroupSizeX = 8;
		const uint32_t threadGroupSizeY = 8;

		// Compute Dispatch範囲を現在のGameViewportRenderTargetサイズに合わせる。
		uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();

		uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

		// ディスパッチ処理
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}


	/// -------------------------------------------------------------
	///						　ImGui描画処理
	/// -------------------------------------------------------------
	void DissolveEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		// 他EffectのThreshold/Edge Colorと内部IDが重ならないよう、Effect名サフィックスを付ける。
		ImGui::SliderFloat("Dissolve Threshold##DissolveEffect", &dissolveSetting_->threshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Edge Thickness##DissolveEffect", &dissolveSetting_->edgeThickness, 0.0f, 1.0f);
		ImGui::ColorEdit4("Edge Color##DissolveEffect", &dissolveSetting_->edgeColor.x);
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
