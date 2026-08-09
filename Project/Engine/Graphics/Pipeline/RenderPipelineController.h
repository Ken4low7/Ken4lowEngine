#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// <summary>
	/// 既存描画処理を並べるだけの薄いレンダーパイプライン入口です。<br/>
	/// RenderPass/FrameGraphはまだ導入せず、GameApplicationに散らばっていた1フレーム内の
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

		/// RenderPipeline各Passと完了済みフレームのCPU時間をEditorへ表示する。
		void DrawPerformanceImGui();

		const PerformanceMetric& GetPerformanceMetric(PerformancePhase phase) const;
		const FrameTimingSummary& GetFrameTimingSummary() const { return frameTimingSummary_; }

		/// 現在GameApplicationが使用しているControllerをDebugSceneの診断UIから参照する。
		static RenderPipelineController* GetActiveController() { return activeController_; }

	private:
		using Clock = std::chrono::steady_clock;
		static constexpr std::size_t kPerformancePhaseCount = static_cast<std::size_t>(PerformancePhase::Count);

		static constexpr std::size_t ToIndex(PerformancePhase phase)
		{
			return static_cast<std::size_t>(phase);
		}

		void MeasurePhase(PerformancePhase phase, const std::function<void()>& callback);
		void UpdatePerformanceMetric(PerformancePhase phase, float elapsedMs);
		static const char* GetPerformancePhaseName(PerformancePhase phase);

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
		std::array<PerformanceMetric, kPerformancePhaseCount> performanceMetrics_{};
		FrameTimingSummary frameTimingSummary_{};
	};
} // namespace Ken4lowEngine
