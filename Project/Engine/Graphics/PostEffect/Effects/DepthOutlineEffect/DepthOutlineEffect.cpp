#include "DepthOutlineEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <WinApp.h>
#include "Camera.h"

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　		　コンストラクタ
	/// -------------------------------------------------------------
	DepthOutlineEffect::DepthOutlineEffect(Camera* camera) : camera_(camera)
	{
	}

	/// -------------------------------------------------------------
	///							初期化処理
	/// -------------------------------------------------------------
	void DepthOutlineEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;

		// ルートシグネチャの生成
		rootSignature_ = builder->CreateRootSignature();

		// パイプラインの生成
		graphicsPipelineState_ = builder->CreateGraphicsPipeline(
			PostEffectGraphicsShaderId::DepthOutlinePS,
			rootSignature_.Get(),
			false);

		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DepthOutlineSetting));

		// データの設定
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&depthOutlineSetting_));

		// アウトラインの設定
		depthOutlineSetting_->texelSize = Vector2(1.0f / static_cast<float>(WinApp::kClientWidth), 1.0f / static_cast<float>(WinApp::kClientHeight));
		depthOutlineSetting_->depthScale = 1.0f; // 輪郭の閾値
		depthOutlineSetting_->edgeThickness = 2.0f; // ピクセル単位の太さ
		depthOutlineSetting_->edgeColor = { 0.0f,0.0f,0.0f,1.0f }; // 黒色

		// 毎フレーム（カメラ更新後に）
		Matrix4x4 proj = camera_->GetProjectionMatrix();
		depthOutlineSetting_->projectionInverse = Matrix4x4::Inverse(proj);
	}

	void DepthOutlineEffect::Finalize()
	{
		// Mapして保持している生ポインタを無効化（Unmapは安全のため）
		if (constantBuffer_) {
			constantBuffer_->Unmap(0, nullptr);
		}
		depthOutlineSetting_ = nullptr;

		// D3Dリソース解放
		constantBuffer_.Reset();
		graphicsPipelineState_.Reset();
		rootSignature_.Reset();

		// 借り物参照を切る（所有してない）
		dxCommon_ = nullptr;
		camera_ = nullptr;
	}

	/// -------------------------------------------------------------
	///							適用処理
	/// -------------------------------------------------------------
	void DepthOutlineEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)uavIndex; // 未使用

		depthOutlineSetting_->projectionInverse = Matrix4x4::Inverse(camera_->GetProjectionMatrix());

		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(graphicsPipelineState_.Get());

		// SRVヒープの設定はPostEffectManager側で済ませておく前提
		commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(3, SRVManager::GetInstance()->GetGPUDescriptorHandle(dsvIndex));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}

	/// -------------------------------------------------------------
	///						ImGui描画処理
	/// -------------------------------------------------------------
	void DepthOutlineEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		// PostEffect Settings内の共通ラベル衝突を避けるため、表示名は残して内部IDだけをEffect別にする。
		ImGui::SliderFloat("Depth Scale##DepthOutlineEffect", &depthOutlineSetting_->depthScale, 0.0f, 100.0f);
		ImGui::SliderFloat("Thickness##DepthOutlineEffect", &depthOutlineSetting_->edgeThickness, 1.0f, 10.0f);
		ImGui::ColorEdit4("Edge Color##DepthOutlineEffect", &depthOutlineSetting_->edgeColor.x);
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
