#pragma once
#include "IPostEffect.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///						ヴィネットエフェクト
	/// -------------------------------------------------------------
	class VignetteEffect : public IPostEffect
	{
	private: /// ---------- 構造体 ---------- ///

		// ヴィネットの設定
		struct VignetteSetting
		{
			float power = 1.0f; // 強さ
			float range = 0.5f; // 範囲
			float padding[4];   // パディング
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

		// runtime control
		void SetPower(float power);
		void SetRange(float range);
		float GetPower() const;
		float GetRange() const;

	private: /// ---------- メンバ変数 ---------- ///

		// DirectX共通クラス
		DirectXCommon* dxCommon_ = nullptr;

		// コンピュートパイプラインステートオブジェクト
		Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

		// コンピュートルートシグネチャ
		Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

		// ヴィグネットの設定
		Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

		// ヴィグネットの設定データ
		VignetteSetting* vignetteSetting_ = nullptr;

		// ヘッダー側に追加
		VignetteSetting setting_ = { 1.0f, 0.5f };
	};


} // namespace Ken4lowEngine
