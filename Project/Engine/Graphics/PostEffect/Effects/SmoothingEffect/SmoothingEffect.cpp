#include "SmoothingEffect.h"
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
	void SmoothingEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::SmoothingCS, computeRootSignature_.Get());
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(SmoothingSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&smoothingSetting_));
		smoothingSetting_->kernelType = 0;
		constantBuffer_->SetName(L"SmoothingEffect_ConstantBuffer");
		computeRootSignature_->SetName(L"SmoothingEffect_RootSignature");
		computePipelineState_->SetName(L"SmoothingEffect_PipelineState");
	}

	void SmoothingEffect::Finalize()
	{
		if (constantBuffer_ && smoothingSetting_)
		{
			constantBuffer_->Unmap(0, nullptr);
			smoothingSetting_ = nullptr;
		}
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void SmoothingEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !smoothingSetting_) return;

		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*smoothingSetting_);
		if (!settingAllocation.IsValid()) return;
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // kernelTypeを現在FrameのDispatchへ固定してUI編集との競合を防ぐ。

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void SmoothingEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!smoothingSetting_) return;
		const char* kernelOptions[] =
		{
			"なし",
			"ボックス 3x3",
			"ボックス 5x5",
			"ガウシアン 5x5",
			"ボックス 7x7",
			"ガウシアン 7x7",
			"ボックス 9x9",
			"ガウシアン 9x9"
		};
		// 平滑化方式を日本語で選択できるようにし、カーネルサイズもそのまま確認できるようにする。
		ImGui::Combo("平滑化方式##SmoothingEffect", &smoothingSetting_->kernelType, kernelOptions, IM_ARRAYSIZE(kernelOptions));
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
