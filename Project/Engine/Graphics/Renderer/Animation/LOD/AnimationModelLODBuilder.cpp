#include "AnimationModelLODBuilder.h"
#include "DirectXCommon.h"
#include "AssimpLoader.h"
#include "ResourceManager.h"
#include "UAVManager.h"
#include "SkinCluster.h"
#include <cassert>
#include "TextureManager.h"

namespace Ken4lowEngine
{

	void Ken4lowEngine::AnimationModelLODBuilder::Build(DirectXCommon* dxCommon, Skeleton& skeleton, const std::string& lod0File, const std::vector<std::string>& lodFiles, std::vector<LODEntry>& outLods, std::vector<std::unique_ptr<SkinCluster>>& outSkinClusters, std::vector<std::string>& outLodFileNames)
	{
		assert(dxCommon);
		auto* device = dxCommon->GetDevice();

		// 入力：lod0File は LOD0、lodFiles は LOD1+ を想定。
		// ただし "LOD0込み" が渡されても破綻しないように両対応しておく。
		std::vector<std::string> files;
		if (!lodFiles.empty() && lodFiles.front() == lod0File)
		{
			files = lodFiles;
		}
		else if (!lodFiles.empty())
		{
			files.reserve(1 + lodFiles.size());
			files.push_back(lod0File);
			files.insert(files.end(), lodFiles.begin(), lodFiles.end());
		}
		else
		{
			files = { lod0File };
		}

		// 書き込み前に必ずサイズ確保
		outLods.clear();
		outLods.resize(files.size());
		outSkinClusters.clear();
		outSkinClusters.resize(files.size());
		outLodFileNames = files;

		// LODごとに処理
		for (size_t i = 0; i < files.size(); ++i)
		{
			const std::string& fname = files[i];

			// --- モデル読込（同一スケルトン前提） ---
			ModelData md = AssimpLoader::LoadModel(fname);

			// サブメッシュをフラット化
			auto flat = FlattenSubMeshes(md);
			const UINT vbSize = UINT(sizeof(VertexData) * flat.vertices.size());

			// --- VB: DEFAULT で作成 ---
			ComPtr<ID3D12Resource> defaultVB;
			{
				D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
				D3D12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

				HRESULT hr = device->CreateCommittedResource(
					&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&defaultVB));
				assert(SUCCEEDED(hr));

				ComPtr<ID3D12Resource> upload = ResourceManager::CreateBufferResource(device, vbSize);
				void* p = nullptr;
				upload->Map(0, nullptr, &p);
				std::memcpy(p, flat.vertices.data(), vbSize);
				upload->Unmap(0, nullptr);

				auto* cl = dxCommon->GetCommandManager()->GetCommandList();
				dxCommon->ResourceTransition(defaultVB.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
				cl->CopyBufferRegion(defaultVB.Get(), 0, upload.Get(), 0, vbSize);
				dxCommon->ResourceTransition(defaultVB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
				dxCommon->GetCommandManager()->ExecuteAndWait();
			}

			// UAVヒープに t1 SRV を作成
			uint32_t t1Index = UAVManager::GetInstance()->Allocate();
			UAVManager::GetInstance()->CreateSRVForStructureBuffer(
				t1Index, defaultVB.Get(), static_cast<UINT>(flat.vertices.size()), sizeof(VertexData));

			// --- t2: SkinCluster（LOD ごとに作成） ---
			outSkinClusters[i] = std::make_unique<SkinCluster>();
			outSkinClusters[i]->Initialize(md, skeleton);

			// --- IB: DEFAULT で作成 ---
			const UINT ibSize = UINT(sizeof(uint32_t) * flat.indices.size());
			ComPtr<ID3D12Resource> defaultIB;
			{
				D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
				D3D12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

				HRESULT hr = device->CreateCommittedResource(
					&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&defaultIB));
				assert(SUCCEEDED(hr));

				ComPtr<ID3D12Resource> upload = ResourceManager::CreateBufferResource(device, ibSize);
				void* p = nullptr;
				upload->Map(0, nullptr, &p);
				std::memcpy(p, flat.indices.data(), ibSize);
				upload->Unmap(0, nullptr);

				auto* cl = dxCommon->GetCommandManager()->GetCommandList();
				dxCommon->ResourceTransition(defaultIB.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
				cl->CopyBufferRegion(defaultIB.Get(), 0, upload.Get(), 0, ibSize);
				dxCommon->ResourceTransition(defaultIB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
				dxCommon->GetCommandManager()->ExecuteAndWait();
			}

			// IBV 設定
			D3D12_INDEX_BUFFER_VIEW ibv{};
			ibv.BufferLocation = defaultIB->GetGPUVirtualAddress();
			ibv.Format = DXGI_FORMAT_R32_UINT;
			ibv.SizeInBytes = static_cast<UINT>(defaultIB->GetDesc().Width);

			// --- u0: 出力頂点（UAV） & VBV ---
			ComPtr<ID3D12Resource> skinnedVB;
			{
				D3D12_HEAP_PROPERTIES heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
				D3D12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(
					sizeof(VertexData) * flat.vertices.size(),
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

				HRESULT hr = device->CreateCommittedResource(
					&heapDefault, D3D12_HEAP_FLAG_NONE, &desc,
					D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&skinnedVB));
				assert(SUCCEEDED(hr));
			}

			// UAV ヒープに u0 UAV を作成
			uint32_t u0Index = UAVManager::GetInstance()->Allocate();
			UAVManager::GetInstance()->CreateUAVForStructuredBuffer(
				u0Index, skinnedVB.Get(), static_cast<UINT>(flat.vertices.size()), sizeof(VertexData));

			D3D12_VERTEX_BUFFER_VIEW skinnedVBV{};
			skinnedVBV.BufferLocation = skinnedVB->GetGPUVirtualAddress();
			skinnedVBV.SizeInBytes = UINT(sizeof(VertexData) * flat.vertices.size());
			skinnedVBV.StrideInBytes = sizeof(VertexData);

			// --- LODEntry へ格納 ---
			auto& L = outLods[i];
			L.staticVBDefault = defaultVB;
			L.srvInputVerticesOnUavHeap = t1Index;
			L.influenceSrvGpuOnUavHeap = outSkinClusters[i]->GetInfluenceSrvOnUAVHeap();
			L.indexBuffer = defaultIB;
			L.ibv = ibv;
			L.skinnedVB = skinnedVB;
			L.skinnedVBV = skinnedVBV;
			L.uavIndex = u0Index;
			L.vertexCount = static_cast<uint32_t>(flat.vertices.size());
			L.indexCount = static_cast<uint32_t>(flat.indices.size());
			L.skinnedState = D3D12_RESOURCE_STATE_COMMON;

			// サブメッシュ範囲とマテリアルSRVを設定
			L.subMeshRanges = flat.ranges;
			for (size_t si = 0; si < L.subMeshRanges.size(); ++si)
			{
				const auto& sm = md.subMeshes[si];
				L.subMeshRanges[si].baseColorSrvGpuHandle = LoadSrvOrFallback(sm.material.textureFilePath);
			}
		}
	}

	AnimationModelLODBuilder::FlattenResult AnimationModelLODBuilder::FlattenSubMeshes(const ModelData& md)
	{
		// 結果格納用
		FlattenResult out;
		out.vertices.reserve(4096); // 仮予約 : 頂点
		out.indices.reserve(8192);	// 仮予約 : インデックス

		uint32_t baseVertex = 0; // 頂点オフセット
		uint32_t startIndex = 0; // インデックスオフセット

		// サブメッシュごとにループ
		for (const auto& sm : md.subMeshes)
		{
			// 頂点コピー
			out.vertices.insert(out.vertices.end(), sm.vertices.begin(), sm.vertices.end());

			// インデックスコピー（baseVertex を足す）
			LODEntry::SubMeshRange R{};
			R.startIndex = startIndex;								 // 開始インデックス
			R.indexCount = static_cast<uint32_t>(sm.indices.size()); // インデックス数

			// インデックスコピー
			for (uint32_t idx : sm.indices)	out.indices.push_back(idx + baseVertex);

			// オフセット更新
			startIndex += R.indexCount;

			// マテリアルSRVはここでは未設定（Initialize側で設定）
			out.ranges.push_back(R);

			// 頂点オフセット更新
			baseVertex += static_cast<uint32_t>(sm.vertices.size());
		}

		// 戻す
		return out;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Ken4lowEngine::AnimationModelLODBuilder::LoadSrvOrFallback(const std::string& path)
	{
		static const std::string kFallback = "Effects/white.dds";		// フォールバックテクスチャ
		auto* tm = TextureManager::GetInstance();				// テクスチャマネージャ取得
		const std::string& p = path.empty() ? kFallback : path; // パス決定
		tm->LoadTexture(p);										// テクスチャロード
		return tm->GetSrvHandleGPU(p);							// SRVハンドル取得
	}

}