#pragma once
#include "IPostEffect.h"
#include <Vector4.h>

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///					ディゾルブエフェクトクラス
/// -------------------------------------------------------------
class DissolveEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// ディゾルブの設定
	struct DissolveSetting
	{
		float threshold;        // 閾値
		float edgeThickness;    // エッジの太さ
		float padding0[2];      // パディング
		Vector4 edgeColor;      // 色
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// DirectX 共通リソースとポストエフェクトパイプラインのビルダーを使用して初期化します。基底クラスの仮想関数をオーバーライドします。
	/// </summary>
	/// <param name="dxCommon">DirectX の共通リソース（デバイス、コンテキスト、スワップチェーンなど）へのポインタ。レンダリングに必要なリソースにアクセスします。</param>
	/// <param name="builder">ポストエフェクトパイプラインを構築・登録するためのビルダーへのポインタ。エフェクトやパスの設定を行います。</param>
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

	/// <summary>
	/// ポストエフェクトの終了処理を行います。基底クラスの仮想関数をオーバーライドします。
	/// </summary>
	void Finalize() override;

	/// <summary>
	/// 指定したコマンドリストに対して、SRV（シェーダリソースビュー）、UAV（アンオーダードアクセ スビュー）、DSV（深度ステンシルビュー）のインデックスに基づく設定を適用します。
	/// </summary>
	/// <param name="commandList">設定を適用する対象の ID3D12GraphicsCommandList へのポインタ。描画コマンドの記録に使用されます。</param>
	/// <param name="srvIndex">適用するシェーダリソースビュー (SRV) のインデックス。</param>
	/// <param name="uavIndex">適用するアンオーダードアクセスビュー (UAV) のインデックス。</param>
	/// <param name="dsvIndex">適用する深度ステンシルビュー (DSV) のインデックス。</param>
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;

	/// <summary>
	/// ImGui を用いて UI の描画処理を行う。基底クラスの同名メソッドをオーバーライドする。
	/// </summary>
	void DrawImGui() override;

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// コンピュートパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

	// コンピュートルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

	// ディゾルブの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	DissolveSetting* dissolveSetting_ = nullptr;

	uint32_t dissolveMaskSrvIndexOnUAV_ = UINT32_MAX;   // UAVヒープ側に複製したSRV
};


} // namespace Ken4lowEngine
