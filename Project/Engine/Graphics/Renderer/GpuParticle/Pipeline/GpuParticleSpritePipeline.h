#pragma once
#include <DX12Include.h>
#include "BlendStateFactory.h"
#include "BlendModeType.h"

#include <array>

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
	ID3D12PipelineState* GetGfxPSO(BlendMode blendMode) const;

private: /// ---------- 内部メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// BlendModeごとのPSOを生成し、Authoring側のAlpha/Additive/Multiplyを実描画へ反映する。
	void CreatePSO(BlendMode blendMode);

private: /// ---------- メンバ変数 ---------- ///

	static constexpr size_t kBlendModeCount = static_cast<size_t>(BlendMode::kcountOfBlendMode);

	// DirectX共通管理
	DirectXCommon* dxCommon_ = nullptr;

	ComPtr<ID3D12RootSignature> rootSignature_;
	std::array<ComPtr<ID3D12PipelineState>, kBlendModeCount> pipelineStates_{};
};


} // namespace Ken4lowEngine
