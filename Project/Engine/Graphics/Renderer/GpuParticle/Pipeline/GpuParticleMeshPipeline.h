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
	ID3D12PipelineState* GetGfxPSO(BlendMode blendMode) const;

private: /// ---------- 内部メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// Authoring側のBlendModeごとにMesh用PSOも生成する。
	void CreatePSO(BlendMode blendMode);

private:
	static constexpr size_t kBlendModeCount = static_cast<size_t>(BlendMode::kcountOfBlendMode);

	DirectXCommon* dxCommon_ = nullptr;

	ComPtr<ID3D12RootSignature> rootSignature_;
	std::array<ComPtr<ID3D12PipelineState>, kBlendModeCount> pipelineStates_{};
};


} // namespace Ken4lowEngine
