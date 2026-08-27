#pragma once

namespace Ken4lowEngine::EditorPanelIds
{
	// 表示名だけを日本語化し、###以降の内部IDは既存レイアウト互換のため維持する。
	inline constexpr const char* Toolbar = "ツールバー###Toolbar";
	inline constexpr const char* PlaceActors = "アクタ配置###Place Actors";
	inline constexpr const char* MainViewport = "メインビューポート###Main Viewport";
	inline constexpr const char* ViewportToolbarOverlay = "ビューポートツールバー###ViewportToolbarOverlay";
	inline constexpr const char* WorldOutliner = "アウトライナー###World Outliner";
	inline constexpr const char* Details = "詳細###Details";
	inline constexpr const char* ContentBrowser = "コンテンツブラウザ###Content Browser";
	inline constexpr const char* OutputLog = "診断###Diagnostics";
	inline constexpr const char* Scene = "シーン###Scene";

	inline constexpr const char* Parameters = "パラメーター###Parameters";
	inline constexpr const char* Display = "表示###Display";
	inline constexpr const char* PostEffectSettings = "ポストエフェクト設定###Post Effect Settings";
	inline constexpr const char* LightEditor = "ライト編集###Light Editor";
	inline constexpr const char* JsonAssetManager = "JSONアセット管理###Json Asset Manager";

	inline constexpr const char* GameDebug = "ゲームデバッグ###Game Debug";
	inline constexpr const char* CollisionDebug = "衝突デバッグ###Collision Debug";
	inline constexpr const char* CullingDebug = "カリングデバッグ###Culling Debug";
	inline constexpr const char* PhysicsWorldDebug = "物理ワールドデバッグ###PhysicsWorld Debug";
	inline constexpr const char* GpuParticleEditor = "GPUパーティクル編集###GPU Particle Editor";
} // namespace Ken4lowEngine::EditorPanelIds
