#include "ShadowMapRenderTarget.h"
#include "DirectXCommon.h"
#include "DSVManager.h"
#include "SRVManager.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///							初期化
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::Initialize(DirectXCommon* dxCommon, const ShadowMapSettings& settings)
	{
		dxCommon_ = dxCommon;
		settings_ = settings;

		CreateResource();
		CreateDescriptors();
		UpdateViewports();
	}

	/// -------------------------------------------------------------
	///							終了処理
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::Finalize()
	{
		ReleaseDescriptors();
		shadowMapResource_.Reset();
		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///				シャドウマップリソース生成
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::CreateResource()
	{
		shadowMapResource_ = DSVManager::GetInstance()->CreateShadowMapResource(
			settings_.width,
			settings_.height
		);
		// ShadowMapRenderTargetにも名前を付け、深度RTのDebugLayer出力を特定しやすくする。
		shadowMapResource_->SetName(L"ShadowMapRenderTarget");

		// 新規生成直後は depth write 状態
		resourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	/// -------------------------------------------------------------
	///					DSV / SRV の作成
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::CreateDescriptors()
	{
		auto* dsvManager = DSVManager::GetInstance();
		auto* srvManager = SRVManager::GetInstance();

		if (!hasCreatedDSV_)
		{
			shadowMapDsvIndex_ = dsvManager->Allocate();
			dsvManager->CreateDSVForShadowMap(shadowMapDsvIndex_, shadowMapResource_.Get());
			hasCreatedDSV_ = true;
		}
		else
		{
			dsvManager->CreateDSVForShadowMap(shadowMapDsvIndex_, shadowMapResource_.Get());
		}

		if (!hasCreatedSRV_)
		{
			shadowMapSrvIndex_ = srvManager->Allocate();
			srvManager->CreateSRVForShadowMap(shadowMapSrvIndex_, shadowMapResource_.Get());
			hasCreatedSRV_ = true;
		}
		else
		{
			srvManager->CreateSRVForShadowMap(shadowMapSrvIndex_, shadowMapResource_.Get());
		}
	}

	/// -------------------------------------------------------------
	///					DSV / SRV の解放
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::ReleaseDescriptors()
	{
		auto* dsvManager = DSVManager::GetInstance();
		auto* srvManager = SRVManager::GetInstance();

		if (hasCreatedDSV_)
		{
			dsvManager->Free(shadowMapDsvIndex_);
			shadowMapDsvIndex_ = UINT32_MAX;
			hasCreatedDSV_ = false;
		}

		if (hasCreatedSRV_)
		{
			srvManager->Free(shadowMapSrvIndex_);
			shadowMapSrvIndex_ = UINT32_MAX;
			hasCreatedSRV_ = false;
		}
	}

	/// -------------------------------------------------------------
	///				viewport / scissor 更新
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::UpdateViewports()
	{
		viewport_.TopLeftX = 0.0f;
		viewport_.TopLeftY = 0.0f;
		viewport_.Width = static_cast<float>(settings_.width);
		viewport_.Height = static_cast<float>(settings_.height);
		viewport_.MinDepth = 0.0f;
		viewport_.MaxDepth = 1.0f;

		scissorRect_.left = 0;
		scissorRect_.top = 0;
		scissorRect_.right = static_cast<LONG>(settings_.width);
		scissorRect_.bottom = static_cast<LONG>(settings_.height);
	}

	/// -------------------------------------------------------------
	///					シャドウパス開始
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::Begin(ID3D12GraphicsCommandList* commandList)
	{
		assert(dxCommon_ != nullptr);
		assert(commandList != nullptr);
		assert(shadowMapResource_ != nullptr);
		assert(shadowMapDsvIndex_ != UINT32_MAX);

		if (resourceState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			// ShadowMapを描画先として使う直前にDepthWriteへ戻し、SRV状態のまま描かない。
			dxCommon_->ResourceTransition(
				shadowMapResource_.Get(),
				resourceState_,
				D3D12_RESOURCE_STATE_DEPTH_WRITE
			);
			resourceState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		commandList->RSSetViewports(1, &viewport_);
		commandList->RSSetScissorRects(1, &scissorRect_);

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
			DSVManager::GetInstance()->GetCPUDescriptorHandle(shadowMapDsvIndex_);

		commandList->OMSetRenderTargets(0, nullptr, false, &dsvHandle);
		commandList->ClearDepthStencilView(
			dsvHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			settings_.clearDepth,
			0,
			0,
			nullptr
		);
	}

	/// -------------------------------------------------------------
	///					シャドウパス終了
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::End(ID3D12GraphicsCommandList* commandList)
	{
		(void)commandList;

		assert(dxCommon_ != nullptr);
		assert(shadowMapResource_ != nullptr);

		if (resourceState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		{
			// ShadowMapを通常描画で読む前にPixelShaderResourceへ遷移する。
			dxCommon_->ResourceTransition(
				shadowMapResource_.Get(),
				resourceState_,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
			resourceState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
	}

	/// -------------------------------------------------------------
	///						サイズ変更
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::Resize(uint32_t width, uint32_t height)
	{
		settings_.width = width;
		settings_.height = height;

		shadowMapResource_.Reset();

		CreateResource();
		CreateDescriptors();
		UpdateViewports();
	}

	/// -------------------------------------------------------------
	///						設定変更
	/// -------------------------------------------------------------
	void ShadowMapRenderTarget::SetSettings(const ShadowMapSettings& settings)
	{
		settings_ = settings;
		Resize(settings_.width, settings_.height);
	}

	/// -------------------------------------------------------------
	///				GPU SRV ハンドル取得
	/// -------------------------------------------------------------
	D3D12_GPU_DESCRIPTOR_HANDLE ShadowMapRenderTarget::GetSrvHandleGPU() const
	{
		return SRVManager::GetInstance()->GetGPUDescriptorHandle(shadowMapSrvIndex_);
	}

	/// -------------------------------------------------------------
	///				CPU DSV ハンドル取得
	/// -------------------------------------------------------------
	D3D12_CPU_DESCRIPTOR_HANDLE ShadowMapRenderTarget::GetDsvHandleCPU() const
	{
		return DSVManager::GetInstance()->GetCPUDescriptorHandle(shadowMapDsvIndex_);
	}

}