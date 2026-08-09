#pragma once
#include <DX12Include.h>
#include "ParticleMesh.h"
#include "ParticleMaterial.h"
#include "GpuParticleMeshPipeline.h"

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class GpuParticleSpritePipeline;
class GpuParticleBuffers;

/// -------------------------------------------------------------
///			　	GPUパーティクルレンダラークラス
/// -------------------------------------------------------------
class GpuParticleRenderer
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// GPU パーティクルレンダラーの初期化を行います。<br/>
	/// ・パイプライン / バッファのポインタを保持<br/>
	/// ・パーティクルメッシュの生成と初期化<br/>
	/// ・パーティクルマテリアルの生成と初期化<br/>
	/// ・使用テクスチャ（textureFilePath_）の読み込み<br/>
	/// を行います。
	/// </summary>
	/// <param name="pipeline">描画に使用する GPU パーティクル用グラフィックスパイプライン。</param>
	/// <param name="buffers">PerView 定数バッファやパーティクル用 SRV を持つバッファ管理クラス。</param>
	void Initialize(GpuParticleSpritePipeline* pipeline, GpuParticleBuffers* buffers);

	/// <summary>
	/// GPU パーティクルの描画処理を行います。<br/>
	/// textureFilePath_ が "Mesh:1000" のような形式なら MeshParticleAsset を使って描画し、<br/>
	/// それ以外は従来通りスプライト用クアッドで描画します。
	/// </summary>
	/// <param name="instanceCount">描画するパーティクルインスタンス数（GPU 上で生存しているパーティクル数など）。</param>
	void Draw(UINT instanceCount, uint32_t slot = 0);

public: /// ---------- セッター ---------- ///

	// テクスチャファイルパスのセッター
	void SetTextureFilePath(const std::string& path);

	// 描画タイプのセッター
	void SetDrawType(uint32_t type, uint32_t slot);

private: /// ---------- 内部処理 ---------- ///

	bool TryGetMeshIdFromTexturePath(uint32_t& outMeshId) const;
	void DrawSprite(UINT instanceCount, uint32_t slot);
	void DrawMesh(UINT instanceCount, uint32_t slot, uint32_t meshId);

private: /// ---------- メンバ変数 ---------- ///

	// GPUパーティクルスプライトパイプライン
	GpuParticleSpritePipeline* gpuParticlePipeline_ = nullptr;

	// GPUパーティクルバッファ
	GpuParticleBuffers* gpuParticleBuffers_ = nullptr;

	// パーティクルメッシュ
	std::unique_ptr<ParticleMesh> particleMesh_;

	// パーティクルマテリアル
	std::unique_ptr<ParticleMaterial> particleMaterial_;

	// GPUパーティクルメッシュパイプライン
	std::unique_ptr<GpuParticleMeshPipeline> gpuParticleMeshPipeline_;

	std::string textureFilePath_ = "Effects/circle2.dds";
};


} // namespace Ken4lowEngine
