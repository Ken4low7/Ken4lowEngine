#include "VignetteEffect.h"
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
	void VignetteEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;

		// ルートシグネチャの生成（コンピュート用）
		computeRootSignature_ = builder->CreateComputeRootSignature();

		// パイプラインステートの生成（コンピュート用）
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::VignetteCS, computeRootSignature_.Get());

		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VignetteSetting));

		// データの設定
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteSetting_));

		// ヴィグネットの設定
		vignetteSetting_->power = 0.8f; // 強さ
		vignetteSetting_->range = 0.5f; // 範囲

		constantBuffer_->SetName(L"VignetteEffect::ConstantBuffer");
		computeRootSignature_->SetName(L"VignetteEffect::ComputeRootSignature");
		computePipelineState_->SetName(L"VignetteEffect::ComputePipelineState");
	}

	void VignetteEffect::Finalize()
	{
		// Mapしてるポインタを無効化（Unmapは安全のため）
		if (constantBuffer_ && vignetteSetting_) {
			constantBuffer_->Unmap(0, nullptr);
			vignetteSetting_ = nullptr;
		}

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
	void VignetteEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex; // 未使用

		// コンピュート用のルートシグネチャとPSOを設定
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());

		// SRVとUAVを設定（ディスクリプタテーブル）
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));  // t0
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

		// ディスパッチの実行
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}


	/// -------------------------------------------------------------
	///						　ImGui描画処理
	/// -------------------------------------------------------------
	void VignetteEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		// 複数PostEffectを同じSettings内で開くため、表示名は保ちつつ##で内部IDだけをEffect別に分ける。
		ImGui::SliderFloat("Vignette Power##VignetteEffect", &vignetteSetting_->power, 0.0f, 3.0f);
		ImGui::SliderFloat("Vignette Range##VignetteEffect", &vignetteSetting_->range, 0.0f, 1.0f);
#endif // USE_IMGUI
	}

	/// -------------------------------------------------------------
	///					 runtime control
	/// -------------------------------------------------------------
	void VignetteEffect::SetPower(float power)
	{
		if (vignetteSetting_) { vignetteSetting_->power = power; }
	}

	void VignetteEffect::SetRange(float range)
	{
		if (vignetteSetting_) { vignetteSetting_->range = range; }
	}

	float VignetteEffect::GetPower() const
	{
		return vignetteSetting_ ? vignetteSetting_->power : 0.0f;
	}

	float VignetteEffect::GetRange() const
	{
		return vignetteSetting_ ? vignetteSetting_->range : 0.0f;
	}

} // namespace Ken4lowEngine
