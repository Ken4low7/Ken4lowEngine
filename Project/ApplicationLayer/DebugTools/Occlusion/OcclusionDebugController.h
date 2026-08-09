#pragma once

namespace Ken4lowEngine { class Stage; }
namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///          オクルージョンカリングデバッグコントローラー
/// -------------------------------------------------------------
class OcclusionDebugController
{
public: /// ---------- メンバ関数 ---------- ///

	// 描画処理
	void DrawImGui(K4E::Stage* stage);

	// ImGui描画処理（通常Dockウィンドウ内に表示する内容）
	void DrawImGuiContent(K4E::Stage* stage);
};
