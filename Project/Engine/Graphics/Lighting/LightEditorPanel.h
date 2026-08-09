#pragma once

namespace Ken4lowEngine
{
	class LightManager;

	/// <summary>
	/// LightManagerが所有するライト/Shadow設定をImGuiで表示・編集するエディタ専用パネルです。<br/>
	/// ライトデータやGPUリソースは所有せず、Runtime責務を持つLightManagerからUI責務だけを分けるための
	/// 薄いビューとして機能します。Preset保存/読み込みも実体処理はLightManagerへ委譲します。
	/// </summary>
	class LightEditorPanel
	{
	public:
		/// <summary>
		/// Docking可能なLight Editorウィンドウ全体を描画します。<br/>
		/// 既存のLightManager::DrawImGui互換入口から呼ばれ、ウィンドウ開閉状態とPreset操作を扱います。
		/// </summary>
		void Draw(LightManager& lightManager, bool* pOpen = nullptr);

		/// <summary>
		/// Global Lighting/IBL/Shadow/LightComponent DebugのInspector UIを描画します。<br/>
		/// Details Inspectorと専用Light Editorで同じ表示を使えるよう、ウィンドウ枠なしの中身だけを提供します。
		/// </summary>
		void DrawPunctualLightsInspector(LightManager& lightManager);
	};
}
