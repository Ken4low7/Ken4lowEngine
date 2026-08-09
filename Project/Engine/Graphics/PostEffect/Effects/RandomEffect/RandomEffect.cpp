#include "RandomEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <GameTimer.h>
#include <PostEffectManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
	void RandomEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;

		// コンピュート用ルートシグネチャの生成
		computeRootSignature_ = builder->CreateComputeRootSignature();

		// コンピュートパイプラインステートの生成
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::RandomCS, computeRootSignature_.Get());

		// リソースの生成
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(RandomSetting));

		// データの設定
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&randomSetting_));

		// ランダムエフェクトの設定
		randomSetting_->time = 0.0f; // 時間
		randomSetting_->useMultiply = false; // 乗算を使用するかどうか
		// ノイズ計算の基準サイズも現在のGameViewportRenderTargetサイズに合わせる。
		randomSetting_->textureSize = Vector2(
			static_cast<float>(PostEffectManager::GetInstance()->GetGameRenderTargetWidth()),
			static_cast<float>(PostEffectManager::GetInstance()->GetGameRenderTargetHeight()));

		// 名前の設定
		constantBuffer_->SetName(L"RandomEffect_ConstantBuffer");
		computeRootSignature_->SetName(L"RandomEffect_ComputeRootSignature");
		computePipelineState_->SetName(L"RandomEffect_ComputePipelineState");
	}

	void RandomEffect::Finalize()
	{
		// Mapしているポインタを無効化（Unmapは安全のため）
		if (constantBuffer_) {
			constantBuffer_->Unmap(0, nullptr);
		}
		randomSetting_ = nullptr;

		// D3Dリソース解放
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();

		// 借り物参照を切る
		dxCommon_ = nullptr;
	}


	/// -------------------------------------------------------------
	///						　更新処理
	/// -------------------------------------------------------------
	void RandomEffect::Update()
	{
		// 時間の更新
		randomSetting_->time += GameTimer::GetInstance()->GetDeltaTime(); // 適当な値で更新	
	}


	/// -------------------------------------------------------------
	///						　適用処理
	/// -------------------------------------------------------------
	void RandomEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
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

		// いまの画面サイズを使う
		// Compute Dispatch範囲を現在のGameViewportRenderTargetサイズに合わせる。
		uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();

		// （textureSize を残すなら毎回更新しておく）
		randomSetting_->textureSize = Vector2((float)width, (float)height);

		uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

		commandList->Dispatch(groupCountX, groupCountY, 1);
	}


	/// -------------------------------------------------------------
	///						　ImGui描画処理
	/// -------------------------------------------------------------
	void RandomEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::Button(randomSetting_->useMultiply ? "No Multiply" : "Apply Multiply")) {
			randomSetting_->useMultiply = !randomSetting_->useMultiply;
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
