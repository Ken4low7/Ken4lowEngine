#pragma once
#include "IPostEffect.h"

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///					アブソーブエフェクトクラス
/// -------------------------------------------------------------
class AbsorbEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	/// ---------- 吸収エフェクトの設定構造体 ---------- ///
	struct AbsorbSetting
	{
		float time;		  // 経過時間
		float strength;   // 吸収の強さ
		float padding[2]; // アライメント調整
	};

	/// ---------- アブソーブのパラメータ構造体 ---------- ///
	struct AbsorbParams
	{
		float timestepPerFrame = 1.0f / 12.0f; // フレームごとの時間ステップ
		float loopDuration = 1.0f;			   // ループの持続時間
		float strengthMin = 0.0f;			   // 吸収の最小強度
		float strengthMax = 5.0f;			   // 吸収の最大強度
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
	/// 基底クラスの仮想関数をオーバーライドして、オブジェクトやコンポーネントの更新処理を行います。
	/// </summary>
	void Update() override;

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

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// 吸収の設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

	// 吸収設定のマッピング先
	AbsorbSetting* absorbSetting_ = nullptr;

	// アブソーブのパラメータ
	AbsorbParams absorbParams_ = {};
};


} // namespace Ken4lowEngine
