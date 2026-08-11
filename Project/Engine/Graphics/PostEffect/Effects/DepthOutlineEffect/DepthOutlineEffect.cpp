#include "DepthOutlineEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <WinApp.h>
#include "Camera.h"

#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	DepthOutlineEffect::DepthOutlineEffect(Camera* camera) : camera_(camera)
	{
	}

	void DepthOutlineEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
	{
		dxCommon_ = dxCommon;
		rootSignature_ = builder->CreateRootSignature();
		graphicsPipelineState_ = builder->CreateGraphicsPipeline(PostEffectGraphicsShaderId::DepthOutlinePS, rootSignature_.Get(), false);
		constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DepthOutlineSetting));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&depthOutlineSetting_));
		depthOutlineSetting_->texelSize = Vector2(1.0f / static_cast<float>(WinApp::kClientWidth), 1.0f / static_cast<float>(WinApp::kClientHeight));
		depthOutlineSetting_->depthScale = 1.0f;
		depthOutlineSetting_->edgeThickness = 2.0f;
		depthOutlineSetting_->edgeColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		if (camera_)
		{
			depthOutlineSetting_->projectionInverse = Matrix4x4::Inverse(camera_->GetProjectionMatrix());
		}
	}

	void DepthOutlineEffect::Finalize()
	{
		if (constantBuffer_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
		depthOutlineSetting_ = nullptr;
		constantBuffer_.Reset();
		graphicsPipelineState_.Reset();
		rootSignature_.Reset();
		dxCommon_ = nullptr;
		camera_ = nullptr;
	}

	void DepthOutlineEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
	{
		(void)uavIndex;
		if (!commandList || !dxCommon_ || !depthOutlineSetting_ || !camera_) return;

		depthOutlineSetting_->projectionInverse = Matrix4x4::Inverse(camera_->GetProjectionMatrix());
		const FrameUploadArena::Allocation settingAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(*depthOutlineSetting_);
		if (!settingAllocation.IsValid()) return;

		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(graphicsPipelineState_.Get());
		commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
		commandList->SetGraphicsRootConstantBufferView(1, settingAllocation.gpuAddress); // カメラProjection更新をDraw単位のFrame専用CBへ固定する。
		commandList->SetGraphicsRootDescriptorTable(3, SRVManager::GetInstance()->GetGPUDescriptorHandle(dsvIndex));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}

	void DepthOutlineEffect::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!depthOutlineSetting_) return;
		ImGui::SliderFloat("Depth Scale##DepthOutlineEffect", &depthOutlineSetting_->depthScale, 0.0f, 100.0f);
		ImGui::SliderFloat("Thickness##DepthOutlineEffect", &depthOutlineSetting_->edgeThickness, 1.0f, 10.0f);
		ImGui::ColorEdit4("Edge Color##DepthOutlineEffect", &depthOutlineSetting_->edgeColor.x);
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
