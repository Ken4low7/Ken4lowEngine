#pragma once

#ifdef USE_IMGUI

#include "EditorPrefabDiff.h"

#include <ActorJsonSerializer.h>
#include <PrefabInstanceRegistry.h>
#include <PrefabReferenceResolver.h>
#include <imgui.h>

#include <cstddef>
#include <string>

namespace Ken4lowEngine
{
	class Actor;

	namespace EditorPrefabDiffPanelDetail
	{
		struct State final
		{
			const Actor* actor = nullptr;
			std::string prefabPath;
			std::string error;
			EditorPrefabDiffResult diff{};
			bool refreshed = false;
		};

		inline State& GetState()
		{
			static State state;
			return state;
		}

		inline std::string JsonPreview(const nlohmann::json& value, bool exists)
		{
			if (!exists) return "<missing>";
			std::string text = value.dump();
			constexpr std::size_t kPreviewLength = 180;
			if (text.size() > kPreviewLength) text.resize(kPreviewLength), text += "...";
			return text;
		}

		inline void Refresh(const Actor* actor)
		{
			State& state = GetState();
			state = {};
			state.actor = actor;
			state.refreshed = true;
			if (!actor)
			{
				state.error = "選択中のActorがありません。";
				return;
			}

			if (!PrefabInstanceRegistry::GetInstance()->Find(actor, state.prefabPath))
			{
				state.error = "このActorには追跡中のPrefab参照がありません。";
				return;
			}

			nlohmann::json baseActorJson;
			if (!PrefabReferenceResolver::LoadBaseActor(state.prefabPath, {}, baseActorJson, state.error)) return;

			const nlohmann::json instanceActorJson = ActorJsonSerializer::SerializeActor(*actor);
			state.diff = EditorPrefabDiff::Build(baseActorJson, instanceActorJson); // 表示専用差分なのでPrefab/Instance本体は変更しない。
		}

		inline const char* KindLabel(EditorPrefabDiffKind kind)
		{
			switch (kind)
			{
			case EditorPrefabDiffKind::ActorPropertyChanged: return "Actor Property";
			case EditorPrefabDiffKind::ComponentAdded: return "Component Added";
			case EditorPrefabDiffKind::ComponentRemoved: return "Component Removed";
			case EditorPrefabDiffKind::ComponentPropertyChanged: return "Component Property";
			default: return "Unknown";
			}
		}
	}

	inline void DrawEditorPrefabDiffPanel(const Actor* actor)
	{
		using namespace EditorPrefabDiffPanelDetail;
		State& state = GetState();
		if (!state.refreshed || state.actor != actor) Refresh(actor);

		ImGui::SeparatorText("Prefab Diff");
		if (ImGui::Button("Refresh Prefab Diff")) Refresh(actor);

		if (!state.prefabPath.empty()) ImGui::TextWrapped("Source: %s", state.prefabPath.c_str());
		if (!state.error.empty())
		{
			ImGui::TextDisabled("%s", state.error.c_str());
			return;
		}

		const EditorPrefabDiffSummary& summary = state.diff.summary;
		ImGui::Text(
			"Changes %zu | Actor %zu | Added %zu | Removed %zu | Modified %zu",
			summary.GetTotalChangeCount(),
			summary.actorPropertyChanges,
			summary.componentAdded,
			summary.componentRemoved,
			summary.componentPropertyChanges);

		if (!state.diff.HasChanges())
		{
			ImGui::TextDisabled("Prefab baseからの差分はありません。");
			return;
		}

		const bool childVisible = ImGui::BeginChild("PrefabDiffEntries", ImVec2(0.0f, 220.0f), true);
		if (childVisible)
		{
			for (std::size_t index = 0; index < state.diff.entries.size(); ++index)
			{
				const EditorPrefabDiffEntry& entry = state.diff.entries[index];
				std::string label = "[" + std::string(KindLabel(entry.kind)) + "] ";
				if (!entry.componentName.empty()) label += entry.componentName;
				if (!entry.componentClass.empty()) label += " (" + entry.componentClass + ")";
				if (!entry.propertyPath.empty())
				{
					if (!entry.componentName.empty()) label += ".";
					label += entry.propertyPath;
				}
				label += "##PrefabDiff" + std::to_string(index);

				if (ImGui::TreeNode(label.c_str()))
				{
					const std::string before = JsonPreview(entry.baseValue, entry.baseExists);
					const std::string after = JsonPreview(entry.instanceValue, entry.instanceExists);
					ImGui::TextDisabled("Before");
					ImGui::TextWrapped("%s", before.c_str());
					ImGui::TextDisabled("After");
					ImGui::TextWrapped("%s", after.c_str());
					ImGui::TreePop();
				}
			}
		}
		ImGui::EndChild();
	}
} // namespace Ken4lowEngine

#endif // USE_IMGUI
