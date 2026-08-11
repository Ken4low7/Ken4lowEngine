#include "PixelateEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <WinApp.h>
#include <PostEffectManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void PixelateEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::PixelateCS, computeRootSignature_.Get());
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(PixelateSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&pixelateSetting_));
		pixelateSetting_->screenSize = { static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) };
		pixelateSetting_->blockSize = 8.0f;
		pixelateSetting_->strength = 1.0f;
		constantBuffer_->SetName(L"PixelateEffect ConstantBuffer");
		computePipelineState_->SetName(L"PixelateEffect PipelineState");
		computeRootSignature_->SetName(L"PixelateEffect RootSignature");
	}

	void PixelateEffect::Finalize()
	{
		if (constantBuffer_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
		pixelateSetting_ = nullptr;
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void PixelateEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !pixelateSetting_) return;

		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		pixelateSetting_->screenSize = { static_cast<float>(width), static_cast<float>(height) };
		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*pixelateSetting_);
		if (!settingAllocation.IsValid()) return;

		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // resize後のscreenSizeも現在Frameの値として固定する。

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void PixelateEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!pixelateSetting_) return;
		ImGui::SliderFloat("Block Size##PixelateEffect", &pixelateSetting_->blockSize, 1.0f, 128.0f);
		ImGui::SliderFloat("Strength##PixelateEffect", &pixelateSetting_->strength, 0.0f, 1.0f);
#endif
	}

} // namespace Ken4lowEngine
