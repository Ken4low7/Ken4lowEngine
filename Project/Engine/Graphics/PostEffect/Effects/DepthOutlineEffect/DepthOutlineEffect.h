#pragma once
#include <IPostEffect.h>
#include "Vector2.h"
#include "Vector4.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class Camera;

/// -------------------------------------------------------------
///				　  DepthOutlineEffectクラス
/// -------------------------------------------------------------
class DepthOutlineEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// 深度アウトライン設定構造体
	struct DepthOutlineSetting
	{
		Vector2 texelSize;			 // 1 / 解像度
		float depthScale;			 // 深度差
		float edgeThickness;		 // ピクセル単位の太さ
		Vector4 edgeColor;			 // アウトラインの色
		Matrix4x4 projectionInverse; // 逆射影行列
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定した Camera を使用して DepthOutlineEffect を初期化します。
	/// </summary>
	/// <param name="camera">エフェクトを初期化するために使用する Camera オブジェクトへのポインタ。nullptr でないことが期待されます。</param>
	DepthOutlineEffect(Camera* camera);

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

private: /// ---------- 構造体 ---------- ///

	Camera* camera_ = nullptr; // カメラへのポインタ

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// ガウシアンフィルタの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	DepthOutlineSetting* depthOutlineSetting_ = nullptr;
};


} // namespace Ken4lowEngine
