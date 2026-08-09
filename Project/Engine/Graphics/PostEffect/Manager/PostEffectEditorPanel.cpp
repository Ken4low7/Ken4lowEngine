#include "PostEffectEditorPanel.h"

#include "PostEffectRegistry.h"
#include "PostEffectRuntimeState.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void PostEffectEditorPanel::Draw(PostEffectRegistry& registry, PostEffectRuntimeState& runtimeState, bool* pOpen)
	{
#ifdef USE_IMGUI
		// WindowメニューのPost Effect Settings表示フラグが閉じている間は、既存通りUIを生成しない。
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		ImGui::Begin("Post Effect Settings", pOpen);

		for (const PostEffectDefinition& definition : registry.GetDefinitions())
		{
			bool enabled = runtimeState.IsEditorEnabled(definition.name);
			if (ImGui::Checkbox(definition.name.c_str(), &enabled))
			{
				runtimeState.SetEditorEnabled(definition.name, enabled);
			}
			if (enabled)
			{
				if (IPostEffect* effect = registry.Find(definition.name))
				{
					effect->DrawImGui(); // 各Effect固有UIはEffect実装へ委譲する。
				}
			}
		}

		ImGui::End();
#else
		(void)registry;
		(void)runtimeState;
		(void)pOpen;
#endif // USE_IMGUI
	}
}
