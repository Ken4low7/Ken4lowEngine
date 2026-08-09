#pragma once
#include "Vector4.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///          フラスタムカリングデバッグコントローラー
/// -------------------------------------------------------------
class FrustumCullingDebugController
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(bool resetMainCamera = false);

	// 更新処理
	void Update(float deltaTime);

	// 描画処理
	void DrawDebug();

	// ImGui描画処理
	void DrawImGui();

	// ImGui描画処理（MainCamera設定などの通常Dockウィンドウ内に表示する内容）
	void DrawImGuiContent();

public: /// ---------- アクセサ ---------- ///

	// ワイヤーフレーム表示のON/OFF
	void SetWireframeVisible(bool visible) { showFrustumWireframe_ = visible; }

	// ワイヤーフレーム表示状態の取得
	bool IsWireframeVisible() const { return showFrustumWireframe_; }

private: /// ---------- メンバ関数 ---------- ///

	// MainCameraの位置/回転/FOV/クリップ距離/アスペクト比をImGuiで表示・編集する処理
	void DrawMainCameraImGui();

	// DebugCameraの位置/回転/FOV/クリップ距離をMainCameraにコピーする処理
	void CopyDebugCameraToMainCamera();

	// MainCameraを初期位置にリセットする処理
	void ResetMainCamera();

	// カリングカメラのNear/Far距離を取得する処理。MainCameraのクリップ距離をカリングカメラに反映させるために必要。
	void GetCullingCameraClipDistances(float& nearDistance, float& farDistance) const;

	// 現在のウィンドウのアスペクト比を取得する処理
	float GetCurrentWindowAspectRatio() const;

private: /// ---------- メンバ変数 ---------- ///

	// Frustumワイヤーフレームの色。デフォルトは緑っぽい色。
	K4E::Vector4 frustumWireColor_ = { 0.1f, 1.0f, 0.2f, 1.0f };

	// Frustumワイヤーフレーム表示フラグ。デフォルトはON。
	bool showFrustumWireframe_ = true;
};
