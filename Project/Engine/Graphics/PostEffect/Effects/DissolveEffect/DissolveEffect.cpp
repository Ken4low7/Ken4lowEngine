#include "DissolveEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <TextureManager.h>
#include <PostEffectManager.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void DissolveEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::DissolveCS, computeRootSignature_.Get());
		std::string filePath = "Effects/Masks/Noise.dds";
		TextureManager::GetInstance()->LoadTexture(filePath);
		dissolveMaskSrvIndexOnUAV_ = UAVManager::GetInstance()->Allocate();
		ID3D12Resource* texture = TextureManager::GetInstance()->GetResource(filePath);
		const auto& metaData = TextureManager::GetInstance()->GetMetaData(filePath);
		UAVManager::GetInstance()->CreateSRVForTexture2DOnThisHeap(dissolveMaskSrvIndexOnUAV_, texture, metaData.format, static_cast<UINT>(metaData.mipLevels));
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DissolveSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveSetting_));
		dissolveSetting_->threshold = 0.5f;
		dissolveSetting_->edgeThickness = 0.05f;
		dissolveSetting_->edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		constantBuffer_->SetName(L"DissolveEffect::ConstantBuffer");
		computePipelineState_->SetName(L"DissolveEffect::ComputePipelineState");
		computeRootSignature_->SetName(L"DissolveEffect::ComputeRootSignature");
	}

	void DissolveEffect::Finalize()
	{
		if (dissolveMaskSrvIndexOnUAV_ != UINT32_MAX)
		{
			UAVManager::GetInstance()->Free(dissolveMaskSrvIndexOnUAV_);
			dissolveMaskSrvIndexOnUAV_ = UINT32_MAX;
		}
		if (constantBuffer_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
		dissolveSetting_ = nullptr;
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void DissolveEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !dissolveSetting_) return;

		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*dissolveSetting_);
		if (!settingAllocation.IsValid()) return;
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // threshold等の編集値を現在Frame専用CBへ固定する。
		commandList->SetComputeRootDescriptorTable(3, UAVManager::GetInstance()->GetGPUDescriptorHandle(dissolveMaskSrvIndexOnUAV_));

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void DissolveEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!dissolveSetting_) return;
		// ディゾルブの消失境界を日本語で直感的に調整できるようにする。
		ImGui::SliderFloat("消失しきい値##DissolveEffect", &dissolveSetting_->threshold, 0.0f, 1.0f);
		ImGui::SliderFloat("境界の太さ##DissolveEffect", &dissolveSetting_->edgeThickness, 0.0f, 1.0f);
		ImGui::ColorEdit4("境界色##DissolveEffect", &dissolveSetting_->edgeColor.x);
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
