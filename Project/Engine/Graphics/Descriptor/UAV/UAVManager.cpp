#include "UAVManager.h"
#include "DirectXCommon.h"

namespace Ken4lowEngine
{

UAVManager* UAVManager::GetInstance()
{
	static UAVManager instance;
	return &instance;
}

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void UAVManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	// デスクリプタヒープの設定
	descriptorHeapDesc_.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc_.NumDescriptors = kMaxUAVCount;
	descriptorHeapDesc_.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc_.NodeMask = 0;
	// デスクリプタヒープの作成
	HRESULT result = dxCommon_->GetDevice()->CreateDescriptorHeap(&descriptorHeapDesc_, IID_PPV_ARGS(&descriptorHeap_));
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create UAV descriptor heap");
	}
	// デスクリプタサイズを取得
	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	descriptorHeap_->SetName(L"UAV Descriptor Heap");
}

/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void UAVManager::Finalize()
{
	std::lock_guard<std::mutex> lock(allocationMutex_);

	// 所有しているCOM(ヒープ)を解放
	descriptorHeap_.Reset();

	// 管理状態を初期化
	descriptorSize_ = 0;
	useIndex_ = 0;
	while (!freeIndices_.empty()) { freeIndices_.pop(); }

	// 借り物参照を切る
	dxCommon_ = nullptr;
}

/// -------------------------------------------------------------
///						ディスパッチ前処理
/// -------------------------------------------------------------
void UAVManager::PreDispatch()
{
	ID3D12DescriptorHeap* heaps[] = { descriptorHeap_.Get() };
	dxCommon_->GetCommandManager()->GetCommandList()->SetDescriptorHeaps(_countof(heaps), heaps);
}

/// -------------------------------------------------------------
///						UAV作成(Texture2D用)
/// -------------------------------------------------------------
void UAVManager::CreateUAVForTexture2D(uint32_t uavIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	if (!pResource) {
		throw std::runtime_error("pResource is null in CreateUAVForTexture2D");
	}
	if (uavIndex >= kMaxUAVCount) {
		throw std::runtime_error("uavIndex out of bounds in CreateUAVForTexture2D");
	}

	// UAV 設定（Texture2D 用）
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = Format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = MipLevels;

	// UAV 作成
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(uavIndex));
}

/// -------------------------------------------------------------
///			UAVヒープ上にSRVを作成(Texture2D用)
/// -------------------------------------------------------------
void UAVManager::CreateSRVForTexture2DOnThisHeap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	if (!pResource) { throw std::runtime_error("pResource is null in CreateSRVForTexture2DOnThisHeap"); }
	D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
	srv.Format = Format;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv.Texture2D.MipLevels = MipLevels;
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srv, GetCPUDescriptorHandle(srvIndex));
}

/// -------------------------------------------------------------
///						UAV作成(Buffer用)
/// -------------------------------------------------------------
void UAVManager::CreateUAVForBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT64 bufferSize)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;  // Rawバッファ用
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = static_cast<UINT>(bufferSize / 4); // 4byte単位
	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(uavIndex));
}

/// -------------------------------------------------------------
///				UAV作成(Structure Buffer用)
/// -------------------------------------------------------------
void UAVManager::CreateUAVForStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファの場合、フォーマットは特に指定しない
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.CounterOffsetInBytes = 0; // カウンターオフセットは使用しない
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE; // 特にフラグは指定しない
	uavDesc.Buffer.StructureByteStride = structureByteStride;
	dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(uavIndex));
}

/// -------------------------------------------------------------
///							容量の確保
/// -------------------------------------------------------------
uint32_t UAVManager::Allocate()
{
	// 排他制御のためのロックを取得（スレッドセーフにするため）
	std::lock_guard<std::mutex> lock(allocationMutex_);

	// 空きリストに要素がある場合は、それを利用する
	if (!freeIndices_.empty()) {
		uint32_t index = freeIndices_.front(); // 空きリストの先頭からインデックスを取得
		freeIndices_.pop(); // 空きリストから削除
		return index; // 空いているインデックスを返す
	}

	// 空きリストが空の場合、次に使用可能なインデックスを確認
	if (useIndex_ >= kMaxUAVCount)
	{
		throw std::runtime_error("No more SRV descriptors can be allocated"); // 確保可能な最大数を超えた場合は例外を投げる
	}

	// 空きリストも使用済みインデックスも残っていれば、新しいインデックスを返す
	return useIndex_++; // 次の未使用インデックスを返す（useIndexをインクリメントして更新）
}

/// -------------------------------------------------------------
///							解放処理
/// -------------------------------------------------------------
void UAVManager::Free(uint32_t uavIndex)
{
	// 排他制御のためのロックを取得（スレッドセーフにするため）
	std::lock_guard<std::mutex> lock(allocationMutex_);

	// 解放しようとしているインデックスが有効範囲外（0以上かつ kMaxSRVCount 未満でない）場合はエラーをスロー
	if (uavIndex >= kMaxUAVCount) {
		throw std::runtime_error("Invalid SRV index for freeing"); // 範囲外のインデックスに対する解放操作は無効
	}

	// 解放対象のインデックスを空きリストに追加
	freeIndices_.push(uavIndex); // 再利用可能なインデックスとしてリストに登録
}

/// -------------------------------------------------------------
///					デスクリプタヒープの取得
/// -------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE UAVManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<unsigned long long>(index) * descriptorSize_;
	return handle;
}

/// -------------------------------------------------------------
///					デスクリプタヒープの取得
/// -------------------------------------------------------------
D3D12_GPU_DESCRIPTOR_HANDLE UAVManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<unsigned long long>(index) * descriptorSize_;
	return handle;
}

/// -------------------------------------------------------------
///				SRV作成(Structure Buffer用)
/// -------------------------------------------------------------
void UAVManager::CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	if (!pResource) { throw std::runtime_error("pResource is null in CreateSRVForStructureBuffer"); }

	D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
	srv.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファの場合、フォーマットは特に指定しない
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srv.Buffer.FirstElement = 0;
	srv.Buffer.NumElements = numElements;
	srv.Buffer.StructureByteStride = structureByteStride;
	srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE; // 特にフラグは指定しない
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srv, GetCPUDescriptorHandle(srvIndex));
}
} // namespace Ken4lowEngine
