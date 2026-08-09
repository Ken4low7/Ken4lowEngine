#pragma once
#include "DX12Include.h"
#include "ModelData.h"

#include <memory>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　	　アニメーションメッシュクラス
/// -------------------------------------------------------------
class AnimationMesh
{
private: /// ---------- 型定義 ---------- ///

	// サブメッシュ一つ分のGPUリソースをまとめた構造体
	struct SubMeshGPU
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_; // 頂点バッファリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;	// インデックスバッファリソース
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};			// 頂点バッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferView_{};				// インデックスバッファビュー
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定した ID3D12Device とモデルデータを使用して、内部リソースや状態を初期化します。
	/// </summary>
	/// <param name="device">初期化に使用する ID3D12Device へのポインタ。バッファやテクスチャなどの GPU リソースの作成に使用されます。</param>
	/// <param name="modelData">初期化対象のモデルに関する頂点、インデックス、マテリアルなどの情報を保持する const 参照。</param>
	void Initialize(ID3D12Device* device, const ModelData& modelData);

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// サブメッシュの数を取得します。
	/// </summary>
	/// <returns>サブメッシュの数を size_t 型で返します。</returns>
	size_t GetSubmeshCount() const { return subMeshes_.size(); }

	/// <summary>
	/// 指定したサブメッシュの頂点バッファビューを返す const メンバー関数です。
	/// </summary>
	/// <param name="i">取得するサブメッシュのインデックス。</param>
	/// <returns>指定したサブメッシュに対応する D3D12_VERTEX_BUFFER_VIEW への const 参照。</returns>
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView(size_t i) const { return subMeshes_[i].vertexBufferView_; }

	/// <summary>
	/// 指定したサブメッシュのインデックスバッファビューへの const 参照を返します。
	/// </summary>
	/// <param name="i">取得するサブメッシュの添字（size_t）。配列の範囲外を渡すと未定義動作になります。</param>
	/// <returns>指定されたサブメッシュの D3D12_INDEX_BUFFER_VIEW への const 参照。</returns>
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView(size_t i)  const { return subMeshes_[i].indexBufferView_; }

	/// <summary>
	/// 指定したサブメッシュの頂点バッファ リソースへのポインターを返します。
	/// </summary>
	/// <param name="i">取得するサブメッシュのインデックス。範囲外の場合は未定義の動作になる可能性があります。</param>
	/// <returns>指定したサブメッシュに対応する ID3D12Resource*（頂点バッファ）</returns>
	ID3D12Resource* GetVertexBufferResource(size_t i) const { return subMeshes_[i].vertexResource_.Get(); }

	/// <summary>
	/// 指定したサブメッシュのインデックスバッファリソースへのポインターを取得します（const メンバー関数）。
	/// </summary>
	/// <param name="i">取得するサブメッシュのインデックス（0 から始まる）。</param>
	/// <returns>該当するサブメッシュのインデックスバッファリソースへの ID3D12Resource*</returns>
	ID3D12Resource* GetIndexBufferResource(size_t i)  const { return subMeshes_[i].indexResource_.Get(); }

public: /// ---------- ゲッター（サブメッシュ0固定版） ---------- ///

	/// <summary>
	/// この const メンバー関数は、先頭のサブメッシュが保持する頂点バッファビューへの const 参照を返します。
	/// </summary>
	/// <returns>先頭サブメッシュの D3D12_VERTEX_BUFFER_VIEW の const 参照（subMeshes_.front().vertexBufferView_）</returns>
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return subMeshes_.front().vertexBufferView_; }

	/// <summary>
	/// subMeshes_ の先頭サブメッシュが保持するインデックス バッファ ビューへの定数参照を返します。
	/// </summary>
	/// <returns>先頭サブメッシュ (subMeshes_.front()) の indexBufferView_ を指す </returns>
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView()  const { return subMeshes_.front().indexBufferView_; }

	/// <summary>
	/// 先頭のサブメッシュが保持する頂点バッファ用のID3D12Resourceポインターを返します。
	/// </summary>
	/// <returns>先頭サブメッシュの頂点バッファリソースへの生ポインター</returns>
	ID3D12Resource* GetVertexBufferResource() const { return subMeshes_.front().vertexResource_.Get(); }

	/// <summary>
	/// 先頭のサブメッシュが保持するインデックス バッファ リソースへのポインターを返します。
	/// </summary>
	/// <returns>ID3D12Resource*：subMeshes_ の先頭要素の indexResource_ から取得したインデックス バッファ リソースへのポインター</returns>
	ID3D12Resource* GetIndexBufferResource()  const { return subMeshes_.front().indexResource_.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	// サブメッシュごとのGPUリソース配列
	std::vector<SubMeshGPU> subMeshes_;
};


} // namespace Ken4lowEngine
