#include "PostEffectManager.h"

#include "PostEffectPipelineBuilder.h"
#include "PostEffectRegistry.h"
#include "PostEffectChain.h"
#include "PostEffectRuntimeState.h"
#include "PostEffectRenderTargetManager.h"
#include "PostEffectExecutor.h"
#include "PostEffectEditorPanel.h"
#include "DirectXCommon.h"

namespace Ken4lowEngine
{
	PostEffectManager::PostEffectManager() = default;
	PostEffectManager::~PostEffectManager() = default;

	PostEffectManager* PostEffectManager::GetInstance()
	{
		static PostEffectManager instance;
		return &instance;
	}

	void PostEffectManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;
		pipelineBuilder_ = std::make_unique<PostEffectPipelineBuilder>();
		pipelineBuilder_->Initialize(dxCommon_);
		pipelineBuilder_->BuildCopyPipeline();

		// Effect初期化中にも既存GetGameRenderTargetWidth/Heightが使えるよう、RTを先に生成する。
		renderTargetManager_ = std::make_unique<PostEffectRenderTargetManager>();
		renderTargetManager_->Initialize(dxCommon_);

		registry_ = std::make_unique<PostEffectRegistry>();
		registry_->Initialize(dxCommon_, pipelineBuilder_.get());
		chain_ = std::make_unique<PostEffectChain>();
		runtimeState_ = std::make_unique<PostEffectRuntimeState>();
		for (const PostEffectDefinition& definition : registry_->GetDefinitions())
		{
			chain_->RegisterEffect(definition.name, definition.order);
			runtimeState_->RegisterEffect(definition.name, definition.editorEnabledByDefault);
		}

		executor_ = std::make_unique<PostEffectExecutor>();
		executor_->Initialize(
			dxCommon_, pipelineBuilder_.get(), registry_.get(), chain_.get(),
			runtimeState_.get(), renderTargetManager_.get());
		editorPanel_ = std::make_unique<PostEffectEditorPanel>();
	}

	void PostEffectManager::Finalize()
	{
		if (!dxCommon_)
		{
			return;
		}

		dxCommon_->GetCommandManager()->ExecuteAndWait(); // GPU参照完了後にEffectとRTを破棄する既存順を維持する。
		if (executor_) { executor_->Finalize(); }
		if (registry_) { registry_->Finalize(); }
		if (pipelineBuilder_) { pipelineBuilder_->Finalize(); }
		if (chain_) { chain_->Clear(); }
		if (runtimeState_) { runtimeState_->Clear(); }
		if (renderTargetManager_) { renderTargetManager_->Finalize(); }

		editorPanel_.reset();
		executor_.reset();
		registry_.reset();
		chain_.reset();
		runtimeState_.reset();
		renderTargetManager_.reset();
		pipelineBuilder_.reset();
		dxCommon_ = nullptr;
	}

	void PostEffectManager::Update()
	{
		if (registry_ && runtimeState_)
		{
			registry_->UpdateEditorEnabledEffects(*runtimeState_);
		}
	}

	void PostEffectManager::BeginDraw() { if (executor_) { executor_->BeginDraw(); } }
	void PostEffectManager::EndDraw() { if (executor_) { executor_->EndDraw(); } }
	void PostEffectManager::Resize(uint32_t width, uint32_t height) { if (renderTargetManager_) { renderTargetManager_->Resize(width, height); } }
	void PostEffectManager::RenderPostEffect() { if (executor_) { executor_->RenderPostEffect(); } }
	void PostEffectManager::RenderPostEffectToBackBuffer() { if (executor_) { executor_->RenderPostEffectToBackBuffer(); } }
	void PostEffectManager::BeginGameRenderTargetOverlay() { if (executor_) { executor_->BeginGameRenderTargetOverlay(); } }
	void PostEffectManager::EndGameRenderTargetOverlay() { if (executor_) { executor_->EndGameRenderTargetOverlay(); } }
	void PostEffectManager::BindSceneRenderTarget() { if (executor_) { executor_->BindSceneRenderTarget(); } }

	void PostEffectManager::ImGuiRender(bool* pOpen)
	{
		if (editorPanel_ && registry_ && runtimeState_)
		{
			editorPanel_->Draw(*registry_, *runtimeState_, pOpen);
		}
	}

	void PostEffectManager::EnableEffect(const std::string& effectName)
	{
		if (runtimeState_) { runtimeState_->SetRuntimeEnabled(effectName, true); }
	}

	void PostEffectManager::DisableEffect(const std::string& effectName)
	{
		if (runtimeState_) { runtimeState_->SetRuntimeEnabled(effectName, false); }
	}

	IPostEffect* PostEffectManager::GetEffect(const std::string& effectName)
	{
		return registry_ ? registry_->Find(effectName) : nullptr;
	}

	uint32_t PostEffectManager::GetGameRenderTargetSrvIndex() const
	{
		return renderTargetManager_ ? renderTargetManager_->GetGameRenderTargetSrvIndex() : UINT32_MAX;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE PostEffectManager::GetGameRenderTargetSrvHandleGPU() const
	{
		return renderTargetManager_ ? renderTargetManager_->GetGameRenderTargetSrvHandleGPU() : D3D12_GPU_DESCRIPTOR_HANDLE{};
	}

	uint32_t PostEffectManager::GetGameRenderTargetWidth() const
	{
		return renderTargetManager_ ? renderTargetManager_->GetWidth() : PostEffectRenderTargetManager::kDefaultWidth;
	}

	uint32_t PostEffectManager::GetGameRenderTargetHeight() const
	{
		return renderTargetManager_ ? renderTargetManager_->GetHeight() : PostEffectRenderTargetManager::kDefaultHeight;
	}

	void PostEffectManager::RequestGameRenderTargetResize(uint32_t width, uint32_t height)
	{
		Resize(width, height); // RenderTargetManager側で固定内部解像度へ丸める既存仕様を維持する。
	}
}
