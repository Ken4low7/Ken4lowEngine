#pragma once
#include "Vector2.h"
#include "EditorContext.h"
#include "EditorAssetBrowser.h"
#include "EditorAssetBuildService.h"
#include "EditorOutputLog.h"
#include "EditorTexturePreviewCache.h"
#include "PerformanceMonitor.h"

namespace Ken4lowEngine
{
	class SceneManager;

	/// <summary>
	/// UE5風エディタUIの各ウィンドウ表示状態を保持します。
	/// </summary>
	struct EditorWindowState
	{
		// 旧文字Toolbarは既定で隠し、Viewport上のアイコンToolbarを標準操作にする。
		bool showToolbar = false;
		bool showMainViewport = true;
		bool showContentBrowser = true;
		bool showWorldOutliner = true;
		bool showDetails = true;
		bool showOutputLog = false; // Phase 12のDiagnosticsへLog / Profiler / Errorを統合したため旧Panelは既定で隠す。
		bool showScene = true;

		bool showParameters = true;
		bool showDisplay = true;
		bool showPostEffectSettings = true;
		bool showLightEditor = true;
		bool showJsonAssetManager = true;

		bool showTitleDebug = true;
		bool showStageSelectDebug = true;
		bool showGameDebug = true;
		bool showPlayerDebug = true;
		bool showWeaponDebug = true;
		bool showEnemyDebug = true;
		bool showCollisionDebug = true;
		bool showCullingDebug = true;
		bool showFadeManager = true;

		bool debugShowCollider = false;
		bool debugShowEnemyInfo = false;
		bool debugShowWaveInfo = false;
		bool debugShowFps = true;
	};

	/// <summary>
	/// Main Viewport内で実際にゲーム画面を表示している矩形を保持します。
	/// </summary>
	struct EditorViewportRect
	{
		Vector2 screenMin = { 0.0f, 0.0f };
		Vector2 screenMax = { 0.0f, 0.0f };
		Vector2 imageSize = { 0.0f, 0.0f };
		bool isHovered = false;
		bool valid = false;
	};

	struct EditorInputDebugInfo
	{
		Vector2 gameMousePosition = { -1.0f, -1.0f };
		bool mainViewportHovered = false;
		bool imguiMouseClicked0 = false;
		bool imguiMouseDown0 = false;
		bool inputLeftTrigger = false;
		bool gameMouseEnabled = false;
	};

	/// <summary>
	/// UE5風エディタUIのメニュー、ツールバー、各パネルをまとめて描画します。
	/// </summary>
	class EditorWindowManager
	{
	public:
		enum class OutputLogPerformanceDisplayMode
		{
			FPS = 0,
			FrameTimeMs = 1
		};

		static EditorWindowManager* GetInstance();

		void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }
		SceneManager* GetSceneManager() const { return sceneManager_; }

		void Draw();
		void DrawMenuBar();
		void DrawToolbar();
		void DrawMainViewport();
		void DrawWorldOutliner();
		void DrawDetails();
		void DrawContentBrowser();
		void DrawOutputLog();
		void DrawScene();
		void InitializeEditorServices();
		void FinalizeEditorServices();

		void AddOutputLog(EditorLogLevel level, const std::string& message);
		Vector2 ConvertScreenToMainViewportPosition(const Vector2& screenPosition) const;
		bool GetMousePositionInGameViewport(Vector2& outMouse) const;

		const EditorViewportRect& GetMainViewportRect() const { return mainViewportRect_; }
		const Vector2& GetMainViewportScreenPosition() const { return mainViewportScreenPosition_; }
		const Vector2& GetMainViewportSize() const { return mainViewportSize_; }

		EditorWindowState& GetWindowState() { return windowState_; }
		const EditorWindowState& GetWindowState() const { return windowState_; }

		void SetPerformanceOverlayVisible(bool visible) { showPerformanceOverlay_ = visible; }
		bool IsPerformanceOverlayVisible() const { return showPerformanceOverlay_; }

		bool StartAllAssetBuild() { return assetBuildService_.StartBuild(EditorAssetBuildKind::All); }
		bool IsAssetBuildRunning() const { return assetBuildService_.IsRunning(); }

	private:
		EditorWindowManager() = default;
		~EditorWindowManager() = default;
		EditorWindowManager(const EditorWindowManager&) = delete;
		EditorWindowManager& operator=(const EditorWindowManager&) = delete;

		void DrawAssetList();
		void DrawTextureGrid();
		void DrawAssetDetails();

		SceneManager* sceneManager_ = nullptr;
		EditorWindowState windowState_{};
		EditorViewportRect mainViewportRect_{};
		Vector2 mainViewportScreenPosition_ = { 0.0f, 0.0f };
		Vector2 mainViewportSize_ = { 0.0f, 0.0f };
		EditorInputDebugInfo inputDebugInfo_{};
		EditorSelection& selection_ = EditorContext::GetInstance()->GetSelection();
		EditorOutputLog outputLog_{};
		EditorAssetBrowser assetBrowser_{};
		EditorTexturePreviewCache texturePreviewCache_{};
		EditorAssetBuildService assetBuildService_{};
		bool editorServicesInitialized_ = false;
		bool outputLogAutoScroll_ = true;
		bool openRebuildDefaultLayoutPopup_ = false;
		PerformanceMonitor outputLogPerformanceMonitor_{};
		OutputLogPerformanceDisplayMode outputLogPerformanceDisplayMode_ = OutputLogPerformanceDisplayMode::FPS;
		bool showPerformanceInOutputLog_ = true;
		bool showPerformanceOverlay_ = true;
		bool performanceOverlayCompactMode_ = false;
	};

} // namespace Ken4lowEngine
