#pragma once
#include "DX12Include.h"
#include "ModelData.h"
#include "Matrix4x4.h"

#include <span>
#include <vector>
#include <utility>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class Skeleton;

/// -------------------------------------------------------------
///				　		スキンクラスタクラス
/// -------------------------------------------------------------
class SkinCluster
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// デストラクタ。<br/>
	/// SRVManager / UAVManager 上で確保したディスクリプタを解放し、<br/>
	/// 参照している GPU リソースを後始末します。
	/// </summary>
	~SkinCluster();

	/// <summary>
	/// モデルデータとスケルトン情報をもとに、スキニング用リソースを初期化します。<br/>
	/// ・modelData.subMeshes から総頂点数を集計<br/>
	/// ・ジョイント数分のパレット用 UPLOAD / DEFAULT バッファを生成し、SRV を作成<br/>
	/// ・頂点数分のインフルエンス用 UPLOAD / DEFAULT バッファを生成し、SRV を作成<br/>
	/// ・modelData.skinClusterData と skeleton の jointMap を使って<br/>
	///   各頂点のインフルエンス(最大 kNumMaxInfluence)を書き込む<br/>
	/// ・各ジョイントの逆バインドポーズ行列を inverseBindPoseMatrices_ に保存<br/>
	/// を行います。
	/// </summary>
	/// <param name="modelData">
	/// サブメッシュ単位の頂点配列や skinClusterData を含むモデルデータ。
	/// </param>
	/// <param name="skeleton">
	/// ジョイント階層と jointName → index マップを持つスケルトン。
	/// </param>
	void Initialize(const ModelData& modelData, Skeleton& skeleton);

	/// <summary>
	/// Skeleton からパレット行列を更新します。<br/>
	/// ・各ジョイントごとに<br/>
	///   palette[j] = inverseBindPose * joints[j].skeletonSpaceMatrix<br/>
	///   を計算し、法線用に逆転置行列も作成<br/>
	/// ・計算結果を UPLOAD バッファ (paletteResource_) に書き込み<br/>
	/// ・UPLOAD → DEFAULT(paletteResourceDefault_) に CopyBufferRegion で転送し、<br/>
	///   読み取り用ステート(D3D12_RESOURCE_STATE_GENERIC_READ) に戻す<br/>
	/// といった処理を行います。
	/// </summary>
	/// <param name="skeleton">最新のジョイント行列を持つスケルトン。</param>
	void UpdatePaletteMatrix(Skeleton& skeleton);

	/// <summary>
	/// 頂点ごとのインフルエンス情報を格納した頂点バッファビューを取得します。<br/>
	/// IA の slot1 などにセットして、VS でスキニングに利用します。
	/// </summary>
	/// <returns>VertexInfluence 用の D3D12_VERTEX_BUFFER_VIEW。</returns>
	const D3D12_VERTEX_BUFFER_VIEW& GetInfluenceBufferView() const { return influenceBufferView_; }

	/// <summary>
	/// パレット行列配列(WellForGPU[])への SRV ハンドルを取得します。<br/>
	/// SRVManager 側のディスクリプタヒープ（通常のテクスチャ等と同じヒープ）上の<br/>
	/// CPU / GPU ハンドルのペアを返します。
	/// </summary>
	/// <returns>CPU / GPU の SRV ディスクリプタハンドルのペア。</returns>
	const std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>& GetPaletteSrvHandle() const { return paletteSrvHandle_; }

	/// <summary>
	/// Compute シェーダなどで使用するために、UAVManager ヒープ上の<br/>
	/// パレット SRV の GPU ディスクリプタハンドルを取得します。
	/// </summary>
	/// <returns>UAVManager ヒープ上のパレット SRV の GPU ハンドル。</returns>
	D3D12_GPU_DESCRIPTOR_HANDLE GetPaletteSrvOnUAVHeap() const { return paletteSrvGpuOnUavHeap_; }

	/// <summary>
	/// Compute シェーダ用に、インフルエンス配列(VertexInfluence[])の<br/>
	/// SRV GPU ハンドル（UAVManager ヒープ上）を取得します。
	/// </summary>
	/// <returns>UAVManager ヒープ上のインフルエンス SRV の GPU ハンドル。</returns>
	D3D12_GPU_DESCRIPTOR_HANDLE GetInfluenceSrvOnUAVHeap() const { return influenceSrvGpuOnUavHeap_; }

private: /// ---------- メンバ変数 ---------- ///

	std::vector<Matrix4x4> inverseBindPoseMatrices_; // パレット行列

	// influence（頂点ごとのデータ）
	ComPtr<ID3D12Resource> influenceResource_; // 頂点バッファリソース
	D3D12_VERTEX_BUFFER_VIEW influenceBufferView_{}; // 頂点バッファビュー
	std::span<VertexInfluence> mappedInfluenceData_; // マッピングしたデータ

	// palette（ジョイント行列の配列）
	ComPtr<ID3D12Resource> paletteResource_; // パレットリソース
	std::span<WellForGPU> mappedPalette_; // マッピングしたデータ
	uint32_t paletteSrvIndex_ = UINT32_MAX; // SRVのインデックス
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle_; // SRVのハンドル

	ComPtr<ID3D12Resource> influenceResourceDefault_; // CS/VS が読む DEFAULT 常駐
	ComPtr<ID3D12Resource> paletteResourceDefault_;  // CS/VS が読む用（毎フレ Copy で更新）
	uint32_t paletteSrvIndexOnUavHeap_ = UINT32_MAX; // t0
	uint32_t influenceSrvIndexOnUavHeap_ = UINT32_MAX; // t2
	D3D12_GPU_DESCRIPTOR_HANDLE paletteSrvGpuOnUavHeap_{}; // t0 
	D3D12_GPU_DESCRIPTOR_HANDLE influenceSrvGpuOnUavHeap_{}; // t2
};


} // namespace Ken4lowEngine
