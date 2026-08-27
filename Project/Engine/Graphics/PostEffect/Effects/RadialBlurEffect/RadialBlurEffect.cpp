#include "RadialBlurEffect.h"
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
	void RadialBlurEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::RadialBlurCS, computeRootSignature_.Get());
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(RadialBlurSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&radialBlurSetting_));
		radialBlurSetting_->center = Vector2(0.5f, 0.5f);
		radialBlurSetting_->blurStrength = 0.3f;
		radialBlurSetting_->sampleCount = 16.0f;
		constantBuffer_->SetName(L"RadialBlurEffect::ConstantBuffer");
		computePipelineState_->SetName(L"RadialBlurEffect::ComputePipelineState");
		computeRootSignature_->SetName(L"RadialBlurEffect::ComputeRootSignature");
	}

	void RadialBlurEffect::Finalize()
	{
		if (constantBuffer_ && radialBlurSetting_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
		radialBlurSetting_ = nullptr;
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void RadialBlurEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !radialBlurSetting_) return;

		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*radialBlurSetting_);
		if (!settingAllocation.IsValid()) return;
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // runtime制御されるcenter/strengthをDispatch時点のFrame専用値へ固定する。

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void RadialBlurEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!radialBlurSetting_) return;
		// 放射状ぼかしの調整項目を効果の意味が分かる日本語名で表示する。
		ImGui::SliderFloat("ぼかしの強さ##RadialBlurEffect", &radialBlurSetting_->blurStrength, 0.0f, 5.0f);
		ImGui::SliderFloat("サンプル数##RadialBlurEffect", &radialBlurSetting_->sampleCount, 1.0f, 64.0f);
		ImGui::SliderFloat2("ぼかし中心##RadialBlurEffect", &radialBlurSetting_->center.x, 0.0f, 1.0f);
#endif // USE_IMGUI
	}

	void RadialBlurEffect::SetCenter(const Vector2& center)
	{
		if (radialBlurSetting_) { radialBlurSetting_->center = center; }
	}

	void RadialBlurEffect::SetBlurStrength(float strength)
	{
		if (radialBlurSetting_) { radialBlurSetting_->blurStrength = strength; }
	}

	void RadialBlurEffect::SetSampleCount(float samples)
	{
		if (radialBlurSetting_) { radialBlurSetting_->sampleCount = samples; }
	}

	Vector2 RadialBlurEffect::GetCenter() const
	{
		return radialBlurSetting_ ? radialBlurSetting_->center : Vector2{ 0.5f, 0.5f };
	}

	float RadialBlurEffect::GetBlurStrength() const
	{
		return radialBlurSetting_ ? radialBlurSetting_->blurStrength : 0.0f;
	}

	float RadialBlurEffect::GetSampleCount() const
	{
		return radialBlurSetting_ ? radialBlurSetting_->sampleCount : 0.0f;
	}

} // namespace Ken4lowEngine
