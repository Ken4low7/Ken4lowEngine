#include "GaussianFilterEffect.h"
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
	void GaussianFilterEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::GaussianFilterCS, computeRootSignature_.Get());
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(GaussianFilterSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&gaussianFilterSetting_));
		gaussianFilterSetting_->kernelType = 1;
		gaussianFilterSetting_->intensity = 1.0f;
		gaussianFilterSetting_->threshold = 0.0f;
		gaussianFilterSetting_->sigma = 1.0f;
		gaussianFilterSetting_->isHorizontal = true;
		constantBuffer_->SetName(L"GaussianFilterEffect ConstantBuffer");
		computePipelineState_->SetName(L"GaussianFilterEffect ComputePipelineState");
		computeRootSignature_->SetName(L"GaussianFilterEffect ComputeRootSignature");
	}

	void GaussianFilterEffect::Finalize()
	{
		if (constantBuffer_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
		gaussianFilterSetting_ = nullptr;
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void GaussianFilterEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !gaussianFilterSetting_) return;

		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*gaussianFilterSetting_);
		if (!settingAllocation.IsValid()) return;
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // Kernel調整値を現在FrameのCompute Dispatchへ固定する。

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void GaussianFilterEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!gaussianFilterSetting_) return;
		const char* kernelOptions[] = { "3x3", "5x5", "7x7", "9x9" };
		// ガウシアンフィルターの各設定値を用途が伝わる日本語で表示する。
		ImGui::Combo("カーネルサイズ##GaussianFilterEffect", &gaussianFilterSetting_->kernelType, kernelOptions, IM_ARRAYSIZE(kernelOptions));
		ImGui::SliderFloat("適用の強さ##GaussianFilterEffect", &gaussianFilterSetting_->intensity, 0.0f, 5.0f);
		ImGui::SliderFloat("ぼかし幅（シグマ）##GaussianFilterEffect", &gaussianFilterSetting_->sigma, 0.1f, 5.0f);
		ImGui::SliderFloat("適用しきい値##GaussianFilterEffect", &gaussianFilterSetting_->threshold, 0.0f, 1.0f);
		ImGui::Checkbox("横方向に適用##GaussianFilterEffect", &gaussianFilterSetting_->isHorizontal);
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
