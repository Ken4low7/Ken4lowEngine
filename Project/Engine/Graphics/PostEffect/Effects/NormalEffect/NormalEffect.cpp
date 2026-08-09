#include "NormalEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <SRVManager.h>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
	void NormalEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;

		// ルートシグネチャの生成
		rootSignature_ = builder->CreateRootSignature();

		// パイプラインの生成
		graphicsPipelineState_ = builder->CreateGraphicsPipeline(PostEffectGraphicsShaderId::NormalPS, rootSignature_.Get(), false);

		// 名前の設定
		rootSignature_->SetName(L"NormalEffect RootSignature");
		graphicsPipelineState_->SetName(L"NormalEffect PSO");
	}

	void NormalEffect::Finalize()
	{
		graphicsPipelineState_.Reset();
		rootSignature_.Reset();

		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///						　適用処理
	/// -------------------------------------------------------------
	void NormalEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)uavIndex; // 未使用
		(void)dsvIndex; // 未使用

		// グラフィックス用のルートシグネチャとPSOを設定
		commandList->SetPipelineState(graphicsPipelineState_.Get());
		commandList->SetGraphicsRootSignature(rootSignature_.Get());

		// SRVヒープの設定はPostEffectManager側で済ませておく前提
		commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));

		// 全画面三角形を描画
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}

} // namespace Ken4lowEngine
