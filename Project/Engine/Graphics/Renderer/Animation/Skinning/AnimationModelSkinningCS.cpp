#include "AnimationModelSkinningCS.h"

#include <DirectXCommon.h>
#include <ResourceManager.h>

namespace Ken4lowEngine
{
	void AnimationModelSkinningCS::Initialize(DirectXCommon* dxCommon, uint32_t initialVertexCount, bool isSkinning)
	{
		Reset();
		if (!dxCommon) { return; }

		csCB_ = ResourceManager::CreateBufferResource(dxCommon->GetDevice(), sizeof(SkinningInformationForGPU));
		csCB_->Map(0, nullptr, reinterpret_cast<void**>(&mapped_));

		mapped_->numVertices = initialVertexCount;
		mapped_->isSkinning = isSkinning;
		runtimeEnabled_ = true;
	}

	void AnimationModelSkinningCS::Reset()
	{
		mapped_ = nullptr;
		csCB_.Reset();
		runtimeEnabled_ = true;
	}

	void AnimationModelSkinningCS::Dispatch(DirectXCommon* dxCommon, SkinCluster* skinCluster, D3D12_GPU_DESCRIPTOR_HANDLE inputVerticesSrv, D3D12_GPU_DESCRIPTOR_HANDLE influenceSrv, D3D12_GPU_DESCRIPTOR_HANDLE outputUav, uint32_t vertexCount, ID3D12Resource* skinnedVB, D3D12_RESOURCE_STATES& skinnedState, D3D12_GPU_DESCRIPTOR_HANDLE externalPaletteSrv)
	{
		if (!dxCommon || !mapped_ || !mapped_->isSkinning) { return; }
		if (!runtimeEnabled_) { return; }
		if (!skinCluster || !skinnedVB || vertexCount == 0) { return; }

		auto* cl = dxCommon->GetCommandManager()->GetCommandList();

		// 毎回頂点数更新
		mapped_->numVertices = vertexCount;

		// VB → UAV
		if (skinnedState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			dxCommon->ResourceTransition(skinnedVB, skinnedState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			skinnedState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		// ルート：t0/t1/t2/u0/b0
		// DebugSceneの共有姿勢検証ではt0だけ代表Paletteへ差し替え、その他の入出力は個体ごとに維持する。
		const D3D12_GPU_DESCRIPTOR_HANDLE paletteSrv = externalPaletteSrv.ptr != 0
			? externalPaletteSrv
			: skinCluster->GetPaletteSrvOnUAVHeap();
		cl->SetComputeRootDescriptorTable(0, paletteSrv);
		cl->SetComputeRootDescriptorTable(1, inputVerticesSrv);
		cl->SetComputeRootDescriptorTable(2, influenceSrv);
		cl->SetComputeRootDescriptorTable(3, outputUav);
		cl->SetComputeRootConstantBufferView(4, csCB_->GetGPUVirtualAddress());

		// Dispatch（HLSLの[numthreads]と合わせる）
		constexpr uint32_t GROUP = 256;
		cl->Dispatch((vertexCount + GROUP - 1) / GROUP, 1, 1);

		// UAV → VB
		if (skinnedState != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		{
			dxCommon->ResourceTransition(skinnedVB, skinnedState, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			skinnedState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		}
	}
}
