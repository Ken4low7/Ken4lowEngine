#include "RTVManager.h"
#include "DirectXCommon.h"

namespace Ken4lowEngine
{

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


/// -------------------------------------------------------------
///			       シングルトンインスタンスの取得
/// -------------------------------------------------------------
RTVManager* RTVManager::GetInstance()
{
	static RTVManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///			           RTVマネージャの初期化
/// -------------------------------------------------------------
void RTVManager::Initialize(DirectXCommon* dxCommon, uint32_t maxRTVCount)
{
	dxCommon_ = dxCommon;
	maxRTVCount_ = maxRTVCount;

	// RTV用のデスクリプタヒープを作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;   // レンダーターゲットビュー
	heapDesc.NumDescriptors = maxRTVCount_;           // 最大RTV数
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT result = dxCommon_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create RTV descriptor heap");
	}

	// RTVデスクリプタサイズを取得
	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	descriptorHeap_->SetName(L"RTV Descriptor Heap");
}

void RTVManager::Finalize()
{
	// デスクリプタヒープを解放
	descriptorHeap_.Reset();
	// メンバ変数をリセット
	dxCommon_ = nullptr;
	descriptorSize_ = 0;
	maxRTVCount_ = kDefaultMaxRTVCount_;
	useIndex_ = 0;
	// 解放済みインデックスリストをクリア
	std::lock_guard<std::mutex> lock(allocationMutex_);
	std::queue<uint32_t> emptyQueue;
	std::swap(freeIndices_, emptyQueue);
}


/// -------------------------------------------------------------
///			    RTVを確保（未使用のインデックスを取得）
/// -------------------------------------------------------------
uint32_t RTVManager::Allocate()
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
	if (useIndex_ >= maxRTVCount_)
	{
		throw std::runtime_error("No more RTV descriptors can be allocated");
	}

	return useIndex_++;
}


/// -------------------------------------------------------------
///			 RTVを解放（再利用可能なインデックスを保存）
/// -------------------------------------------------------------
void RTVManager::Free(uint32_t rtvIndex)
{
	std::lock_guard<std::mutex> lock(allocationMutex_);

	// インデックスの範囲チェック
	if (rtvIndex >= maxRTVCount_)
	{
		throw std::runtime_error("Invalid RTV index for freeing");
	}

	// 解放済みリストに追加
	freeIndices_.push(rtvIndex);
}


/// -------------------------------------------------------------
///		     指定リソースのRTVを作成（2Dテクスチャ用）
/// -------------------------------------------------------------
void RTVManager::CreateRTVForTexture2D(uint32_t rtvIndex, ID3D12Resource* resource)
{
	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	// RTVを作成
	dxCommon_->GetDevice()->CreateRenderTargetView(resource, &rtvDesc, GetCPUDescriptorHandle(rtvIndex));
}


/// -------------------------------------------------------------
///			指定インデックスのCPUデスクリプタハンドルを取得
/// -------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE RTVManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize_ * index;
	return handle;
}

} // namespace Ken4lowEngine
