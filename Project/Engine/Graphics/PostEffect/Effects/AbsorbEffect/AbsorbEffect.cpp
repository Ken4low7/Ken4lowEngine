#include "AbsorbEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	void AbsorbEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		rootSignature_ = builder->CreateRootSignature();
		graphicsPipelineState_ = builder->CreateGraphicsPipeline(PostEffectGraphicsShaderId::AbsorbPS, rootSignature_.Get(), false);
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(AbsorbSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&absorbSetting_));
		absorbSetting_->time = 0.0f;
		absorbSetting_->strength = 1.0f;
	}

	void AbsorbEffect::Finalize()
	{
		if (constantBuffer_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
		absorbSetting_ = nullptr;
		constantBuffer_.Reset();
		graphicsPipelineState_.Reset();
		rootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	void AbsorbEffect::Update()
	{
		if (!absorbSetting_) return;
		absorbSetting_->time += absorbParams_.timestepPerFrame;
		if (absorbSetting_->time > absorbParams_.loopDuration)
		{
			absorbSetting_->time = 0.0f;
		}
	}

	void AbsorbEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)uavIndex;
		(void)dsvIndex;
		if (!commandList || !dxCommon_ || !absorbSetting_) return;

		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*absorbSetting_);
		if (!settingAllocation.IsValid()) return;

		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(graphicsPipelineState_.Get());
		commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetGraphicsRootConstantBufferView(1, settingAllocation.gpuAddress); // 毎フレーム進むtime値をFrame専用CBへ複製し、GPU読取中の上書きを防ぐ。
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}

	void AbsorbEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (absorbSetting_)
		{
			// 吸収演出の適用量を日本語で明確に表示する。
			ImGui::SliderFloat("吸収の強さ", &absorbSetting_->strength, 0.0f, 5.0f);
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
