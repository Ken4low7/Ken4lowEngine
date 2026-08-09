#pragma once

#include "DX12Include.h"

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// <summary>Shadow用Texture2DArrayをHLSLへ公開するときのSRV種別です。</summary>
	enum class ShadowArrayViewType : uint32_t
	{
		Texture2DArray,
		TextureCube,
	};

	/// <summary>CSMの各Cascade、またはPoint ShadowのCube 6面を保持する深度配列です。</summary>
	class ShadowMapArrayRenderTarget
	{
	public:
		void Initialize(DirectXCommon* dxCommon, uint32_t width, uint32_t height, uint32_t arraySize, ShadowArrayViewType viewType, const wchar_t* debugName);
		void Finalize();
		void Resize(uint32_t width, uint32_t height);

		/// <summary>指定配列SliceをDepth描画先にし、そのSliceだけをクリアします。</summary>
		void BeginSlice(ID3D12GraphicsCommandList* commandList, uint32_t sliceIndex);
		/// <summary>全Slice描画後に配列全体をPixelShaderResourceへ遷移します。</summary>
		void End(ID3D12GraphicsCommandList* commandList);

		uint32_t GetSrvIndex() const { return srvIndex_; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const;
		uint32_t GetArraySize() const { return arraySize_; }
		uint32_t GetWidth() const { return width_; }
		uint32_t GetHeight() const { return height_; }

	private:
		void CreateResource();
		void CreateDescriptors();
		void ReleaseDescriptors();
		void TransitionTo(D3D12_RESOURCE_STATES nextState);
		void UpdateViewport();

		DirectXCommon* dxCommon_ = nullptr;
		ComPtr<ID3D12Resource> resource_;
		std::vector<uint32_t> dsvIndices_;
		uint32_t srvIndex_ = UINT32_MAX;
		uint32_t width_ = 1;
		uint32_t height_ = 1;
		uint32_t arraySize_ = 1;
		ShadowArrayViewType viewType_ = ShadowArrayViewType::Texture2DArray;
		const wchar_t* debugName_ = L"ShadowMapArrayRenderTarget";
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissorRect_{};
		D3D12_RESOURCE_STATES resourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	};
}
