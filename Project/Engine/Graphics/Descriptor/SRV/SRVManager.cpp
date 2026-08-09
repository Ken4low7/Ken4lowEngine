#include "SRVManager.h"

#include "DirectXCommon.h"

namespace Ken4lowEngine
{

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


	/// -------------------------------------------------------------
	///				　	シングルトンインスタンス
	/// -------------------------------------------------------------
	SRVManager* SRVManager::GetInstance()
	{
		static SRVManager instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
	void SRVManager::Initialize(DirectXCommon* dxCommon)
	{
		// 引数を受け取ってメンバ変数に代入する
		dxCommon_ = dxCommon;

		// デスクリプタヒープの設定
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = kMaxSRVCount;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;

		// デスクリプタヒープの作成
		HRESULT result = dxCommon_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
		if (FAILED(result))
		{
			throw std::runtime_error("Failed to create SRV descriptor heap");
		}

		// デスクリプタサイズを取得
		descriptorSize = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		descriptorHeap_->SetName(L"SRV Descriptor Heap");
	}

	/// -------------------------------------------------------------
	///					　		終了処理
	/// -------------------------------------------------------------
	void SRVManager::Finalize()
	{
		std::lock_guard<std::mutex> lock(allocationMutex);

		// デスクリプタヒープの解放
		descriptorHeap_.Reset();

		// 管理状態（再初期化できるように戻す）
		descriptorSize = 0;
		useIndex = 1;
		while (!freeIndices.empty()) { freeIndices.pop(); }

		// メンバ変数のクリア
		dxCommon_ = nullptr;
	}


	/// -------------------------------------------------------------
	///						　スプライト用のSRV生成
	/// -------------------------------------------------------------
	void SRVManager::CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
	{
		if (!pResource) {
			throw std::runtime_error("pResource is null in CreateSRVForTexture2D");
		}

		// srvDescの項目を埋める
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = Format; // テクスチャのフォーマット
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // デフォルトのマッピング
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;				//2Dテクスチャ
		srvDesc.Texture2D.MipLevels = MipLevels;

		// Shader Resource View を作成
		dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}


	/// -------------------------------------------------------------
	///					ストラクチャバッファ用のSRV生成
	/// -------------------------------------------------------------
	void SRVManager::CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
	{
		if (!pResource) {
			throw std::runtime_error("pResource is null in CreateSRVForStructureBuffer");
		}
		if (srvIndex >= kMaxSRVCount) {
			throw std::runtime_error("srvIndex out of bounds in CreateSRVForTexture2D");
		}

		// SRV 設定
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファではフォーマットは UNKNOWN
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER; // バッファとして扱う
		srvDesc.Buffer.FirstElement = 0;                   // バッファの先頭要素から開始
		srvDesc.Buffer.NumElements = numElements;          // バッファの要素数
		srvDesc.Buffer.StructureByteStride = structureByteStride; // 各要素のサイズ（バイト単位）
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE; // 特殊なフラグなし

		// Shader Resource View を作成
		dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForShadowMap(uint32_t srvIndex, ID3D12Resource* shadowMap)
	{
		assert(shadowMap && "shadowMap resource is null in CreateSRVForShadowMap");

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // シャドウマップは通常、深度値を格納するために単一チャンネルのフォーマットを使用
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして扱う
		srvDesc.Texture2D.MostDetailedMip = 0; // 最も詳細なミップレベルは0
		srvDesc.Texture2D.MipLevels = 1; // シャドウマップは通常ミップマップを使用しない
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f; // 最小LODクランプは0

		// Shader Resource View を作成
		dxCommon_->GetDevice()->CreateShaderResourceView(shadowMap, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForShadowMapArray(uint32_t srvIndex, ID3D12Resource* shadowMapArray, uint32_t arraySize)
	{
		assert(shadowMapArray && "ShadowMap array resource is null");
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = arraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		dxCommon_->GetDevice()->CreateShaderResourceView(shadowMapArray, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}

	void SRVManager::CreateSRVForShadowCube(uint32_t srvIndex, ID3D12Resource* shadowCube)
	{
		assert(shadowCube && "Shadow cube resource is null");
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		dxCommon_->GetDevice()->CreateShaderResourceView(shadowCube, &srvDesc, GetCPUDescriptorHandle(srvIndex));
	}


	/// -------------------------------------------------------------
	///						ヒープセットコマンド
	/// -------------------------------------------------------------
	void SRVManager::PreDraw()
	{
		// ディスクリプタヒープの設定
		ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
		dxCommon_->GetCommandManager()->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
	}


	/// -------------------------------------------------------------
	///						SRVセットコマンド
	/// -------------------------------------------------------------
	void SRVManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
	{
		dxCommon_->GetCommandManager()->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
	}


	/// -------------------------------------------------------------
	///						深度バッファのSRVを作成
	/// -------------------------------------------------------------
	void SRVManager::CreateSRVForDepthBuffer(uint32_t srvIndex, ID3D12Resource* depthBuffer)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // 24bit 深度
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = GetCPUDescriptorHandle(srvIndex);
		dxCommon_->GetDevice()->CreateShaderResourceView(depthBuffer, &srvDesc, srvHandle);
	}


	/// -------------------------------------------------------------
	///						　		確保
	/// -------------------------------------------------------------
	uint32_t SRVManager::Allocate()
	{
		// 排他制御のためのロックを取得（スレッドセーフにするため）
		std::lock_guard<std::mutex> lock(allocationMutex);

		// 空きリストに要素がある場合は、それを利用する
		if (!freeIndices.empty()) {
			uint32_t index = freeIndices.front(); // 空きリストの先頭からインデックスを取得
			freeIndices.pop(); // 空きリストから削除
			return index; // 空いているインデックスを返す
		}

		// 空きリストが空の場合、次に使用可能なインデックスを確認
		if (useIndex >= kMaxSRVCount)
		{
			throw std::runtime_error("No more SRV descriptors can be allocated"); // 確保可能な最大数を超えた場合は例外を投げる
		}

		// 空きリストも使用済みインデックスも残っていれば、新しいインデックスを返す
		return useIndex++; // 次の未使用インデックスを返す（useIndexをインクリメントして更新）
	}


	/// -------------------------------------------------------------
	///						　	解放処理
	/// -------------------------------------------------------------
	void SRVManager::Free(uint32_t srvIndex)
	{
		// 排他制御のためのロックを取得（スレッドセーフにするため）
		std::lock_guard<std::mutex> lock(allocationMutex);

		// 解放しようとしているインデックスが有効範囲外（0以上かつ kMaxSRVCount 未満でない）場合はエラーをスロー
		if (srvIndex >= kMaxSRVCount) {
			throw std::runtime_error("Invalid SRV index for freeing"); // 範囲外のインデックスに対する解放操作は無効
		}

		// 解放対象のインデックスを空きリストに追加
		freeIndices.push(srvIndex); // 再利用可能なインデックスとしてリストに登録
	}


	/// -------------------------------------------------------------
	///				デスクリプタヒープを生成する
	/// -------------------------------------------------------------
	ComPtr<ID3D12DescriptorHeap> SRVManager::CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible)
	{
		//ディスクリプタヒープの生成
		Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> descriptorHeap;
		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
		descriptorHeapDesc.Type = heapType;	//レンダーターゲットビュー用
		descriptorHeapDesc.NumDescriptors = numDescriptors;						//ダブルバッファ用に2つ。多くても別に構わない
		descriptorHeapDesc.Flags = shadervisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		HRESULT hr = S_FALSE;
		hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
		//ディスクリプタヒープが作れなかったので起動できない
		assert(SUCCEEDED(hr));

		return descriptorHeap;
	}


	/// -------------------------------------------------------------
	///				　CPUデスクリプタヒープを取得する
	/// -------------------------------------------------------------
	D3D12_CPU_DESCRIPTOR_HANDLE SRVManager::GetCPUDescriptorHandle(uint32_t index)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += descriptorSize * index;
		return handle;
	}


	/// -------------------------------------------------------------
	///				　GPUデスクリプタヒープを取得する
	/// -------------------------------------------------------------
	D3D12_GPU_DESCRIPTOR_HANDLE SRVManager::GetGPUDescriptorHandle(uint32_t index)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += descriptorSize * index;
		return handle;
	}

} // namespace Ken4lowEngine
