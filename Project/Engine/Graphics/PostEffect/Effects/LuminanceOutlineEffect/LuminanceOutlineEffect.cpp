#include "LuminanceOutlineEffect.h"
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
	void LuminanceOutlineEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::LuminanceOutlineCS, computeRootSignature_.Get());
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LuminanceOutlineSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&luminanceOutlineSetting_));
		luminanceOutlineSetting_->color = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
		luminanceOutlineSetting_->texelSize = Vector2(1.0f / static_cast<float>(WinApp::kClientWidth), 1.0f / static_cast<float>(WinApp::kClientHeight));
		luminanceOutlineSetting_->edgeStrength = 1.0f;
		luminanceOutlineSetting_->threshold = 0.5f;
		constantBuffer_->SetName(L"LuminanceOutlineEffect ConstantBuffer");
		computePipelineState_->SetName(L"LuminanceOutlineEffect ComputePipelineState");
		computeRootSignature_->SetName(L"LuminanceOutlineEffect ComputeRootSignature");
	}

	void LuminanceOutlineEffect::Finalize()
	{
		if (constantBuffer_ && luminanceOutlineSetting_)
		{
			constantBuffer_->Unmap(0, nullptr);
			luminanceOutlineSetting_ = nullptr;
		}
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void LuminanceOutlineEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !luminanceOutlineSetting_) return;

		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		if (width > 0 && height > 0)
		{
			luminanceOutlineSetting_->texelSize = Vector2(1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
		}
		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*luminanceOutlineSetting_);
		if (!settingAllocation.IsValid()) return;

		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // texelSizeとOutline設定を現在FrameのDispatchへ固定する。

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void LuminanceOutlineEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!luminanceOutlineSetting_) return;
		ImGui::Text("Luminance Outline Effect Settings");
		ImGui::ColorEdit4("Outline Color##LuminanceOutlineEffect", &luminanceOutlineSetting_->color.x);
		ImGui::SliderFloat("Edge Strength##LuminanceOutlineEffect", &luminanceOutlineSetting_->edgeStrength, 0.0f, 5.0f);
		ImGui::SliderFloat("Threshold##LuminanceOutlineEffect", &luminanceOutlineSetting_->threshold, 0.0f, 1.0f);
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
