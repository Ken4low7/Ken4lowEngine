#include "GrayScaleEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <PostEffectShaderManifest.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <PostEffectManager.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void GrayScaleEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		assert(dxCommon != nullptr);
		assert(builder != nullptr);
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		assert(computeRootSignature_ != nullptr);
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::GrayScaleCS, computeRootSignature_.Get());
		assert(computePipelineState_ != nullptr);
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(GrayScaleSetting));
		assert(constantBuffer_ != nullptr);
		const HRESULT hr = constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&grayScaleSetting_));
		assert(SUCCEEDED(hr));
		assert(grayScaleSetting_ != nullptr);
		grayScaleSetting_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		constantBuffer_->SetName(L"GrayScaleEffect ConstantBuffer");
		computePipelineState_->SetName(L"GrayScaleEffect PipelineState");
		computeRootSignature_->SetName(L"GrayScaleEffect RootSignature");
	}

	void GrayScaleEffect::Finalize()
	{
		if (constantBuffer_ && grayScaleSetting_)
		{
			constantBuffer_->Unmap(0, nullptr);
			grayScaleSetting_ = nullptr;
		}
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void GrayScaleEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		assert(commandList != nullptr);
		assert(dxCommon_ != nullptr);
		assert(computeRootSignature_ != nullptr);
		assert(computePipelineState_ != nullptr);
		assert(grayScaleSetting_ != nullptr);

		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*grayScaleSetting_);
		if (!settingAllocation.IsValid()) return;
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // Color編集値を現在Frame専用CBへ複製して前フレーム参照と分離する。

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void GrayScaleEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!grayScaleSetting_) return;
		ImGui::ColorEdit4("GrayScale Color", &grayScaleSetting_->color.x);
		ImGui::Text("GrayScale Effect");
		ImGui::Separator();
		ImGui::Text("Intensity: %f", grayScaleSetting_->color.x);
		ImGui::Separator();
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
