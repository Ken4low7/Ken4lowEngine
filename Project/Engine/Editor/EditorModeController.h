#pragma once

namespace Ken4lowEngine
{
	class Input;

	/// <summary>
	/// Debugビルド中のEditor Mode / Game Preview Modeを一元管理します。
	/// Releaseビルドでは常にGame Preview Mode相当として扱い、Editor UIとDebug表示を出さない方針にします。
	/// </summary>
	class EditorModeController
	{
	public:
		/// <summary>
		/// シングルトンインスタンスを取得します。
		/// </summary>
		static EditorModeController* GetInstance();

		/// <summary>
		/// 起動時のモードをビルド種別に合わせて初期化します。
		/// DebugではEditor Mode ON、ReleaseではGame Preview Mode相当になります。
		/// </summary>
		void Initialize();

		/// <summary>
		/// F1キーの押下を監視し、Debugビルド中だけEditor Modeを切り替えます。
		/// </summary>
		void Update(Input* input);

		/// <summary>
		/// Editor ModeのON/OFFを明示的に設定します。
		/// Releaseビルドでは要求値に関係なくOFFになります。
		/// </summary>
		void SetEditorModeEnabled(bool enabled);

		/// <summary>
		/// 現在Editor Modeとして動作しているかを返します。
		/// </summary>
		bool IsEditorModeEnabled() const;

		/// <summary>
		/// 現在Game Preview Modeとして動作しているかを返します。
		/// </summary>
		bool IsGamePreviewMode() const { return !IsEditorModeEnabled(); }

		/// <summary>
		/// ImGuiやEditor系Windowを描画してよいかを返します。
		/// </summary>
		bool ShouldDrawEditorUi() const { return IsEditorModeEnabled(); }

		/// <summary>
		/// Wireframe / ColliderなどのDebug表示を描画してよいかを返します。
		/// </summary>
		bool ShouldDrawDebugVisuals() const { return IsEditorModeEnabled(); }

	private:
		EditorModeController() = default;
		~EditorModeController() = default;
		EditorModeController(const EditorModeController&) = delete;
		EditorModeController& operator=(const EditorModeController&) = delete;

		void ApplyModeSideEffects();
		void NotifyModeChanged() const;

		bool editorModeEnabled_ = false;
	};

} // namespace Ken4lowEngine
