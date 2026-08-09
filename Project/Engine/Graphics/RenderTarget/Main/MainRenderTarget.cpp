#include "MainRenderTarget.h"
#include "DirectXCommon.h"
#include "RTVManager.h"
#include "DSVManager.h"

#include <cwchar>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///							初期化
	/// -------------------------------------------------------------
	void MainRenderTarget::Initialize(DirectXCommon* dxCommon, const MainRenderTargetSettings& settings)
	{
		// DirectXCommon を保持
		dxCommon_ = dxCommon;

		// 初期設定を保持
		settings_ = settings;

		// 深度バッファ生成
		CreateDepthStencilResource();

		// DSV 作成
		CreateDepthStencilView();

		// バックバッファ用 RTV 作成
		CreateBackBufferRTVs();

		// viewport / scissor 更新
		UpdateViewports();
	}

	/// -------------------------------------------------------------
	///							終了処理
	/// -------------------------------------------------------------
	void MainRenderTarget::Finalize()
	{
		// RTV / DSV の解放
		ReleaseDescriptors();

		// 深度リソース解放
		depthStencilResource_.Reset();

		// 借り物参照を切る
		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///				深度バッファ生成
	/// -------------------------------------------------------------
	void MainRenderTarget::CreateDepthStencilResource()
	{
		D3D12_CLEAR_VALUE clearValue{};
		depthStencilResource_ = DSVManager::GetInstance()->CreateDepthStencilBuffer(
			dxCommon_->GetClientWidth(),
			dxCommon_->GetClientHeight(),
			settings_.depthFormat,
			clearValue
		);
		// MainRenderTargetの深度も名前を付け、DSV関連のDebugLayer出力から識別できるようにする。
		depthStencilResource_->SetName(L"MainRenderTargetDepth");
	}

	/// -------------------------------------------------------------
	///						DSV 作成
	/// -------------------------------------------------------------
	void MainRenderTarget::CreateDepthStencilView()
	{
		auto* dsvManager = DSVManager::GetInstance();

		// 未確保なら index を確保
		if (!hasCreatedDSV_)
		{
			dsvIndex_ = dsvManager->Allocate();
			hasCreatedDSV_ = true;
		}

		// 現在の深度リソースに対して DSV を作成
		dsvManager->CreateDSVForDepthBuffer(dsvIndex_, depthStencilResource_.Get());
	}

	/// -------------------------------------------------------------
	///				バックバッファ RTV 作成
	/// -------------------------------------------------------------
	void MainRenderTarget::CreateBackBufferRTVs()
	{
		auto* rtvManager = RTVManager::GetInstance();

		// swapchain のバッファ数を取得
		const uint32_t bufferCount = dxCommon_->GetBackBufferCount();

		// 初回だけ index を確保
		if (!hasCreatedRTVs_)
		{
			backBufferRtvIndices_.resize(bufferCount);

			for (uint32_t i = 0; i < bufferCount; ++i)
			{
				backBufferRtvIndices_[i] = rtvManager->Allocate();
			}

			hasCreatedRTVs_ = true;
		}

		// 各バックバッファに RTV を張る
		for (uint32_t i = 0; i < bufferCount; ++i)
		{
			auto backBuffer = dxCommon_->GetBackBuffer(i);
			wchar_t name[32]{};
			swprintf_s(name, L"BackBuffer%u", i);
			// MainRenderTarget側で取得したComPtrにもBackBuffer名を再設定し、RTVエラーをUnnamedにしない。
			backBuffer->SetName(name);
			rtvManager->CreateRTVForTexture2D(backBufferRtvIndices_[i], backBuffer.Get());
		}
	}

	/// -------------------------------------------------------------
	///					RTV / DSV 解放
	/// -------------------------------------------------------------
	void MainRenderTarget::ReleaseDescriptors()
	{
		auto* rtvManager = RTVManager::GetInstance();
		auto* dsvManager = DSVManager::GetInstance();

		// RTV 解放
		if (hasCreatedRTVs_)
		{
			for (uint32_t index : backBufferRtvIndices_)
			{
				rtvManager->Free(index);
			}
			backBufferRtvIndices_.clear();
			hasCreatedRTVs_ = false;
		}

		// DSV 解放
		if (hasCreatedDSV_)
		{
			dsvManager->Free(dsvIndex_);
			dsvIndex_ = UINT32_MAX;
			hasCreatedDSV_ = false;
		}
	}

	/// -------------------------------------------------------------
	///				viewport / scissor 更新
	/// -------------------------------------------------------------
	void MainRenderTarget::UpdateViewports()
	{
		const float width = static_cast<float>(dxCommon_->GetClientWidth());
		const float height = static_cast<float>(dxCommon_->GetClientHeight());

		// viewport
		viewport_.TopLeftX = 0.0f;
		viewport_.TopLeftY = 0.0f;
		viewport_.Width = width;
		viewport_.Height = height;
		viewport_.MinDepth = 0.0f;
		viewport_.MaxDepth = 1.0f;

		// scissor
		scissorRect_.left = 0;
		scissorRect_.top = 0;
		scissorRect_.right = static_cast<LONG>(dxCommon_->GetClientWidth());
		scissorRect_.bottom = static_cast<LONG>(dxCommon_->GetClientHeight());
	}

	/// -------------------------------------------------------------
	///						クリア
	/// -------------------------------------------------------------
	void MainRenderTarget::Clear(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex)
	{
		// RTV / DSV ハンドル取得
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetRtvHandleCPU(backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDsvHandleCPU();

		// カラークリア
		commandList->ClearRenderTargetView(
			rtvHandle,
			settings_.clearColor,
			0,
			nullptr
		);

		// 深度クリア
		commandList->ClearDepthStencilView(
			dsvHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			settings_.clearDepth,
			settings_.clearStencil,
			0,
			nullptr
		);
	}

	/// -------------------------------------------------------------
	///					メイン描画開始
	/// -------------------------------------------------------------
	void MainRenderTarget::Begin(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex)
	{
		Bind(commandList, backBufferIndex);

		// 画面クリア
		Clear(commandList, backBufferIndex);
	}

	/// -------------------------------------------------------------
	///				バックバッファ再バインド
	/// -------------------------------------------------------------
	void MainRenderTarget::Bind(ID3D12GraphicsCommandList* commandList, uint32_t backBufferIndex)
	{
		// GPU Particle等がRTVを切り替えた後もHUDをBackBufferへ戻すため、Clearなしで描画先だけ復元する。
		commandList->RSSetViewports(1, &viewport_);
		commandList->RSSetScissorRects(1, &scissorRect_);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetRtvHandleCPU(backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDsvHandleCPU();
		commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
	}

	/// -------------------------------------------------------------
	///					メイン描画終了
	/// -------------------------------------------------------------
	void MainRenderTarget::End(ID3D12GraphicsCommandList* commandList)
	{
		// 今は特に何もしない
		// 後で必要なら barrier や resolve などをここへ寄せる
		(void)commandList;
	}

	/// -------------------------------------------------------------
	///						リサイズ
	/// -------------------------------------------------------------
	void MainRenderTarget::Resize(uint32_t width, uint32_t height)
	{
		(void)width;
		(void)height;

		// 深度リソースを再生成
		depthStencilResource_.Reset();
		CreateDepthStencilResource();

		// DSV を張り直す
		CreateDepthStencilView();

		// swapchain 側で backBuffer が作り直されたあとに RTV を張り直す
		CreateBackBufferRTVs();

		// viewport / scissor 更新
		UpdateViewports();
	}

	/// -------------------------------------------------------------
	///						設定変更
	/// -------------------------------------------------------------
	void MainRenderTarget::SetSettings(const MainRenderTargetSettings& settings)
	{
		// 設定だけ差し替え
		settings_ = settings;

		// クリア色だけなら本来は再生成不要
		// ただし depth format が変わる可能性もあるので安全側で再生成
		Resize(dxCommon_->GetClientWidth(), dxCommon_->GetClientHeight());
	}

	/// -------------------------------------------------------------
	///				DSV CPU ハンドル取得
	/// -------------------------------------------------------------
	D3D12_CPU_DESCRIPTOR_HANDLE MainRenderTarget::GetDsvHandleCPU() const
	{
		return DSVManager::GetInstance()->GetCPUDescriptorHandle(dsvIndex_);
	}

	/// -------------------------------------------------------------
	///				RTV CPU ハンドル取得
	/// -------------------------------------------------------------
	D3D12_CPU_DESCRIPTOR_HANDLE MainRenderTarget::GetRtvHandleCPU(uint32_t backBufferIndex) const
	{
		assert(backBufferIndex < backBufferRtvIndices_.size());
		return RTVManager::GetInstance()->GetCPUDescriptorHandle(backBufferRtvIndices_[backBufferIndex]);

	}

}