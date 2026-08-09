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

	/// <summary>
	/// 既存PostEffect APIを維持するFacadeです。<br/>
	/// 登録・順序・RenderTarget・Barrier/Apply・Editor UI・Runtime状態は各専用クラスへ委譲します。
	/// </summary>
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

		/// <summary>Runtime側からEffectを強制的に有効化します。</summary>
		void EnableEffect(const std::string& effectName);
		/// <summary>Runtime側の強制有効状態を解除します。</summary>
		void DisableEffect(const std::string& effectName);
		IPostEffect* GetEffect(const std::string& effectName);

		uint32_t GetGameRenderTargetSrvIndex() const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGameRenderTargetSrvHandleGPU() const;
		uint32_t GetGameRenderTargetWidth() const;
		uint32_t GetGameRenderTargetHeight() const;
		void RequestGameRenderTargetResize(uint32_t width, uint32_t height);

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
