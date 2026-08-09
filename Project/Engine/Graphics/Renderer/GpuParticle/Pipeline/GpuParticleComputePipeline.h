#pragma once
#include <DX12Include.h>

namespace Ken4lowEngine
{

class DirectXCommon;

/// -------------------------------------------------------------
///  GPUパーティクル（CS専用）パイプライン
///  ※ Emit / Update / Simulation の Dispatch はここだけを見る
/// -------------------------------------------------------------
class GpuParticleComputePipeline
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

public: /// ---------- ゲッター ---------- ///

	ID3D12RootSignature* GetCsRootSignature() const { return computeRootSignature_.Get(); }
	ID3D12PipelineState* GetCsPSO() const { return computePipelineState_.Get(); }
	ID3D12PipelineState* GetCsEmitPSO() const { return emitComputePipelineState_.Get(); }
	ID3D12PipelineState* GetCsUpdatePSO() const { return updateComputePipelineState_.Get(); }

private: /// ---------- 内部メンバ関数 ---------- ///

	// コンピュートシェーダ用ルートシグネチャの生成
	void CreateComputeRootSignature();

	// シミュレーション用コンピュートPSOの生成
	void CreateComputePSO();

	// エミット用コンピュートPSOの生成
	void CreateEmitComputePSO();

	// 更新用コンピュートPSOの生成
	void CreateUpdateComputePSO();

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通管理
	DirectXCommon* dxCommon_ = nullptr;

	ComPtr<ID3D12RootSignature> computeRootSignature_;
	ComPtr<ID3D12PipelineState> computePipelineState_;
	ComPtr<ID3D12PipelineState> emitComputePipelineState_;
	ComPtr<ID3D12PipelineState> updateComputePipelineState_;
};

} // namespace Ken4lowEngine
