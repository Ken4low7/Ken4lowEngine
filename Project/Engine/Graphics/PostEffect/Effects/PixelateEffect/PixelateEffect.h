#pragma once
#include "IPostEffect.h"
#include "Vector2.h"
#include "Vector4.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　		ピクセル化エフェクト
/// -------------------------------------------------------------
class PixelateEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// ピクセル化設定構造体
	struct PixelateSetting
	{
		Vector2 screenSize; // 画面サイズ(ピクセル)
		float   blockSize;  // 1ブロックのサイズ(ピクセル単位)
		float   strength;   // 0=通常, 1=完全ピクセル化
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

public: /// ---------- 設定 ---------- ///

	/// <summary>
	/// ピクセル化エフェクトのブロックサイズを設定します。
	/// </summary>
	/// <param name="blockSize">1 ブロックあたりのピクセル数。大きいほど粗いピクセル化になります。</param>
	void SetBlockSize(float blockSize) { pixelateSetting_->blockSize = blockSize; }

	/// <summary>
	/// ピクセル化エフェクトの強度を設定します。
	/// </summary>
	/// <param name="strength">ピクセル化の強度。0.0f で通常、1.0f で完全なピクセル化。</param>
	void SetStrength(float strength) { pixelateSetting_->strength = strength; }

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	PixelateSetting* pixelateSetting_ = nullptr;
};


} // namespace Ken4lowEngine
