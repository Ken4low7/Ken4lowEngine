#include "BloomEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <PostEffectShaderManifest.h>
#include <PostEffectManager.h>
#include <ResourceManager.h>
#include <UAVManager.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void BloomEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		assert(dxCommon != nullptr);
		assert(builder != nullptr);

		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::BloomCS, computeRootSignature_.Get());

		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(BloomSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&bloomSetting_));

		// 初期状態で既存見た目を変えないためintensityは0.0にし、ONにしても調整開始点を安全にする。
		*bloomSetting_ = BloomSetting{};

		constantBuffer_->SetName(L"BloomEffect::ConstantBuffer");
		computeRootSignature_->SetName(L"BloomEffect::ComputeRootSignature");
		computePipelineState_->SetName(L"BloomEffect::ComputePipelineState");
	}

	void BloomEffect::Finalize()
	{
		if (constantBuffer_ && bloomSetting_)
		{
			constantBuffer_->Unmap(0, nullptr);
			bloomSetting_ = nullptr;
		}

		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void BloomEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;

		assert(commandList != nullptr);
		assert(computeRootSignature_ != nullptr);
		assert(computePipelineState_ != nullptr);
		assert(constantBuffer_ != nullptr);

		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());

		// BrightExtract/Blur/Compositeを1パスに閉じ、今回は追加RTやBarrier順序の変更を避ける。
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress());

		const uint32_t threadGroupSizeX = 8;
		const uint32_t threadGroupSizeY = 8;
		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void BloomEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!bloomSetting_) { return; }

		// RadialBlurEffectなどと同じ表示ラベルを使うため、##BloomEffectで内部IDだけを分ける。
		ImGui::SliderFloat("Threshold##BloomEffect", &bloomSetting_->threshold, 0.0f, 4.0f);
		ImGui::SliderFloat("Intensity##BloomEffect", &bloomSetting_->intensity, 0.0f, 2.0f);
		ImGui::SliderFloat("Blur Strength##BloomEffect", &bloomSetting_->blurStrength, 0.0f, 4.0f);
		ImGui::TextUnformatted("BrightExtract -> small Blur -> Composite. Multi-pass Bloom is intentionally deferred.");
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
