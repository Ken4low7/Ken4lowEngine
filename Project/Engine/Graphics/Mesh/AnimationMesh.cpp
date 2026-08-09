#include "AnimationMesh.h"
#include <ResourceManager.h>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　　　 初期化処理
/// -------------------------------------------------------------
void AnimationMesh::Initialize(ID3D12Device* device, const ModelData& modelData)
{
	subMeshes_.clear(); // 既存データをクリア
	subMeshes_.reserve(modelData.subMeshes.size()); // 必要な分だけメモリを確保

	// サブメッシュごとにループ
	for (const auto& subMesh : modelData.subMeshes)
	{
		// サブメッシュ用のGPUリソースを作成
		SubMeshGPU gpu = {};

		// 頂点バッファの作成
		size_t vertexCount = sizeof(VertexData) * subMesh.vertices.size();					// 頂点データサイズ
		gpu.vertexResource_ = ResourceManager::CreateBufferResource(device, vertexCount);	// 頂点バッファリソース作成
		gpu.vertexBufferView_.BufferLocation = gpu.vertexResource_->GetGPUVirtualAddress();	// 頂点バッファビュー設定
		gpu.vertexBufferView_.SizeInBytes = UINT(vertexCount);								// 頂点バッファサイズ設定
		gpu.vertexBufferView_.StrideInBytes = sizeof(VertexData);							// 頂点一つ分のサイズ設定

		// 頂点データのマッピング
		VertexData* vertexData = nullptr;
		gpu.vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));	 // 頂点データマッピング
		std::span<VertexData> vertexSpan{ vertexData, subMesh.vertices.size() };		 // span 作成
		std::copy(subMesh.vertices.begin(), subMesh.vertices.end(), vertexSpan.begin()); // 頂点データコピー
		gpu.vertexResource_->SetName(L"AnimationMesh VertexBuffer");

		// インデックスバッファの作成
		size_t indexCount = sizeof(uint32_t) * subMesh.indices.size();					  // インデックスデータサイズ
		gpu.indexResource_ = ResourceManager::CreateBufferResource(device, indexCount);	  // インデックスバッファリソース作成
		gpu.indexBufferView_.BufferLocation = gpu.indexResource_->GetGPUVirtualAddress(); // インデックスバッファビュー設定
		gpu.indexBufferView_.SizeInBytes = UINT(indexCount);							  // インデックスバッファサイズ設定
		gpu.indexBufferView_.Format = DXGI_FORMAT_R32_UINT;								  // インデックスフォーマット設定

		// インデックスデータのマッピング
		uint32_t* indexData = nullptr;
		gpu.indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));	  // インデックスデータマッピング
		std::span<uint32_t> indexSpan{ indexData, subMesh.indices.size() };			  // span 作成
		std::copy(subMesh.indices.begin(), subMesh.indices.end(), indexSpan.begin()); // インデックスデータコピー
		gpu.indexResource_->SetName(L"AnimationMesh IndexBuffer");

		// 作成したサブメッシュを保存
		subMeshes_.emplace_back(std::move(gpu));
	}
}

} // namespace Ken4lowEngine
