#pragma once
#include <DX12Include.h>
#include "BlendStateFactory.h"

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///			　GPUパーティクルメッシュパイプラインクラス
/// -------------------------------------------------------------
class GpuParticleMeshPipeline
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 
	/// </summary>
	void Initialize();

public: /// ---------- ゲッター ---------- ///

	ID3D12RootSignature* GetGfxRootSignature() const { return rootSignature_.Get(); }
	ID3D12PipelineState* GetGfxPSO() const { return pipelineState_.Get(); }

private: /// ---------- 内部メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// パイプラインステートオブジェクトの生成
	void CreatePSO();

private:
	DirectXCommon* dxCommon_ = nullptr;

	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;

	// まずは加算でOK（必要ならAlpha/Opaqueへ増やす）
	BlendMode blendMode_ = BlendMode::kBlendModeAdd;
};


} // namespace Ken4lowEngine
