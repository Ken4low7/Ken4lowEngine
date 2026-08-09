#pragma once

#ifdef USE_IMGUI
#if !defined(IMGUI_VERSION) && !defined(IMGUI_DEFINE_MATH_OPERATORS)
#define IMGUI_DEFINE_MATH_OPERATORS // ImGui未読込の翻訳単位だけMath Operatorを先行定義する。
#endif
#include "EditorObjectInfo.h"

namespace Ken4lowEngine
{
	/// <summary>Main Viewport上で選択Objectの移動・回転・拡縮を操作します。</summary>
	class EditorTransformGizmo
	{
	public:
		static EditorTransformGizmo* GetInstance();
		void Draw();
		bool IsUsing() const;
		bool IsOver() const;

	private:
		EditorTransformGizmo() = default;
		~EditorTransformGizmo() = default;
		EditorTransformGizmo(const EditorTransformGizmo&) = delete;
		EditorTransformGizmo& operator=(const EditorTransformGizmo&) = delete;

		void BeginTransformCommand(const EditorObjectInfo& target, const EditorTransform& before);
		void EndTransformCommand();

		bool transformCommandActive_ = false;
		EditorObjectInfo transformCommandTarget_{};
		EditorTransform transformCommandBefore_{};
	};
} // namespace Ken4lowEngine
#endif // USE_IMGUI
