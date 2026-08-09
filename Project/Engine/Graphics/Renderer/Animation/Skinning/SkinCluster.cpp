#include "SkinCluster.h"
#include "DirectXCommon.h"
#include "Skeleton.h"
#include "ResourceManager.h"
#include "SRVManager.h"
#include "UAVManager.h"

#include <cassert>
#include <cstring>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　　　 総頂点数を数える
/// -------------------------------------------------------------
static uint32_t CountTotalVertices(const ModelData& modelData)
{
	uint32_t count = 0;
	for (const auto& sm : modelData.subMeshes) {
		count += static_cast<uint32_t>(sm.vertices.size());
	}
	return count;
}

/// -------------------------------------------------------------
///				　　　		デストラクタ
/// -------------------------------------------------------------
SkinCluster::~SkinCluster()
{
	// リソース解放
	if (paletteSrvIndex_ != UINT32_MAX) {
		SRVManager::GetInstance()->Free(paletteSrvIndex_);
	}
	// UAVヒープ上の SRV 解放
	if (paletteSrvIndexOnUavHeap_ != UINT32_MAX) {
		UAVManager::GetInstance()->Free(paletteSrvIndexOnUavHeap_);
	}
	// UAVヒープ上の SRV 解放
	if (influenceSrvIndexOnUavHeap_ != UINT32_MAX) {
		UAVManager::GetInstance()->Free(influenceSrvIndexOnUavHeap_);
	}
}

/// -------------------------------------------------------------
///				　　　		初期化処理
/// -------------------------------------------------------------
void SkinCluster::Initialize(const ModelData& modelData, Skeleton& skeleton)
{
	auto* dxCommon = DirectXCommon::GetInstance();
	auto* device = dxCommon->GetDevice();
	auto* commandList = dxCommon->GetCommandManager()->GetCommandList();
	auto& joints = skeleton.GetJoints();

	// 総頂点数を出す
	auto coutTotalVertices = [&]() {
		uint32_t count = 0;
		for (const auto& sm : modelData.subMeshes) {
			count += static_cast<uint32_t>(sm.vertices.size());
		}
		return count;
		};

	const uint32_t totalVerts = coutTotalVertices();   // subMeshes 合計

	// t0: パレット（UPLOAD & Map）
	paletteResource_ = ResourceManager::CreateBufferResource(device, sizeof(WellForGPU) * joints.size());
	paletteResource_->SetName(L"SkinCluster_PaletteResource_UPLOADED");

	WellForGPU* mappedPalette = nullptr;
	paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	mappedPalette_ = { mappedPalette, joints.size() }; // span

	// ===== DEFAULT（読取用）を作成 → UPLOAD からコピー（初期 COMMON → 明示遷移）=====
	{
		D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC    bufDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(WellForGPU) * joints.size());

		// 初期ステートは COMMON にする（警告回避）
		HRESULT hr = S_FALSE;
		hr = device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&paletteResourceDefault_));
		assert(SUCCEEDED(hr));

		// COMMON → COPY_DEST に明示遷移してからコピー
		dxCommon->ResourceTransition(paletteResourceDefault_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

		// UPLOAD → DEFAULT へ初期コピー
		commandList->CopyBufferRegion(paletteResourceDefault_.Get(), 0, paletteResource_.Get(), 0, sizeof(WellForGPU) * joints.size());

		// 読み取り用（CS/VS）に遷移
		dxCommon->ResourceTransition(paletteResourceDefault_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

		dxCommon->GetCommandManager()->ExecuteAndWait();
	}

	// SRV（SRVManager 側）— DEFAULT を指す
	paletteSrvIndex_ = SRVManager::GetInstance()->Allocate();
	paletteSrvHandle_.first = SRVManager::GetInstance()->GetCPUDescriptorHandle(paletteSrvIndex_);
	paletteSrvHandle_.second = SRVManager::GetInstance()->GetGPUDescriptorHandle(paletteSrvIndex_);
	SRVManager::GetInstance()->CreateSRVForStructureBuffer(paletteSrvIndex_, paletteResourceDefault_.Get(), static_cast<uint32_t>(joints.size()), sizeof(WellForGPU));

	// t2: インフルエンス（UPLOAD を作成して Map）
	VertexInfluence* mappedInfluence = nullptr;
	influenceResource_ = ResourceManager::CreateBufferResource(device, sizeof(VertexInfluence) * totalVerts);
	influenceResource_->SetName(L"SkinCluster_InfluenceResource_UPLOADED");
	influenceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * totalVerts);
	mappedInfluenceData_ = { mappedInfluence, totalVerts }; // span

	// inverseBindPose 配列
	inverseBindPoseMatrices_.resize(joints.size(), Matrix4x4::MakeIdentity());

	// --- Influence 書き込み & 範囲チェック ---
	const auto& jointMap = skeleton.GetJointMap();
	for (const auto& [jName, jWeightData] : modelData.skinClusterData)
	{
		auto it = jointMap.find(jName);
		if (it == jointMap.end()) continue;

		uint32_t jIdx = it->second;
		inverseBindPoseMatrices_[jIdx] = jWeightData.inverseBindPoseMatrix;

		for (const auto& vw : jWeightData.vertexWeights)
		{
			if (vw.vertexIndex >= mappedInfluenceData_.size()) continue;

			auto& inf = mappedInfluenceData_[vw.vertexIndex];
			for (uint32_t i = 0; i < kNumMaxInfluence; ++i)
			{
				if (inf.weights[i] == 0.0f)
				{
					inf.weights[i] = vw.weight;
					inf.jointIndices[i] = jIdx;
					break;
				}
			}
		}
	}

	// ===== DEFAULT（読取用）を作成 → UPLOAD からコピー（初期 COMMON → 明示遷移）=====
	{
		const UINT64 infSize = UINT64(sizeof(VertexInfluence)) * UINT64(totalVerts);
		D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC    bufDesc = CD3DX12_RESOURCE_DESC::Buffer(infSize);

		// 初期ステートは COMMON
		HRESULT hr = S_FALSE;
		hr = device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&influenceResourceDefault_));
		influenceResourceDefault_->SetName(L"SkinCluster_InfluenceResource_DEFAULT");
		assert(SUCCEEDED(hr));

		// COMMON → COPY_DEST に明示遷移
		dxCommon->ResourceTransition(influenceResourceDefault_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

		// UPLOAD → DEFAULT へコピー
		commandList->CopyBufferRegion(influenceResourceDefault_.Get(), 0, influenceResource_.Get(), 0, infSize);

		// 読み取り用に遷移
		dxCommon->ResourceTransition(influenceResourceDefault_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

		dxCommon->GetCommandManager()->ExecuteAndWait();
	}

	// VS 用 VBV（slot1）は DEFAULT を指す
	influenceBufferView_.BufferLocation = influenceResourceDefault_->GetGPUVirtualAddress();
	influenceBufferView_.SizeInBytes = UINT(sizeof(VertexInfluence) * totalVerts);
	influenceBufferView_.StrideInBytes = sizeof(VertexInfluence);

	// t0 : パレット — DEFAULT を指す
	paletteSrvIndexOnUavHeap_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateSRVForStructureBuffer(paletteSrvIndexOnUavHeap_, paletteResourceDefault_.Get(), static_cast<UINT>(mappedPalette_.size()), sizeof(WellForGPU));
	paletteSrvGpuOnUavHeap_ = UAVManager::GetInstance()->GetGPUDescriptorHandle(paletteSrvIndexOnUavHeap_);

	// t2 : インフルエンス — DEFAULT を指す
	influenceSrvIndexOnUavHeap_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateSRVForStructureBuffer(influenceSrvIndexOnUavHeap_, influenceResourceDefault_.Get(), static_cast<UINT>(mappedInfluenceData_.size()), sizeof(VertexInfluence));
	influenceSrvGpuOnUavHeap_ = UAVManager::GetInstance()->GetGPUDescriptorHandle(influenceSrvIndexOnUavHeap_);
}

/// -------------------------------------------------------------
///				スケルトンからパレット行列を更新
/// -------------------------------------------------------------
void SkinCluster::UpdatePaletteMatrix(Skeleton& skeleton)
{
	auto& joints = skeleton.GetJoints();

	// パレット行列計算
	for (size_t jointIndex = 0; jointIndex < joints.size(); ++jointIndex)
	{
		assert(jointIndex < inverseBindPoseMatrices_.size());
		mappedPalette_[jointIndex].skeletonSpaceMatrix = inverseBindPoseMatrices_[jointIndex] * joints[jointIndex].skeletonSpaceMatrix;
		mappedPalette_[jointIndex].skeletonSpaceInverceTransposeMatrix = Matrix4x4::Transpose(Matrix4x4::Inverse(mappedPalette_[jointIndex].skeletonSpaceMatrix));
	}

	// 毎フレ：UPLOAD → DEFAULT へ Copy（既存どおりでOK）
	auto* dxCommon = DirectXCommon::GetInstance();
	auto* commandLisht = dxCommon->GetCommandManager()->GetCommandList();

	// 書き込み用に遷移
	dxCommon->ResourceTransition(paletteResourceDefault_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
	paletteResourceDefault_->SetName(L"SkinCluster_PaletteResource_DEFAULT_Update");

	// コピー
	const UINT64 bytes = UINT64(sizeof(WellForGPU)) * UINT64(joints.size());
	commandLisht->CopyBufferRegion(paletteResourceDefault_.Get(), 0, paletteResource_.Get(), 0, bytes);

	// 読み取り用に遷移
	dxCommon->ResourceTransition(paletteResourceDefault_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
}

} // namespace Ken4lowEngine
