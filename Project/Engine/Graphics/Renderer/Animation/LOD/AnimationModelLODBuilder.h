#pragma once
#include "DX12Include.h"
#include "ModelData.h"


namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;
	class Skeleton;
	class AnimationModel;
	class SkinCluster;

	/// -------------------------------------------------------------
	///			　アニメーションモデルLODビルダークラス
	/// -------------------------------------------------------------
	class AnimationModelLODBuilder
	{
	public: /// ---------- 構造体 ---------- ///

		// LODごとの情報
		struct LODEntry
		{
			// 入力（共有候補）：DEFAULTの頂点SRV、Influence SRV、IB
			ComPtr<ID3D12Resource> staticVBDefault; // t1
			D3D12_VERTEX_BUFFER_VIEW influenceVBV = {};  // VSで使わないならなくても可

			// インデックスバッファの実体を保持（解放されないように）
			ComPtr<ID3D12Resource> indexBuffer;     //インデックスバッファ

			D3D12_INDEX_BUFFER_VIEW  ibv{};
			uint32_t vertexCount = 0;
			uint32_t indexCount = 0;

			// 出力（インスタンス固有）：スキン結果u0とVBV、UAVディスクリプタ
			ComPtr<ID3D12Resource>  skinnedVB;     // u0
			D3D12_VERTEX_BUFFER_VIEW skinnedVBV = {};
			uint32_t uavIndex = UINT32_MAX; // UAVヒープのu0
			uint32_t srvInputVerticesOnUavHeap = UINT32_MAX; // t1 SRV on UAV heap

			D3D12_GPU_DESCRIPTOR_HANDLE influenceSrvGpuOnUavHeap = {}; // t2
			// 出力VBのリソース状態
			D3D12_RESOURCE_STATES skinnedState = D3D12_RESOURCE_STATE_COMMON;

			// サブメッシュごとの描画範囲とマテリアルSRV
			struct SubMeshRange
			{
				uint32_t startIndex = 0;
				uint32_t indexCount = 0;
				D3D12_GPU_DESCRIPTOR_HANDLE baseColorSrvGpuHandle{}; // t2用
			};
			std::vector<SubMeshRange> subMeshRanges; // subMeshごとに分割
		};

		// subMesh をフラット化した結果
		struct FlattenResult
		{
			std::vector<VertexData> vertices; // 全頂点
			std::vector<uint32_t>   indices;  // 全インデックス
			std::vector<LODEntry::SubMeshRange> ranges; // サブメッシュ範囲
		};

	public: /// ---------- 静的メンバ関数 ---------- ///

		static void Build(
			DirectXCommon* dxCommon,
			Skeleton& skeleton,
			const std::string& lod0File,
			const std::vector<std::string>& lodFiles,
			std::vector<LODEntry>& outLods,
			std::vector<std::unique_ptr<SkinCluster>>& outSkinClusters,
			std::vector<std::string>& outLodFileNames);

		// subMesh をフラット化
		static FlattenResult FlattenSubMeshes(const ModelData& md);

		// テクスチャロード＆SRV取得（空ならフォールバック）
		static D3D12_GPU_DESCRIPTOR_HANDLE LoadSrvOrFallback(const std::string& path);
	};
}
