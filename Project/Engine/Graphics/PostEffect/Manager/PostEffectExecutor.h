#pragma once

#include "DX12Include.h"

namespace Ken4lowEngine
{
	class DirectXCommon;
	class PostEffectPipelineBuilder;
	class PostEffectRegistry;
	class PostEffectChain;
	class PostEffectRuntimeState;
	class PostEffectRenderTargetManager;
	struct PostEffectRenderTarget;

	/// <summary>
	/// PostEffectのResourceBarrier、Effect Apply、ping-pong、BackBufferコピーを実行します。<br/>
	/// Effect所有、実行順定義、Runtime状態、RenderTarget生成は外部の各責務クラスから参照します。
	/// </summary>
	class PostEffectExecutor
	{
	public:
		void Initialize(
			DirectXCommon* dxCommon,
			PostEffectPipelineBuilder* pipelineBuilder,
			PostEffectRegistry* registry,
			PostEffectChain* chain,
			PostEffectRuntimeState* runtimeState,
			PostEffectRenderTargetManager* renderTargetManager);
		void Finalize();

		void BeginDraw();
		void EndDraw();
		void RenderPostEffect();
		void RenderPostEffectToBackBuffer();
		void BeginGameRenderTargetOverlay();
		void EndGameRenderTargetOverlay();
		void BindSceneRenderTarget();

	private:
		void TransitionTo(PostEffectRenderTarget& renderTarget, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES nextState);
		void TransitionDepthTo(D3D12_RESOURCE_STATES nextState);
		void CopyRenderTarget(PostEffectRenderTarget& source, PostEffectRenderTarget& destination, ID3D12GraphicsCommandList* commandList);
		void CopyRenderTargetToBackBuffer(PostEffectRenderTarget& source, ID3D12GraphicsCommandList* commandList);

		DirectXCommon* dxCommon_ = nullptr;
		PostEffectPipelineBuilder* pipelineBuilder_ = nullptr;
		PostEffectRegistry* registry_ = nullptr;
		PostEffectChain* chain_ = nullptr;
		PostEffectRuntimeState* runtimeState_ = nullptr;
		PostEffectRenderTargetManager* renderTargetManager_ = nullptr;
	};
}
