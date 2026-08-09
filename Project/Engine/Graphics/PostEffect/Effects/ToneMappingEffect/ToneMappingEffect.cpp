#include "ToneMappingEffect.h"
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
	void ToneMappingEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		assert(dxCommon != nullptr);
		assert(builder != nullptr);

		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::ToneMappingCS, computeRootSignature_.Get());

		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ToneMappingSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&toneMappingSetting_));

		// 既存シーンの見た目を初期状態で大きく変えないため、None + exposure 1.0 + gamma 2.2を既定にする。
		*toneMappingSetting_ = ToneMappingSetting{};

		constantBuffer_->SetName(L"ToneMappingEffect::ConstantBuffer");
		computeRootSignature_->SetName(L"ToneMappingEffect::ComputeRootSignature");
		computePipelineState_->SetName(L"ToneMappingEffect::ComputePipelineState");
	}

	void ToneMappingEffect::Finalize()
	{
		if (constantBuffer_ && toneMappingSetting_)
		{
			constantBuffer_->Unmap(0, nullptr);
			toneMappingSetting_ = nullptr;
		}

		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void ToneMappingEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;

		assert(commandList != nullptr);
		assert(computeRootSignature_ != nullptr);
		assert(computePipelineState_ != nullptr);
		assert(constantBuffer_ != nullptr);

		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());

		// 既存Compute PostEffectと同じt0/u0/b0順にして、ResourceBarrierとHeap切替の責務をManager側へ残す。
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

	void ToneMappingEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!toneMappingSetting_) { return; }

		const char* items[] = { "None", "Reinhard" };
		int toneMappingType = static_cast<int>(toneMappingSetting_->toneMappingType);
		// PostEffect Settingsは複数EffectのUIを同時に並べるため、##で内部IDを分けてDear ImGuiのID衝突を防ぐ。
		ImGui::SliderFloat("Exposure##ToneMappingEffect", &toneMappingSetting_->exposure, 0.1f, 4.0f);
		ImGui::SliderFloat("Gamma##ToneMappingEffect", &toneMappingSetting_->gamma, 1.0f, 3.0f);
		if (ImGui::Combo("Tone Mapping Type##ToneMappingEffect", &toneMappingType, items, 2))
		{
			toneMappingSetting_->toneMappingType = static_cast<uint32_t>(toneMappingType);
		}
		ImGui::TextUnformatted("HDR/LDR boundary: keep off by default, then place before Bloom/PBR output checks.");
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
