#pragma once
#include "DX12Include.h"
#include <wrl.h>
#include <cstdint>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// -------------------------------------------------------------
	///		シャドウマップ生成に使う設定値
	/// -------------------------------------------------------------
	struct ShadowMapSettings
	{
		uint32_t width = 2048;
		uint32_t height = 2048;

		// 今後拡張用
		DXGI_FORMAT resourceFormat = DXGI_FORMAT_R32_TYPELESS;
		DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT;
		DXGI_FORMAT srvFormat = DXGI_FORMAT_R32_FLOAT;

		float clearDepth = 1.0f;
	};

	/// -------------------------------------------------------------
	///		シャドウマップ用レンダーターゲット管理クラス
	/// -------------------------------------------------------------
	class ShadowMapRenderTarget
	{
	public:
		void Initialize(DirectXCommon* dxCommon, const ShadowMapSettings& settings);
		void Finalize();

		void Begin(ID3D12GraphicsCommandList* commandList);
		void End(ID3D12GraphicsCommandList* commandList);

		void Resize(uint32_t width, uint32_t height);
		void SetSettings(const ShadowMapSettings& settings);

	public: /// ---------- Getter ---------- ///

		ID3D12Resource* GetResource() const { return shadowMapResource_.Get(); }
		uint32_t GetSrvIndex() const { return shadowMapSrvIndex_; }
		uint32_t GetDsvIndex() const { return shadowMapDsvIndex_; }

		D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandleCPU() const;

		const D3D12_VIEWPORT& GetViewport() const { return viewport_; }
		const D3D12_RECT& GetScissorRect() const { return scissorRect_; }

		uint32_t GetWidth() const { return settings_.width; }
		uint32_t GetHeight() const { return settings_.height; }

	private:
		void CreateResource();
		void CreateDescriptors();
		void ReleaseDescriptors();
		void UpdateViewports();

	private:
		// 借り物参照
		DirectXCommon* dxCommon_ = nullptr;

		// 設定値
		ShadowMapSettings settings_{};

		// シャドウマップ用深度テクスチャ
		Microsoft::WRL::ComPtr<ID3D12Resource> shadowMapResource_;

		// DSV / SRV のヒープ上インデックス
		uint32_t shadowMapDsvIndex_ = UINT32_MAX;
		uint32_t shadowMapSrvIndex_ = UINT32_MAX;

		// 二重確保防止用
		bool hasCreatedDSV_ = false;
		bool hasCreatedSRV_ = false;

		// viewport / scissor
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissorRect_{};

		// 現在のリソースステート
		D3D12_RESOURCE_STATES resourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	};
}