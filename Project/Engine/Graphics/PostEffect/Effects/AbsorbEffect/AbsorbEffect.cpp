#include "AbsorbEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
	void AbsorbEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		// DirectX共通クラスのセット
		dxCommon_ = dxCommon;

		// ルートシグネチャの生成
		rootSignature_ = builder->CreateRootSignature();

		// パイプラインの生成
		graphicsPipelineState_ = builder->CreateGraphicsPipeline(PostEffectGraphicsShaderId::AbsorbPS, rootSignature_.Get(), false);

		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(AbsorbSetting));

		// データの設定
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&absorbSetting_));

		// 吸収の設定
		absorbSetting_->time = 0.0f; // 時間
		absorbSetting_->strength = 1.0f; // 吸収の強さ
	}

	void AbsorbEffect::Finalize()
	{
		// Mapしているポインタを無効化（Unmapは安全のため）
		if (constantBuffer_) {
			constantBuffer_->Unmap(0, nullptr);
		}
		absorbSetting_ = nullptr;

		// D3Dリソース解放
		constantBuffer_.Reset();
		graphicsPipelineState_.Reset();
		rootSignature_.Reset();

		// 借り物参照を切る（所有してない）
		dxCommon_ = nullptr;
	}


	/// -------------------------------------------------------------
	///							更新処理
	/// -------------------------------------------------------------
	void AbsorbEffect::Update()
	{
		// 時間の更新
		absorbSetting_->time += absorbParams_.timestepPerFrame;

		// 時間のリセット
		if (absorbSetting_->time > absorbParams_.loopDuration)
		{
			absorbSetting_->time = 0.0f; // リセット
		}
	}


	/// -------------------------------------------------------------
	///						　適用処理
	/// -------------------------------------------------------------
	void AbsorbEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)uavIndex; // 未使用
		(void)dsvIndex; // 未使用

		// 描画コマンド
		commandList->SetGraphicsRootSignature(rootSignature_.Get()); // ルートシグネチャの設定
		commandList->SetPipelineState(graphicsPipelineState_.Get()); // パイプラインステートの設定
		commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex)); // SRVの設定
		commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());					 // 定数バッファの設定
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);									 // プリミティブトポロジーの設定
		commandList->DrawInstanced(3, 1, 0, 0);																		 // フルスクリーンクアッドを描画
	}


	/// -------------------------------------------------------------
	///						　ImGui描画処理
	/// -------------------------------------------------------------
	void AbsorbEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		// 吸収の強さ
		ImGui::SliderFloat("Absorb Strength", &absorbSetting_->strength, 0.0f, 5.0f);
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
