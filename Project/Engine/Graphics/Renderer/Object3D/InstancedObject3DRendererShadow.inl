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
		if (!initialized_ || !model_ || sourceInstances_.empty() || !mappedInstances_ || !perViewData_)
		{
			return;
		}

		const size_t shadowInstanceCount = sourceInstances_.size();
		const uint64_t shadowIndexCount = model_->GetTotalIndexCount() * static_cast<uint64_t>(shadowInstanceCount);
		if (debugIndexBudget_ > 0 && shadowIndexCount > debugIndexBudget_)
		{
			drawSkippedByBudget_ = true;
			return; // Shadow Passでも極端なDraw量によるTDRを防ぐ。
		}

		std::copy(sourceInstances_.begin(), sourceInstances_.end(), mappedInstances_);
		instanceBufferDirty_ = true; // 通常描画ではカメラFrustumに合わせて再度可視Instanceを詰め直す。
		perViewData_->viewProjection = LightManager::GetInstance()->GetActiveShadowPassLightViewProjection();

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		Object3DCommon::GetInstance()->SetInstancedShadowMapRenderSetting();
		commandList->SetGraphicsRootConstantBufferView(0, perViewResource_->GetGPUVirtualAddress());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, instanceSrvIndex_);

		auto& meshes = model_->GetMeshes();
		for (auto& mesh : meshes)
		{
			mesh.DrawInstanced(static_cast<UINT>(shadowInstanceCount));
		}
	}

	inline void Ken4lowEngine::InstancedObject3DRenderer::DrawEditorObjectId(uint32_t baseObjectId)
	{
		const size_t count = UploadSourceInstancesForEditorPicking();
		if (count == 0 || baseObjectId == 0) return;

		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		perViewData_->viewProjection = viewProjection;
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		ObjectIdPipeline::GetInstance()->BindInstanced(commandList, baseObjectId, true);
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, instanceSrvIndex_);
		commandList->SetGraphicsRootConstantBufferView(1, perViewResource_->GetGPUVirtualAddress());
		for (auto& mesh : model_->GetMeshes())
		{
			mesh.DrawInstanced(static_cast<UINT>(count));
		}
	}

	inline void InstancedObject3DRenderer::DrawEditorInstanceObjectId(size_t sourceInstanceIndex, uint32_t objectId)
	{
		const size_t count = UploadSourceInstancesForEditorPicking();
		if (count == 0 || sourceInstanceIndex >= count || objectId == 0) return;

		perViewData_->viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		ObjectIdPipeline::GetInstance()->BindInstanced(commandList, objectId, false);
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(0, instanceSrvIndex_);
		commandList->SetGraphicsRootConstantBufferView(1, perViewResource_->GetGPUVirtualAddress());
		for (auto& mesh : model_->GetMeshes())
		{
			mesh.DrawInstanced(1, static_cast<UINT>(sourceInstanceIndex)); // StructuredBufferの元Indexを維持して1体だけ描く。
		}
	}

}
