#define NOMINMAX
#include "AnimationModelObjectIdRenderer.h"
#include <DirectXCommon.h>
#include <ObjectIdPipeline.h>

#include <algorithm>

namespace Ken4lowEngine
{

	void DrawAnimationModelObjectId(AnimationModel& animationModel, uint32_t objectId)
	{
		AnimationMesh* animationMesh = animationModel.GetAnimationMesh();
		const ModelData& modelData = animationModel.GetModelData();
		if (!animationMesh || modelData.subMeshes.empty() || objectId == 0)
		{
			return;
		}

		ID3D12GraphicsCommandList* commandList =
			DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
		ObjectIdPipeline::GetInstance()->BindStatic(commandList, objectId);
		const_cast<WorldTransform&>(animationModel.GetWorldTransform()).SetPipeline(0);

		const std::size_t submeshCount = std::min(animationMesh->GetSubmeshCount(), modelData.subMeshes.size());
		for (std::size_t index = 0; index < submeshCount; ++index)
		{
			const D3D12_VERTEX_BUFFER_VIEW& vertexBuffer = animationMesh->GetVertexBufferView(index);
			const D3D12_INDEX_BUFFER_VIEW& indexBuffer = animationMesh->GetIndexBufferView(index);
			commandList->IASetVertexBuffers(0, 1, &vertexBuffer);
			commandList->IASetIndexBuffer(&indexBuffer);
			commandList->DrawIndexedInstanced(
				static_cast<UINT>(modelData.subMeshes[index].indices.size()),
				1,
				0,
				0,
				0); // Object-ID PassではMaterialを使わず、AnimationModelの形状と深度だけを描く。
		}
	}

}
