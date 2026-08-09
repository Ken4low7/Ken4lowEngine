#pragma once
#include "DX12Include.h"
#include <wrl.h>
#include <vector>
#include <cstdint>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// -------------------------------------------------------------
	///				メイン描画先の設定
	/// -------------------------------------------------------------
	struct MainRenderTargetSettings
	{
		// 深度バッファのフォーマット
		DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		// クリアカラー
		float clearColor[4] = { 0.1f, 0.25f, 0.5f, 1.0f };

		// 深度クリア値
		float clearDepth = 1.0f;

		// ステンシルクリア値
		uint8_t clearStencil = 0;
	};

	/// -------------------------------------------------------------
	///			メイン画面用レンダーターゲット管理クラス
	/// -------------------------------------------------------------
	/// DirectXCommon に置かれていた
	/// ・バックバッファ RTV
	/// ・深度バッファ
	/// ・DSV
	/// ・viewport / scissor
	/// ・クリア
	/// ・描画開始処理
	/// を分離するためのクラス
	/// -------------------------------------------------------------
	class MainRenderTarget
	{
	public: /// ---------- メイン処理 ---------- ///

		/// ---------------------------------------------------------
		///						初期化
		/// ---------------------------------------------------------
		/// <param name="dxCommon">DirectX 共通管理</param>
		/// <param name="settings">描画先設定</param>
		void Initialize(DirectXCommon* dxCommon, const MainRenderTargetSettings& settings);

		/// ---------------------------------------------------------
		///						終了処理
		/// ---------------------------------------------------------
		void Finalize();

		/// ---------------------------------------------------------
		///					メイン描画開始
		/// ---------------------------------------------------------
		/// 指定バックバッファ index を使って
		/// RTV / DSV / viewport / scissor / clear を設定する
		/// <param name="commandList">コマンドリスト</param>
		/// <param name="backBufferIndex">現在のバックバッファ index</param>
		void Begin(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex);

		/// ---------------------------------------------------------
		///				バックバッファ再バインド
		/// ---------------------------------------------------------
		/// クリアせずに RTV / DSV / viewport / scissor だけを再設定する
		/// <param name="commandList">コマンドリスト</param>
		/// <param name="backBufferIndex">現在のバックバッファ index</param>
		void Bind(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex);

		/// ---------------------------------------------------------
		///					メイン描画終了
		/// ---------------------------------------------------------
		/// 後で必要なら後処理をここへ寄せる
		/// <param name="commandList">コマンドリスト</param>
		void End(ID3D12GraphicsCommandList* commandList);

		/// ---------------------------------------------------------
		///						リサイズ
		/// ---------------------------------------------------------
		/// 深度バッファと viewport/scissor を再生成・更新する
		/// <param name="width">新しい幅</param>
		/// <param name="height">新しい高さ</param>
		void Resize(uint32_t width, uint32_t height);

		/// ---------------------------------------------------------
		///						設定変更
		/// ---------------------------------------------------------
		/// <param name="settings">新しい設定</param>
		void SetSettings(const MainRenderTargetSettings& settings);

	public: /// ---------- Getter ---------- ///

		/// 深度リソース取得
		ID3D12Resource* GetDepthStencilResource() const { return depthStencilResource_.Get(); }

		/// DSV index 取得
		uint32_t GetDsvIndex() const { return dsvIndex_; }

		/// DSV CPU ハンドル取得
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandleCPU() const;

		/// 指定 backBuffer 用 RTV CPU ハンドル取得
		D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandleCPU(uint32_t backBufferIndex) const;

		/// viewport 取得
		const D3D12_VIEWPORT& GetViewport() const { return viewport_; }

		/// scissor 取得
		const D3D12_RECT& GetScissorRect() const { return scissorRect_; }

	private:
		/// ---------------------------------------------------------
		///				深度バッファ生成
		/// ---------------------------------------------------------
		void CreateDepthStencilResource();

		/// ---------------------------------------------------------
		///					DSV 作成
		/// ---------------------------------------------------------
		void CreateDepthStencilView();

		/// ---------------------------------------------------------
		///				バックバッファ RTV 作成
		/// ---------------------------------------------------------
		void CreateBackBufferRTVs();

		/// ---------------------------------------------------------
		///				RTV / DSV 解放
		/// ---------------------------------------------------------
		void ReleaseDescriptors();

		/// ---------------------------------------------------------
		///			viewport / scissor 更新
		/// ---------------------------------------------------------
		void UpdateViewports();

		/// ---------------------------------------------------------
		///					クリア処理
		/// ---------------------------------------------------------
		/// <param name="commandList">コマンドリスト</param>
		/// <param name="backBufferIndex">対象 backBuffer index</param>
		void Clear(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex);

	private:
		// 借り物参照
		DirectXCommon* dxCommon_ = nullptr;

		// 設定
		MainRenderTargetSettings settings_{};

		// メイン深度バッファ
		Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

		// DSV index
		uint32_t dsvIndex_ = UINT32_MAX;

		// back buffer ごとの RTV index
		std::vector<uint32_t> backBufferRtvIndices_;

		// 二重解放防止
		bool hasCreatedDSV_ = false;
		bool hasCreatedRTVs_ = false;

		// viewport / scissor
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissorRect_{};
	};
}