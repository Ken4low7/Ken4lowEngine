#pragma once

#include "DX12Include.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include <Engine/Graphics/RenderGraph/RenderGraph.h>
#include <Engine/Graphics/RenderGraph/RenderGraphTransientPool.h>
#include <Engine/Graphics/RenderGraph/RenderGraphD3D12TransientPool.h> // Phase 9.4のheader-only allocation/backendを通常C++ buildでも検証する。

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// <summary>
	/// 既存描画処理を並べるだけの薄いレンダーパイプライン入口です。<br/>
	/// Phase 6ではRenderGraphへPass依存を宣言し、GameApplicationに散らばっていた1フレーム内の
	/// Shadow/Scene/PostEffect/UI/Present手前までの順序を見える化するためのFacadeとして機能します。
	/// </summary>
	class RenderPipelineController
	{
	public:
		/// <summary>
		/// GameApplicationが既存描画関数を差し込むためのコールバック群です。<br/>
		/// Controllerは描画リソースを所有せず、既存関数の呼び出し順だけを管理します。
		/// </summary>
		struct FrameCallbacks
		{
			std::function<void()> prepareShadowPass;
			std::function<void()> drawShadowObjects;
			std::function<void()> buildEditorUi;
			std::function<void()> executeEditorPickingPass;
			std::function<void()> drawGameWorldToSceneTarget;
			std::function<void()> renderPostEffectToGameRenderTarget;
			std::function<void()> renderEditorSelectionOutline;
			std::function<void()> beginGameRenderTargetOverlay;
			std::function<void()> drawScene2DOverlay;
			std::function<void()> endGameRenderTargetOverlay;
			std::function<void()> drawImGuiOverlay;
			std::function<void()> applyPostEffectToBackBuffer;
			std::function<void()> rebindBackBufferForGameOverlay;
			std::function<void()> drawGameUIToBackBuffer;
		};

		enum class PerformancePhase : std::size_t
		{
			BeginDraw,
			ShadowPrepare,
			ShadowRender,
			EditorUiBuild,
			EditorPicking,
			MainWorldRender,
			PostEffect,
			SelectionOutline,
			SceneOverlay,
			ImGuiRender,
			BackBufferPostEffect,
			BackBufferRebind,
			GameUi,
			Count,
		};

		struct PerformanceMetric
		{
			float lastMs = 0.0f;
			float averageMs = 0.0f;
			float maxMs = 0.0f;
			std::size_t sampleCount = 0;
		};

		struct FrameTimingSummary
		{
			float frameIntervalMs = 0.0f;
			float updateMs = 0.0f;
			float drawMs = 0.0f;
			float presentMs = 0.0f;
			float totalFrameMs = 0.0f;
		};

		/// <summary>
		/// DirectXCommonを保持し、低レベル描画APIを呼ぶための入口を設定します。<br/>
		/// リソース生成やCommandList所有はDirectXCommon側に残します。
		/// </summary>
		void Initialize(DirectXCommon* dxCommon);

		/// <summary>
		/// 1フレーム分の既存描画順序を実行します。<br/>
		/// editorModeEnabledがtrueのときはMain Viewport用GameRenderTargetへ描画し、falseのときはBackBufferへ出力します。
		/// </summary>
		void ExecuteFrame(bool editorModeEnabled, const FrameCallbacks& callbacks);

		/// 完了済みフレームのUpdate/Draw/Present計測値を保持し、次フレームのEditor UIから参照できるようにする。
		void SetFrameTimingSummary(const FrameTimingSummary& summary) { frameTimingSummary_ = summary; }

		/// RenderPipeline各Passと完了済みフレームのCPU/GPU時間をEditorへ表示する。
		void DrawPerformanceImGui();

		const PerformanceMetric& GetPerformanceMetric(PerformancePhase phase) const;
		const PerformanceMetric& GetGpuPerformanceMetric(PerformancePhase phase) const { return gpuPerformanceMetrics_[ToIndex(phase)]; }
		const PerformanceMetric& GetGpuFrameMetric() const { return gpuFrameMetric_; }
		const FrameTimingSummary& GetFrameTimingSummary() const { return frameTimingSummary_; }
		const RenderGraph& GetRenderGraph() const { return renderGraph_; }
		const RenderGraphTransientPool& GetRenderGraphTransientPool() const { return renderGraphTransientPool_; }
		DirectXCommon* GetDirectXCommon() const { return dxCommon_; }

		/// 現在GameApplicationが使用しているControllerをDebugSceneの診断UIから参照する。
		static RenderPipelineController* GetActiveController() { return activeController_; }

	private:
		using Clock = std::chrono::steady_clock;
		static constexpr std::size_t kPerformancePhaseCount = static_cast<std::size_t>(PerformancePhase::Count);
		static constexpr uint32_t kGpuQueriesPerPhase = 2;
		static constexpr uint32_t kGpuFrameQueryCount = 2;
		static constexpr uint32_t kGpuQueriesPerFrame = static_cast<uint32_t>(kPerformancePhaseCount) * kGpuQueriesPerPhase + kGpuFrameQueryCount;

		struct GpuFrameState
		{
			std::array<bool, kPerformancePhaseCount> recordedPhases{};
			bool pendingResolve = false;
		};

		static constexpr std::size_t ToIndex(PerformancePhase phase)
		{
			return static_cast<std::size_t>(phase);
		}

		void MeasurePhase(PerformancePhase phase, const std::function<void()>& callback);
		void UpdatePerformanceMetric(PerformancePhase phase, float elapsedMs);
		void UpdateGpuPerformanceMetric(PerformancePhase phase, float elapsedMs);
		static const char* GetPerformancePhaseName(PerformancePhase phase);

		void InitializeGpuTiming();
		void CollectGpuTiming(uint32_t frameIndex);
		void BeginGpuFrame(uint32_t frameIndex);
		void EndGpuFrame(uint32_t frameIndex);
		void WriteGpuPhaseTimestamp(PerformancePhase phase, bool begin);
		uint32_t GetGpuFrameBaseQuery(uint32_t frameIndex) const;
		uint32_t GetGpuPhaseQuery(uint32_t frameIndex, PerformancePhase phase, bool begin) const;
		uint32_t GetGpuFrameTotalQuery(uint32_t frameIndex, bool begin) const;

		/// <summary>
		/// ShadowMapへ深度を書き込む既存パスを実行します。<br/>
		/// 通常3D描画より先に行うことで、後段のライティングがShadowMapを参照できる順序を維持します。
		/// </summary>
		void ExecuteShadowMapPass(const FrameCallbacks& callbacks);

		/// <summary>
		/// Editor Modeの既存描画順を実行します。<br/>
		/// ImGuiのMain Viewportへ表示するため、3D/PostEffect/UIをGameRenderTargetに集約してからImGuiを描画します。
		/// </summary>
		void ExecuteEditorFrame(const FrameCallbacks& callbacks);

		/// <summary>
		/// Game Preview / Release相当の既存描画順を実行します。<br/>
		/// SceneRenderTargetへ3Dを描いた後、PostEffect結果をBackBufferへ出してUIを重ねます。
		/// </summary>
		void ExecuteGameFrame(const FrameCallbacks& callbacks);

		static RenderPipelineController* activeController_;
		DirectXCommon* dxCommon_ = nullptr;
		RenderGraph renderGraph_{};
		// Visualizerは実際のTransient planner状態だけを読み、独自のaliasing推測を作らない。
		RenderGraphTransientPool renderGraphTransientPool_{};
		std::array<PerformanceMetric, kPerformancePhaseCount> performanceMetrics_{};
		std::array<PerformanceMetric, kPerformancePhaseCount> gpuPerformanceMetrics_{};
		PerformanceMetric gpuFrameMetric_{};
		FrameTimingSummary frameTimingSummary_{};

		ComPtr<ID3D12QueryHeap> gpuTimestampHeap_{};
		ComPtr<ID3D12Resource> gpuTimestampReadback_{};
		std::vector<GpuFrameState> gpuFrameStates_{};
		uint64_t gpuTimestampFrequency_ = 0;
		uint32_t gpuFrameResourceCount_ = 0;
		uint32_t currentGpuFrameIndex_ = 0;
		bool gpuTimingAvailable_ = false;

		uint64_t framesInFlightStableFrames_ = 0;
		uint64_t frameSyncMismatchCount_ = 0;
	};
} // namespace Ken4lowEngine
