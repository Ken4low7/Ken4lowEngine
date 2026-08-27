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
	void VignetteEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::VignetteCS, computeRootSignature_.Get());
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VignetteSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteSetting_));
		vignetteSetting_->power = 0.8f;
		vignetteSetting_->range = 0.5f;
		constantBuffer_->SetName(L"VignetteEffect::ConstantBuffer");
		computeRootSignature_->SetName(L"VignetteEffect::ComputeRootSignature");
		computePipelineState_->SetName(L"VignetteEffect::ComputePipelineState");
	}

	void VignetteEffect::Finalize()
	{
		if (constantBuffer_ && vignetteSetting_)
		{
			constantBuffer_->Unmap(0, nullptr);
			vignetteSetting_ = nullptr;
		}
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void VignetteEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !vignetteSetting_) return;

		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*vignetteSetting_);
		if (!settingAllocation.IsValid()) return;
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // runtimeから変化するpower/rangeを現在Frame専用CBへ固定する。

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void VignetteEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!vignetteSetting_) return;
		// ビネット調整項目は見た目への影響が直感的に分かる日本語名で表示する。
		ImGui::SliderFloat("周辺減光の強さ##VignetteEffect", &vignetteSetting_->power, 0.0f, 3.0f);
		ImGui::SliderFloat("周辺減光の範囲##VignetteEffect", &vignetteSetting_->range, 0.0f, 1.0f);
#endif // USE_IMGUI
	}

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
