#include "RandomEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <GameTimer.h>
#include <PostEffectManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void RandomEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		computeRootSignature_ = builder->CreateComputeRootSignature();
		computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::RandomCS, computeRootSignature_.Get());
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(RandomSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&randomSetting_));
		randomSetting_->time = 0.0f;
		randomSetting_->useMultiply = false;
		randomSetting_->textureSize = Vector2(
			static_cast<float>(PostEffectManager::GetInstance()->GetGameRenderTargetWidth()),
			static_cast<float>(PostEffectManager::GetInstance()->GetGameRenderTargetHeight()));
		constantBuffer_->SetName(L"RandomEffect_ConstantBuffer");
		computeRootSignature_->SetName(L"RandomEffect_ComputeRootSignature");
		computePipelineState_->SetName(L"RandomEffect_ComputePipelineState");
	}

	void RandomEffect::Finalize()
	{
		if (constantBuffer_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
		randomSetting_ = nullptr;
		constantBuffer_.Reset();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void RandomEffect::Update()
	{
		if (randomSetting_)
		{
			randomSetting_->time += GameTimer::GetInstance()->GetDeltaTime();
		}
	}

	void RandomEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !randomSetting_) return;

		const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
		const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
		randomSetting_->textureSize = Vector2(static_cast<float>(width), static_cast<float>(height));
		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*randomSetting_);
		if (!settingAllocation.IsValid()) return;

		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
		commandList->SetComputeRootConstantBufferView(2, settingAllocation.gpuAddress); // timeとtextureSizeをDispatch直前のFrame専用CBへ固定する。

		constexpr uint32_t threadGroupSizeX = 8;
		constexpr uint32_t threadGroupSizeY = 8;
		const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
		const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;
		commandList->Dispatch(groupCountX, groupCountY, 1);
	}

	void RandomEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (randomSetting_ && ImGui::Button(randomSetting_->useMultiply ? "No Multiply" : "Apply Multiply"))
		{
			randomSetting_->useMultiply = !randomSetting_->useMultiply;
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
