#include "DSVManager.h"
#include "DirectXCommon.h"

namespace Ken4lowEngine
{

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


	/// -------------------------------------------------------------
	///				　シングルトンインスタンスの取得
	/// -------------------------------------------------------------
	DSVManager* DSVManager::GetInstance()
	{
		static DSVManager instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///					　DSVマネージャの初期化
	/// -------------------------------------------------------------
	void DSVManager::Initialize(DirectXCommon* dxCommon, uint32_t maxDSVCount)
	{
		dxCommon_ = dxCommon;
		maxDSVCount_ = maxDSVCount;

		// DSV用のデスクリプタヒープを作成
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heapDesc.NumDescriptors = maxDSVCount_;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		HRESULT result = dxCommon_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
		if (FAILED(result))
		{
			throw std::runtime_error("Failed to create DSV descriptor heap");
		}

		// DSVのデスクリプタサイズを取得
		descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		descriptorHeap_->SetName(L"DSV Descriptor Heap");
	}

	void DSVManager::Finalize()
	{
		descriptorHeap_.Reset();

		dxCommon_ = nullptr;
		descriptorSize_ = 0;
		maxDSVCount_ = kDefaultMaxDSVCount_;
		useIndex_ = 0;

		std::lock_guard<std::mutex> lock(allocationMutex_);
		std::queue<uint32_t> empty;
		std::swap(freeIndices_, empty);
	}

	ComPtr<ID3D12Resource> DSVManager::CreateDepthStencilBuffer(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_CLEAR_VALUE& outClearValue)
	{
		D3D12_RESOURCE_DESC depthDesc{};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;
		depthDesc.Format = format;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		outClearValue.Format = format;
		outClearValue.DepthStencil.Depth = 1.0f;
		outClearValue.DepthStencil.Stencil = 0;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

		ComPtr<ID3D12Resource> depthBuffer;
		HRESULT result = S_OK;
		result = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &outClearValue, IID_PPV_ARGS(&depthBuffer));
		assert(SUCCEEDED(result) && "Failed to create Depth Stencil Buffer!");

		return depthBuffer;
	}

	ComPtr<ID3D12Resource> DSVManager::CreateShadowMapResource(uint32_t width, uint32_t height)
	{
		D3D12_RESOURCE_DESC depthDesc{};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;

		// SRV でも読むのでTYPELESS
		depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

		ComPtr<ID3D12Resource> depthBuffer;
		HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&depthBuffer));
		assert(SUCCEEDED(hr) && "Failed to create Shadow Map Resource!");

		return depthBuffer;
	}


	/// -------------------------------------------------------------
	///				DSVを確保（未使用のインデックスを取得）
	/// -------------------------------------------------------------
	uint32_t DSVManager::Allocate()
	{
		std::lock_guard<std::mutex> lock(allocationMutex_);

		// 解放済みのインデックスがある場合は再利用
		if (!freeIndices_.empty())
		{
			uint32_t index = freeIndices_.front();
			freeIndices_.pop();
			return index;
		}

		// 新しく確保するインデックス
		if (useIndex_ >= maxDSVCount_)
		{
			throw std::runtime_error("No more DSV descriptors can be allocated");
		}

		return useIndex_++;
	}


	/// -------------------------------------------------------------
	///			 DSVを解放（再利用可能なインデックスを保存）
	/// -------------------------------------------------------------
	void DSVManager::Free(uint32_t dsvIndex)
	{
		std::lock_guard<std::mutex> lock(allocationMutex_);

		// インデックスの範囲チェック
		if (dsvIndex >= maxDSVCount_)
		{
			throw std::runtime_error("Invalid DSV index for freeing");
		}

		// 解放済みインデックスとして保存（再利用可能にする）
		freeIndices_.push(dsvIndex);
	}


	/// -------------------------------------------------------------
	///		指定リソースのDSVを作成（デプスバッファ用）
	/// -------------------------------------------------------------
	void DSVManager::CreateDSVForDepthBuffer(uint32_t dsvIndex, ID3D12Resource* depthResource)
	{
		assert(depthResource && "Depth buffer resource is null!");

		// DSVの設定
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2D.MipSlice = 0;

		// DSVを作成
		dxCommon_->GetDevice()->CreateDepthStencilView(depthResource, &dsvDesc, GetCPUDescriptorHandle(dsvIndex));
	}

	void DSVManager::CreateDSVForTexture2D(uint32_t dsvIndex, ID3D12Resource* resource)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2D.MipSlice = 0;

		dxCommon_->GetDevice()->CreateDepthStencilView(resource, &dsvDesc, GetCPUDescriptorHandle(dsvIndex));
	}

	void DSVManager::CreateDSVForShadowMap(uint32_t dsvIndex, ID3D12Resource* resource)
	{
		assert(resource && "ShadowMap resource is null!");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2D.MipSlice = 0;

		dxCommon_->GetDevice()->CreateDepthStencilView(resource, &dsvDesc, GetCPUDescriptorHandle(dsvIndex));
	}

	void DSVManager::CreateDSVForShadowMapArraySlice(uint32_t dsvIndex, ID3D12Resource* resource, uint32_t arraySlice)
	{
		assert(resource && "ShadowMap array resource is null!");
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
		dsvDesc.Texture2DArray.ArraySize = 1;
		dxCommon_->GetDevice()->CreateDepthStencilView(resource, &dsvDesc, GetCPUDescriptorHandle(dsvIndex));
	}


	/// -------------------------------------------------------------
	///			指定インデックスのCPUデスクリプタハンドルを取得
	/// -------------------------------------------------------------
	D3D12_CPU_DESCRIPTOR_HANDLE DSVManager::GetCPUDescriptorHandle(uint32_t index)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += descriptorSize_ * index;
		return handle;
	}

} // namespace Ken4lowEngine
