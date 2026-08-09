#pragma once
#include "IPostEffect.h"
#include "Vector4.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　グレイスケールエフェクトクラス
/// -------------------------------------------------------------
class GrayScaleEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// グレイスケールエフェクトの設定
	struct GrayScaleSetting
	{
		Vector4 color; // 色 : グレイスケール変換に使用する色
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

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// grayScaleSetting_ の color メンバーへの const 参照を返します。
	/// </summary>
	/// <returns>設定内の色を示す const Vector4&。</returns>
	const Vector4& GetColor() const { return grayScaleSetting_->color; }

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// グレースケール設定の色を設定します。
	/// </summary>
	/// <param name="color">設定する色。Vector4 型の const 参照で渡されます。</param>
	void SetColor(const Vector4& color) { grayScaleSetting_->color = color; }

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// コンピュートパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

	// コンピュートルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

	// グレイスケールエフェクトの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GrayScaleSetting* grayScaleSetting_ = nullptr;
};


} // namespace Ken4lowEngine
