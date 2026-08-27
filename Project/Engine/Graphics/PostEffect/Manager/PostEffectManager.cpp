#include "PostEffectManager.h"

#include "PostEffectPipelineBuilder.h"
#include "PostEffectRegistry.h"
#include "PostEffectChain.h"
#include "PostEffectRuntimeState.h"
#include "PostEffectRenderTargetManager.h"
#include "PostEffectExecutor.h"
#include "PostEffectEditorPanel.h"
#include "DirectXCommon.h"
#include "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"
#include "Engine/Graphics/Renderer/GpuFluid/Sph/Renderer/GpuSphScreenSpaceFluidRenderer.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

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

		dxCommon_->GetCommandManager()->ExecuteAndWait();
		GpuSphScreenSpaceFluidRenderer::GetInstance()->Finalize(); // 専用RT/SRV/RTVをDescriptor Manager破棄前に返す。
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

	void PostEffectManager::BeginDraw()
	{
		if (executor_) { executor_->BeginDraw(); }
	}

	void PostEffectManager::EndDraw()
	{
		// SceneのOpaque/Transparent/Additiveがすべて終わった後、PostEffectへ渡す直前にSPHをScreen Space合成する。
		GpuSphManager* sphManager = GpuSphManager::GetInstance();
		if (sphManager && sphManager->IsInitialized())
		{
			GpuSphScreenSpaceFluidRenderer::GetInstance()->Draw(sphManager->GetParticleBuffer());
		}
		if (executor_) { executor_->EndDraw(); }
	}

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

#ifdef USE_IMGUI
		GpuSphScreenSpaceFluidRenderer* sphRenderer = GpuSphScreenSpaceFluidRenderer::GetInstance();
		GpuSphScreenSpaceRenderSettings& settings = sphRenderer->GetEditableSettings();
		const GpuSphScreenSpaceRenderStats& stats = sphRenderer->GetStats();
		if (ImGui::Begin("スクリーンスペース流体"))
		{
			ImGui::Checkbox("描画を有効化", &settings.enabled);
			ImGui::DragFloat("粒子描画半径", &settings.particleRadius, 0.001f, 0.005f, 0.5f, "%.3f");
			ImGui::DragFloat("深度ぼかし境界", &settings.blurDepthFalloff, 0.25f, 0.1f, 100.0f, "%.2f");
			ImGui::DragFloat("光の吸収", &settings.absorption, 0.05f, 0.0f, 20.0f, "%.2f");
			ImGui::DragFloat("屈折の強さ", &settings.refractionStrength, 0.0005f, 0.0f, 0.1f, "%.4f");
			ImGui::DragFloat("フレネル指数", &settings.fresnelPower, 0.05f, 1.0f, 12.0f, "%.2f");
			ImGui::DragFloat("厚み倍率", &settings.thicknessScale, 0.05f, 0.0f, 20.0f, "%.2f");
			ImGui::ColorEdit4("浅い部分の色", &settings.shallowColor.x);
			ImGui::ColorEdit4("深い部分の色", &settings.deepColor.x);

			if (ImGui::Button("水向け描画設定を適用"))
			{
				settings.particleRadius = 0.12f;
				settings.blurDepthFalloff = 12.0f;
				settings.absorption = 2.8f;
				settings.refractionStrength = 0.012f;
				settings.fresnelPower = 5.0f;
				settings.thicknessScale = 5.0f; // 粒感を抑えて連続した水面を確認しやすい基準値へ戻す。
			}

			ImGui::SeparatorText("描画状況");
			ImGui::Text("Renderer: %s", stats.initialized ? "準備完了" : "待機中");
			ImGui::Text("直前の描画: %s", stats.lastDrawSucceeded ? "成功" : "失敗");
			ImGui::Text("粒子数: %u | 解像度: %u x %u", stats.lastParticleCount, stats.width, stats.height);
			ImGui::Text("深度描画: %llu | 厚み描画: %llu",
				static_cast<unsigned long long>(stats.particleDepthDrawCount),
				static_cast<unsigned long long>(stats.thicknessDrawCount));
			ImGui::Text("ぼかし描画: %llu | 合成描画: %llu",
				static_cast<unsigned long long>(stats.blurDrawCount),
				static_cast<unsigned long long>(stats.compositeDrawCount));
			ImGui::TextDisabled("SPH粒子数は描画解像度だけでなく、流体そのものの量にも影響します。");
			ImGui::TextDisabled("水らしい境界挙動では、SPHの境界減衰を0.05付近から調整してください。");
		}
		ImGui::End();
#endif // USE_IMGUI
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
		Resize(width, height);
	}

	ID3D12Resource* PostEffectManager::GetGameRenderTargetResource() const
	{
		return renderTargetManager_ ? renderTargetManager_->GetGameRenderTarget().resource.Get() : nullptr;
	}

	D3D12_RESOURCE_STATES PostEffectManager::GetGameRenderTargetState() const
	{
		return renderTargetManager_ ? renderTargetManager_->GetGameRenderTarget().currentState : D3D12_RESOURCE_STATE_COMMON;
	}

	void PostEffectManager::SetGameRenderTargetState(D3D12_RESOURCE_STATES state)
	{
		if (renderTargetManager_) { renderTargetManager_->GetGameRenderTarget().currentState = state; }
	}

	D3D12_CPU_DESCRIPTOR_HANDLE PostEffectManager::GetSceneDsvHandle() const
	{
		return renderTargetManager_ ? renderTargetManager_->GetDsvHandle() : D3D12_CPU_DESCRIPTOR_HANDLE{};
	}
}
