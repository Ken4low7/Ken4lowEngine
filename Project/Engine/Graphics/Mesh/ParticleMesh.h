#pragma once
#include <DX12Include.h>
#include "VertexData.h"

#include <vector>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　	　パーティクルメッシュクラス
///	-------------------------------------------------------------
class ParticleMesh
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// リングの頂点データを生成
	/// </summary>
	void InitializeRing();

	/// <summary>
	/// シリンダーの頂点データの初期化処理
	/// </summary>
	void InitializeCylinder();

	/// <summary>
	/// 星型の頂点データの初期化処理
	/// </summary>
	void InitializeStar();

	/// <summary>
	/// スモークの頂点データの初期化処理
	/// </summary>
	void InitializeSmoke();

	/// <summary>
	/// 描画処理
	/// <summary>
	/// <param name="instanceCount">インスタンス数</param>
	void Draw(UINT instanceCount);

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 内部メンバー vertexBufferView_ への読み取り専用 (const) 参照として頂点バッファビューを返します。
	/// </summary>
	/// <returns>D3D12_VERTEX_BUFFER_VIEW 型のオブジェクトへの const 参照（内部の vertexBufferView_）。</returns>
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }

	/// <summary>
	/// インデックス バッファ ビューへの const 参照を返します。
	/// </summary>
	/// <returns>内部メンバ indexBufferView_ の const D3D12_INDEX_BUFFER_VIEW&。インデックス バッファ ビューの情報を読み取り用に参照します。</returns>
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }

	/// <summary>
	/// インデックスが存在するかどうかを示すブール値を返します。
	/// </summary>
	/// <returns>インデックスが存在する場合は true、存在しない場合は false を返します。</returns>
	bool HasIndex() const { return hasIndex_; }

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 頂点バッファを作成します。
	/// </summary>
	void CreateVertexBuffer();

private: /// ---------- メンバ変数 ---------- ///

	// 自前のジオメトリ（Particleはファイルロードしない）
	std::vector<VertexData> vertices;
	std::vector<uint32_t>   indices;   // ない場合もある

	ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	VertexData* vertexData_ = nullptr;

	// インデックス対応
	ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	bool hasIndex_ = false;
};


} // namespace Ken4lowEngine
