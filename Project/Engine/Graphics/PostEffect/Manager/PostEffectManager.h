#pragma once

#include "DX12Include.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Ken4lowEngine
{
	class DirectXCommon;
	class IPostEffect;
	class PostEffectPipelineBuilder;
	class PostEffectRegistry;
	class PostEffectChain;
	class PostEffectRuntimeState;
	class PostEffectRenderTargetManager;
	class PostEffectExecutor;
	class PostEffectEditorPanel;

	/// <summary>既存PostEffect APIを維持するFacadeです。</summary>
	class PostEffectManager
	{
	public:
		static PostEffectManager* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();
		void Update();

		void BeginDraw();
		void EndDraw();
		void Resize(uint32_t width, uint32_t height);
		void RenderPostEffect();
		void RenderPostEffectToBackBuffer();
		void BeginGameRenderTargetOverlay();
		void EndGameRenderTargetOverlay();
		void BindSceneRenderTarget();

		/// <summary>既存EditorWindowManager互換のPostEffect設定UI入口です。</summary>
		void ImGuiRender(bool* pOpen = nullptr);

		void EnableEffect(const std::string& effectName);
		void DisableEffect(const std::string& effectName);
		IPostEffect* GetEffect(const std::string& effectName);

		uint32_t GetGameRenderTargetSrvIndex() const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGameRenderTargetSrvHandleGPU() const;
		uint32_t GetGameRenderTargetWidth() const;
		uint32_t GetGameRenderTargetHeight() const;
		void RequestGameRenderTargetResize(uint32_t width, uint32_t height);

		// W8 Screen Space FluidはScene Colorを別TextureへCopyしてから同じScene RTへCompositeする。
		ID3D12Resource* GetGameRenderTargetResource() const;
		D3D12_RESOURCE_STATES GetGameRenderTargetState() const;
		void SetGameRenderTargetState(D3D12_RESOURCE_STATES state);
		D3D12_CPU_DESCRIPTOR_HANDLE GetSceneDsvHandle() const;

	private:
		PostEffectManager();
		~PostEffectManager();
		PostEffectManager(const PostEffectManager&) = delete;
		PostEffectManager& operator=(const PostEffectManager&) = delete;

		DirectXCommon* dxCommon_ = nullptr;
		std::unique_ptr<PostEffectPipelineBuilder> pipelineBuilder_;
		std::unique_ptr<PostEffectRegistry> registry_;
		std::unique_ptr<PostEffectChain> chain_;
		std::unique_ptr<PostEffectRuntimeState> runtimeState_;
		std::unique_ptr<PostEffectRenderTargetManager> renderTargetManager_;
		std::unique_ptr<PostEffectExecutor> executor_;
		std::unique_ptr<PostEffectEditorPanel> editorPanel_;
	};
}
