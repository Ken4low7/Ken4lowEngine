#pragma once

namespace Ken4lowEngine::EditorPanelIds
{
	inline constexpr const char* Toolbar = "Toolbar";
	inline constexpr const char* PlaceActors = "Place Actors";
	inline constexpr const char* MainViewport = "Main Viewport";
	inline constexpr const char* ViewportToolbarOverlay = "ビューポートツールバー###ViewportToolbarOverlay";
	inline constexpr const char* WorldOutliner = "アウトライナー###World Outliner";
	inline constexpr const char* Details = "詳細###Details";
	inline constexpr const char* ContentBrowser = "Content Browser";
	inline constexpr const char* OutputLog = "診断###Diagnostics"; // Phase 12では旧Output LogのDock位置を統合Diagnosticsへ引き継ぐ。
	inline constexpr const char* Scene = "Scene";

	inline constexpr const char* Parameters = "Parameters";
	inline constexpr const char* Display = "Display";
	inline constexpr const char* PostEffectSettings = "Post Effect Settings";
	inline constexpr const char* LightEditor = "Light Editor";
	inline constexpr const char* JsonAssetManager = "Json Asset Manager";

	inline constexpr const char* GameDebug = "Game Debug";
	inline constexpr const char* CollisionDebug = "Collision Debug";
	inline constexpr const char* CullingDebug = "Culling Debug";
	inline constexpr const char* PhysicsWorldDebug = "PhysicsWorld Debug";
	inline constexpr const char* GpuParticleEditor = "GPU Particle Editor";
} // namespace Ken4lowEngine::EditorPanelIds
