#pragma once

#include "AnimationPipelineBuilder.h"
#include "AnimationSampler.h"
#include "DirectXCommon.h"
#include "LightManager.h"
#include "Object3DCommon.h"
#include "ResourceManager.h"
#include "UAVManager.h"

namespace Ken4lowEngine
{
	inline void AnimationModel::EnsureShadowTransformResource()
	{
		if (shadowTransformResource_ && shadowTransformData_)
		{
			return;
		}

		shadowTransformResource_ = ResourceManager::CreateBufferResource(
			dxCommon_->GetDevice(), sizeof(TransformationAnimationMatrix));
		shadowTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadowTransformData_));
		shadowTransformData_->WVP = Matrix4x4::MakeIdentity();
		shadowTransformData_->World = Matrix4x4::MakeIdentity();
		shadowTransformData_->WorldInversedTranspose = Matrix4x4::MakeIdentity();
	}

	inline Matrix4x4 AnimationModel::BuildCurrentShadowWorldMatrix() const
	{
		const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
			worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

		if (skeleton_ && !skeleton_->GetJoints().empty() && skinningCS_.IsSkinningModel())
		{
			return worldMatrix; // Skeletal Meshは頂点側へPoseを適用済みなのでActorのWorld行列だけを使う。
		}

		Vector3 translate{ 0.0f, 0.0f, 0.0f };
		Quaternion rotate{};
		Vector3 scale{ 1.0f, 1.0f, 1.0f };
		if (const Animation* currentAnimation = GetCurrentAnimation())
		{
			const auto rootIt = currentAnimation->nodeAnimations.find(modelData.rootNode.name);
			if (rootIt != currentAnimation->nodeAnimations.end())
			{
				const NodeAnimation& rootAnimation = rootIt->second;
				if (!rootAnimation.translate.empty())
				{
					translate = AnimationSampler::CalculateValue(rootAnimation.translate, animationPlayer_.GetTime());
				}
				if (!rootAnimation.rotate.empty())
				{
					rotate = AnimationSampler::CalculateValue(rootAnimation.rotate, animationPlayer_.GetTime());
				}
				if (!rootAnimation.scale.empty())
				{
					scale = AnimationSampler::CalculateValue(rootAnimation.scale, animationPlayer_.GetTime());
				}
			}
		}

		const Matrix4x4 localAnimationMatrix = Matrix4x4::MakeAffineMatrix(scale, rotate, translate);
		return Matrix4x4::Multiply(localAnimationMatrix, worldMatrix);
	}

	inline bool AnimationModel::IsShadowSkinningCacheCurrent() const
	{
		const int lodIndex = lodController_.GetLODIndex();
		if (!shadowSkinningPrepared_ || lodIndex < 0 || lodIndex >= static_cast<int>(lods_.size()))
		{
			return false;
		}

		return shadowPreparedAnimationTime_ == animationPlayer_.GetTime()
			&& shadowPreparedCrossFadeTime_ == crossFadeTime_
			&& shadowPreparedAnimationIndex_ == currentAnimationIndex_
			&& shadowPreparedPreviousAnimationIndex_ == previousAnimationIndex_
			&& shadowPreparedLodIndex_ == lodIndex
			&& shadowPreparedSkinnedBuffer_ == lods_[lodIndex].skinnedVB.Get();
	}

	inline void AnimationModel::UpdateShadowSkinningCacheState()
	{
		const int lodIndex = lodController_.GetLODIndex();
		shadowSkinningPrepared_ = true;
		shadowPreparedAnimationTime_ = animationPlayer_.GetTime();
		shadowPreparedCrossFadeTime_ = crossFadeTime_;
		shadowPreparedAnimationIndex_ = currentAnimationIndex_;
		shadowPreparedPreviousAnimationIndex_ = previousAnimationIndex_;
		shadowPreparedLodIndex_ = lodIndex;
		shadowPreparedSkinnedBuffer_ =
			(0 <= lodIndex && lodIndex < static_cast<int>(lods_.size())) ? lods_[lodIndex].skinnedVB.Get() : nullptr;
	}

	inline void AnimationModel::DrawShadow()
	{
		if (!dxCommon_ || lods_.empty())
		{
			return;
		}

		const int lodIndex = lodController_.GetLODIndex();
		if (lodIndex < 0 || lodIndex >= static_cast<int>(lods_.size()))
		{
			return;
		}

		auto& lod = lods_[lodIndex];
		if (skinningCS_.IsSkinningModel() && useComputeSkinning_ && !IsShadowSkinningCacheCurrent())
		{
			if (lodIndex >= static_cast<int>(skinClusterLOD_.size()) || !skinClusterLOD_[lodIndex])
			{
				return;
			}

			UAVManager::GetInstance()->PreDispatch();
			AnimationPipelineBuilder::GetInstance()->SetComputeSetting();
			const D3D12_GPU_DESCRIPTOR_HANDLE inputSrv =
				UAVManager::GetInstance()->GetGPUDescriptorHandle(lod.srvInputVerticesOnUavHeap);
			const D3D12_GPU_DESCRIPTOR_HANDLE outputUav =
				UAVManager::GetInstance()->GetGPUDescriptorHandle(lod.uavIndex);
			skinningCS_.Dispatch(
				dxCommon_, skinClusterLOD_[lodIndex].get(), inputSrv, lod.influenceSrvGpuOnUavHeap,
				outputUav, lod.vertexCount, lod.skinnedVB.Get(), lod.skinnedState);
			UpdateShadowSkinningCacheState(); // Point 6面とCSM 4層では同じPoseのCompute Skinning結果を再利用する。
		}

		EnsureShadowTransformResource();
		const Matrix4x4 shadowWorld = BuildCurrentShadowWorldMatrix();
		shadowTransformData_->World = shadowWorld;
		shadowTransformData_->WVP = Matrix4x4::Multiply(
			shadowWorld, LightManager::GetInstance()->GetActiveShadowPassLightViewProjection());
		shadowTransformData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(shadowWorld));

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		Object3DCommon::GetInstance()->SetShadowMapRenderSetting();
		commandList->SetGraphicsRootConstantBufferView(0, shadowTransformResource_->GetGPUVirtualAddress());

		if (skinningCS_.IsSkinningModel())
		{
			commandList->IASetVertexBuffers(0, 1, &lod.skinnedVBV);
			commandList->IASetIndexBuffer(&lod.ibv);
			for (const auto& range : lod.subMeshRanges)
			{
				commandList->DrawIndexedInstanced(range.indexCount, 1, range.startIndex, 0, 0);
			}
			return;
		}

		const size_t subMeshCount = animationMesh_ ? animationMesh_->GetSubmeshCount() : 0;
		for (size_t index = 0; index < subMeshCount && index < modelData.subMeshes.size(); ++index)
		{
			const auto& vbv = animationMesh_->GetVertexBufferView(index);
			const auto& ibv = animationMesh_->GetIndexBufferView(index);
			commandList->IASetVertexBuffers(0, 1, &vbv);
			commandList->IASetIndexBuffer(&ibv);
			commandList->DrawIndexedInstanced(static_cast<UINT>(modelData.subMeshes[index].indices.size()), 1, 0, 0, 0);
		}
	}
}
