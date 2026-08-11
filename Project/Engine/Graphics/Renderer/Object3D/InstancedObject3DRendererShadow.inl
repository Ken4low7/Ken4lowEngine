#pragma once

#include "DirectXCommon.h"
#include "LightManager.h"
#include "Model.h"
#include "Object3DCommon.h"
#include "SRVManager.h"

#include <algorithm>
#include "InstancedObject3DRenderer.h"

namespace Ken4lowEngine
{
	inline void InstancedObject3DRenderer::DrawShadow()
	{
		if (!initialized_ || !model_ || sourceInstances_.empty())
		{
			return;
		}

		InstanceStreamBuffer* stream = GetInstanceStream(InstanceStreamUsage::Shadow);
		if (!stream || !stream->mappedInstances || stream->srvIndex == UINT32_MAX)
		{
			return;
		}

		const size_t shadowInstanceCount = (std::min)(sourceInstances_.size(), maxInstanceCount_);
		const uint64_t shadowIndexCount = model_->GetTotalIndexCount() * static_cast<uint64_t>(shadowInstanceCount);
		if (debugIndexBudget_ > 0 && shadowIndexCount > debugIndexBudget_)
		{
			drawSkippedByBudget_ = true;
			return; // Shadow Passでも極端なDraw量によるTDRを防ぐ。
		}

		std::copy_n(sourceInstances_.begin(), shadowInstanceCount, stream->mappedInstances);
		PerViewData shadowPerView{};
		shadowPerView.viewProjection = LightManager::GetInstance()->GetActiveShadowPassLightViewProjection();
		const FrameUploadArena::Allocation perViewAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(shadowPerView);
		if (!perViewAllocation.IsValid())
		{
			return;
		}

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		Object3DCommon::GetInstance()->SetInstancedShadowMapRenderSetting();
		commandList->SetGraphicsRootConstantBufferView(0, perViewAllocation.gpuAddress);
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, stream->srvIndex);

		auto& meshes = model_->GetMeshes();
		for (auto& mesh : meshes)
		{
			mesh.DrawInstanced(static_cast<UINT>(shadowInstanceCount));
		}
	}

	inline void InstancedObject3DRenderer::DrawEditorObjectId(uint32_t baseObjectId)
	{
		const size_t count = UploadSourceInstancesForEditorPicking();
		if (count == 0 || baseObjectId == 0) return;

		InstanceStreamBuffer* stream = GetInstanceStream(InstanceStreamUsage::Picking);
		if (!stream || stream->srvIndex == UINT32_MAX) return;

		PerViewData pickingPerView{};
		pickingPerView.viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const FrameUploadArena::Allocation perViewAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(pickingPerView);
		if (!perViewAllocation.IsValid()) return;

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		ObjectIdPipeline::GetInstance()->BindInstanced(commandList, baseObjectId, true);
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, stream->srvIndex);
		commandList->SetGraphicsRootConstantBufferView(1, perViewAllocation.gpuAddress);
		for (auto& mesh : model_->GetMeshes())
		{
			mesh.DrawInstanced(static_cast<UINT>(count));
		}
	}

	inline void InstancedObject3DRenderer::DrawEditorInstanceObjectId(size_t sourceInstanceIndex, uint32_t objectId)
	{
		const size_t count = UploadSourceInstancesForEditorPicking();
		if (count == 0 || sourceInstanceIndex >= count || objectId == 0) return;

		InstanceStreamBuffer* stream = GetInstanceStream(InstanceStreamUsage::Picking);
		if (!stream || stream->srvIndex == UINT32_MAX) return;

		PerViewData pickingPerView{};
		pickingPerView.viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const FrameUploadArena::Allocation perViewAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(pickingPerView);
		if (!perViewAllocation.IsValid()) return;

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		ObjectIdPipeline::GetInstance()->BindInstanced(commandList, objectId, false);
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, stream->srvIndex);
		commandList->SetGraphicsRootConstantBufferView(1, perViewAllocation.gpuAddress);
		for (auto& mesh : model_->GetMeshes())
		{
			mesh.DrawInstanced(1, static_cast<UINT>(sourceInstanceIndex)); // Picking専用Streamを使い、Main描画のInstance内容を上書きしない。
		}
	}
}
