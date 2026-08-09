#pragma once
#include <DX12Include.h>
#include "BlendStateFactory.h"

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///			　GPUパーティクルスプライトパイプラインクラス
/// -------------------------------------------------------------
class GpuParticleSpritePipeline
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 初期化（RootSig + Graphics PSO 作成）
	/// </summary>
	void Initialize();

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 描画用ルートシグネチャ
	/// </summary>
	ID3D12RootSignature* GetGfxRootSignature() const { return rootSignature_.Get(); }

	/// <summary>
	/// 描画用Graphics PSO
	/// </summary>
	ID3D12PipelineState* GetGfxPSO() const { return pipelineState_.Get(); }

private: /// ---------- 内部メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// パイプラインステートオブジェクトの生成
	void CreatePSO();

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通管理
	DirectXCommon* dxCommon_ = nullptr;

	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;

	// 今は加算固定（必要なら将来 “Add/Alpha” でPSOを増やす）
	BlendMode blendMode_ = BlendMode::kBlendModeAdd;
};


} // namespace Ken4lowEngine
