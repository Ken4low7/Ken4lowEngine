#include "SpriteManager.h"
#include "DirectXCommon.h"
#include "SRVManager.h"

namespace Ken4lowEngine
{
	SpriteManager* SpriteManager::GetInstance()
	{
		static SpriteManager instance;
		return &instance;
	}

	void SpriteManager::Initialize(DirectXCommon* dxCommon)
	{
		// 描画設定時に使用する DirectXCommon を保持する
		dxCommon_ = dxCommon;

		// Sprite 用の Background / UI パイプラインをまとめて生成する
		pipelineSet_.Initialize(dxCommon_->GetPipelineFactory(), dxCommon_->GetDXCCompilerManager(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);
	}

	void SpriteManager::Finalize()
	{
		// 先に Sprite 用パイプライン群を解放する
		pipelineSet_.Finalize();

		// 最後に DirectXCommon 参照を切る
		dxCommon_ = nullptr;
	}

	void SpriteManager::SetRenderSetting_Background()
	{
		// 背景用パイプラインをコマンドリストへ設定する
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const auto& pipeline = pipelineSet_.GetBackground();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Sprite 側で使う SRV テーブルを root parameter[3] に設定する
		SRVManager::GetInstance()->PreDraw();
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(3, 0);
	}

	void SpriteManager::SetRenderSetting_UI()
	{
		// UI 用パイプラインをコマンドリストへ設定する
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const auto& pipeline = pipelineSet_.GetUI();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Sprite 側で使う SRV テーブルを root parameter[3] に設定する
		SRVManager::GetInstance()->PreDraw();
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(3, 0);
	}
}