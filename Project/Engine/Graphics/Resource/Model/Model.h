#pragma once
#include "DX12Include.h"
#include <ModelData.h>
#include <Mesh.h>
#include "Engine/Graphics/Culling/Frustum.h"

#include <cstddef>
#include <vector>
#include <string>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///							モデルクラス
	/// --------------------------------------------------------------
	class Model
	{
	public: /// ---------- メンバ関数 ---------- ///

		// モデルデータのファイルパスを指定して初期化を行います。
		void Initialize(const std::string& filePath);

		// モデルの描画処理
		void BuildLocalBounds();

	public: /// ---------- アクセッサ ---------- ///

		// モデルデータの取得
		const ModelData& GetModelData() const { return modelData_; }

		// メッシュの取得
		const std::vector<Mesh>& GetMeshes() const { return meshes_; }

		// メッシュの取得（書き込み可能）
		std::vector<Mesh>& GetMeshes() { return meshes_; }

		// モデル全体のインデックス数を取得
		uint64_t GetTotalIndexCount() const;

		// モデル全体の頂点数を取得
		uint64_t GetTotalVertexCount() const;

		// モデル全体のローカル空間での境界球を取得
		const BoundingSphere& GetLocalBounds() const { return localBounds_; }

		// モデル全体のローカル空間での境界球が有効かどうか
		bool HasLocalBounds() const { return hasLocalBounds_; }

		// 各メッシュのローカル空間での境界球を取得
		const BoundingSphere& GetMeshLocalBounds(size_t index) const { return meshLocalBounds_.at(index); }

		// 各メッシュのローカル空間での境界球が有効かどうか
		bool HasMeshLocalBounds(size_t index) const { return index < meshHasLocalBounds_.size() && meshHasLocalBounds_[index]; }

		// マテリアルのSRVハンドルの取得
		const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& GetMaterialSRVs() const { return materialSRVs_; }

		// マテリアルのSRVハンドルの取得（書き込み可能）
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& GetMaterialSRVs() { return materialSRVs_; }

		// マテリアルのポイントサンプリングフラグの取得
		const std::vector<bool>& GetMaterialPointSamplingFlags() const { return materialUsePointSampling_; }

	private: /// ---------- メンバ変数 ---------- ///

		ModelData modelData_;
		std::vector<Mesh> meshes_;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;
		std::vector<bool> materialUsePointSampling_{};
		BoundingSphere localBounds_{};
		bool hasLocalBounds_ = false;
		std::vector<BoundingSphere> meshLocalBounds_{};
		std::vector<bool> meshHasLocalBounds_{};
	};
}
