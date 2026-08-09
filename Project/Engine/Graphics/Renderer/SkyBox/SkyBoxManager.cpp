#include "SkyBoxManager.h"
#include "DirectXCommon.h"
#include "SRVManager.h"

namespace Ken4lowEngine
{
	SkyBoxManager* SkyBoxManager::GetInstance()
	{
		static SkyBoxManager instance;
		return &instance;
	}

	void SkyBoxManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;

		pipelineFactory_.Initialize(dxCommon_->GetDevice());

		pipelineSet_.Initialize(pipelineFactory_, dxCommon_->GetDXCCompilerManager(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);
	}

	void SkyBoxManager::Finalize()
	{
		pipelineSet_.Finalize();
		pipelineFactory_.Finalize();
		dxCommon_ = nullptr;
	}

	namespace
	{
		void SetPipeline(ID3D12GraphicsCommandList* commandList, const PipelineBundle& pipeline)
		{
			commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
			commandList->SetPipelineState(pipeline.pipelineState.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			SRVManager::GetInstance()->PreDraw();
			SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(2, 0);
			// PNG 由来の雲 DDS は 2D SRV のため、CubeMap と分離したテーブルへ同じヒープ先頭を公開する。
			SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(3, 0);
		}
	}

	void SkyBoxManager::SetRenderSetting()
	{
		SetPipeline(dxCommon_->GetCommandManager()->GetCommandList(), pipelineSet_.GetDefault());
	}

	void SkyBoxManager::SetCloudRenderSetting()
	{
		SetPipeline(dxCommon_->GetCommandManager()->GetCommandList(), pipelineSet_.GetCloud());
	}
}