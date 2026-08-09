#pragma once
#include "DX12Include.h"
#include <BlendModeType.h>
#include "Vector3.h"
#include "Quaternion.h"
#include <ModelData.h>
#include <TransformationMatrix.h>
#include "LightManager.h"

#include <array>
#include <string>
#include <vector>
#include <numbers>
#include <map>

namespace Ken4lowEngine
{


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///				　アニメーションパイプラインビルダー
/// -------------------------------------------------------------
class AnimationPipelineBuilder
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// AnimationPipelineBuilder のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>AnimationPipelineBuilder の唯一のインスタンス。</returns>
	static AnimationPipelineBuilder* GetInstance();

	/// <summary>
	/// アニメーション描画／Compute スキニング用パイプラインを初期化します。<br/>
	/// ・DirectXCommon の保持<br/>
	/// ・グラフィックス用ルートシグネチャの生成(CreateRootSignature)<br/>
	/// ・グラフィックスパイプラインの生成(CreatePSO)<br/>
	/// ・Compute 用ルートシグネチャの生成(CreateComputeRootSignature)<br/>
	/// ・Compute パイプラインの生成(CreateComputePSO)<br/>
	/// ・LightManager の初期化<br/>
	/// をまとめて行います。
	/// </summary>
	/// <param name="dxCommon">D3D12 デバイスやコマンドリストを管理する DirectXCommon へのポインタ。</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// スキニング済みメッシュを描画する前に、共通のレンダリング設定を行います。 <br/>
	/// ・グラフィックス用ルートシグネチャのセット<br/>
	/// ・アニメーション用グラフィックスパイプラインステートのセット<br/>
	/// ・プリミティブトポロジを三角形リストに設定<br/>
	/// ・LightManager::BindPunctualLights() によるライト情報のバインド<br/>
	/// を行い、その後で各メッシュの Draw() を呼び出す想定です。
	/// </summary>
	void Finalize();

	/// <summary>
	/// スキニング済みメッシュを描画する前に、共通のレンダリング設定を行います。 <br/>
	/// ・グラフィックス用ルートシグネチャのセット<br/>
	/// ・アニメーション用グラフィックスパイプラインステートのセット<br/>
	/// ・プリミティブトポロジを三角形リストに設定<br/>
	/// ・LightManager::BindPunctualLights() によるライト情報のバインド<br/>
	/// を行い、その後で各メッシュの Draw() を呼び出す想定です。
	/// </summary>
	void SetRenderSetting();

	/// <summary>
	/// Compute スキニングを実行する前に、Compute 用ルートシグネチャと<br/>
	/// パイプラインステートをコマンドリストに設定します。  
	/// </summary>
	void SetComputeSetting();

	/// <summary>
	/// アニメーション描画用グラフィックスパイプラインのルートシグネチャを取得します。
	/// </summary>
	/// <returns>ID3D12RootSignature へのポインタ。</returns>
	ID3D12RootSignature* GetRootSignature() const { return rootSignature.Get(); }

	/// <summary>
	/// アニメーション描画用のグラフィックスパイプラインステート(PSO)を取得します。
	/// </summary>
	/// <returns>ID3D12PipelineState へのポインタ。</returns>
	ID3D12PipelineState* GetPipelineState() const { return graphicsPipelineState.Get(); }

	/// <summary>
	/// Compute スキニング用ルートシグネチャを取得します。
	/// </summary>
	/// <returns>Compute 用 ID3D12RootSignature へのポインタ。</returns>
	ID3D12RootSignature* GetComputeRootSignature() const { return computeRootSignature_.Get(); }

	/// <summary>
	/// Compute スキニング用パイプラインステート(PSO)を取得します。
	/// </summary>
	/// <returns>Compute 用 ID3D12PipelineState へのポインタ。</returns>
	ID3D12PipelineState* GetComputePipelineState() const { return computePipelineState_.Get(); }

private: /// ---------- 内部メンバ関数 ---------- ///

	/// <summary>
	/// アニメーション描画用のルートシグネチャを生成します。<br/>
	/// ルートパラメータ構成は以下の通りです：<br/>
	/// ・b0 : マテリアル用 CBV (PS)<br/>
	/// ・b0 : TransformationMatrix 用 CBV (VS)<br/>
	/// ・t0 : マテリアルテクスチャ SRV (PS, ディスクリプタテーブル)<br/>
	/// ・b1 : カメラ用 CBV (PS)<br/>
	/// ・t1 : キューブマップ SRV (PS)<br/>
	/// ・b2 : ライト数 CBV (PS)<br/>
	/// ・t2 : ライト配列 SRV (PS)<br/>
	/// ・t6 : Metallic/Roughness Texture SRV (PS)<br/>
	/// ・t7 : Normal Texture SRV (PS)<br/>
	/// ・t8 : AO Texture SRV (PS)<br/>
	/// ・t9 : Emissive Texture SRV (PS)<br/>
	/// また、s0 としてリニアフィルタ＋WRAP の静的サンプラを 1 つ登録し、<br/>
	/// 入力アセンブラの使用を許可するフラグを設定します。
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// アニメーション描画用のグラフィックスパイプラインステート(PSO)を生成します。 <br/>
	/// ・POSITION / TEXCOORD / NORMAL / WEIGHT / INDEX の 5 要素入力レイアウト<br/>
	///   - POSITION/TEXCOORD/NORMAL : 入力スロット 0（頂点）<br/>
	///   - WEIGHT/INDEX            : 入力スロット 1（インフルエンス）<br/>
	/// ・BlendStateFactory から取得したブレンド設定（blendMode_）<br/>
	/// ・ラスタライザステート（Fill=SOLID, カリング無効）<br/>
	/// ・DepthStencilState（Depth 有効, LessEqual）<br/>
	/// ・VS : SkinningObject3d.VS.hlsl / PS : SkinningObject3d.PS.hlsl をコンパイルして設定<br/>
	/// ・RTV = R8G8B8A8_UNORM_SRGB, DSV = D24_UNORM_S8_UINT, PrimitiveTopologyType = TRIANGLE<br/>
	/// を設定し、ID3D12PipelineState を生成します。
	/// </summary>
	void CreatePSO();

	/// <summary>
	/// Compute スキニング用のルートシグネチャを生成します。<br/>
	/// ルートパラメータ構成は以下の通りです：<br/>
	/// ・t0 : マトリックスパレット SRV（MatrixPalette）<br/>
	/// ・t1 : 頂点入力 SRV（元頂点バッファ）<br/>
	/// ・t2 : インフルエンス SRV（頂点ごとのジョイント重み）<br/>
	/// ・u0 : 頂点出力 UAV（スキニング済み頂点を書き込む）<br/>
	/// ・b0 : 定数バッファ（スキニング設定など）<br/>
	/// ・b1 : 定数バッファ（頂点数など追加情報）<br/>
	/// また、s0 としてリニアフィルタ＋WRAP の静的サンプラを 1 つ登録します。
	/// </summary>
	void CreateComputeRootSignature();

	/// <summary>
	/// Compute スキニング用のパイプラインステート(PSO)を生成します。 <br/>
	/// ・SkinningObject3d.CS.hlsl を cs_6_0 でコンパイル<br/>
	/// ・CreateComputePipelineState により PSO を生成<br/>
	/// を行います。
	/// </summary>
	void CreateComputePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネチャとパイプラインステート
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> graphicsPipelineState;

	// コンピュート用ルートシグネチャとパイプライン
	ComPtr <ID3D12RootSignature> computeRootSignature_;
	ComPtr<ID3D12PipelineState> computePipelineState_;

	// ブレンドモード
	BlendMode blendMode_ = BlendMode::kBlendModeNone;
};


} // namespace Ken4lowEngine
