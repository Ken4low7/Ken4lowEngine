#pragma once

#ifdef USE_IMGUI

#include "EditorContext.h"
#include <Engine/Scene/Streaming/SubLevelManager.h>
#include <Engine/Scene/Streaming/WorldPartitionGrid.h>
#include <Engine/Scene/Streaming/WorldPartitionManager.h>
#include <imgui.h>

#include <algorithm>
#include <string>

namespace Ken4lowEngine
{
	class ActorWorld;

	namespace EditorWorldPartitionPanelDetail
	{
		inline const char* StateLabel(SubLevelState state)
		{
			switch (state)
			{
			case SubLevelState::Unloaded: return "Unloaded";
			case SubLevelState::Loading: return "Loading";
			case SubLevelState::Loaded: return "Loaded";
			case SubLevelState::Failed: return "Failed";
			default: return "Unknown";
			}
		}

		inline const char* DecisionLabel(WorldPartitionStreamingDecision decision)
		{
			switch (decision)
			{
			case WorldPartitionStreamingDecision::AlwaysLoaded: return "Always Load";
			case WorldPartitionStreamingDecision::Load: return "Load";
			case WorldPartitionStreamingDecision::Retain: return "Retain";
			case WorldPartitionStreamingDecision::Unload: return "Unload";
			default: return "Unknown";
			}
		}
	}

	inline void DrawEditorWorldPartitionPanel(ActorWorld* actorWorld)
	{
		using namespace EditorWorldPartitionPanelDetail;
		if (!ImGui::CollapsingHeader("World Partition##WorldPartitionEditor")) return;

		WorldPartitionManager* partition = WorldPartitionManager::GetInstance();
		if (!partition->IsConfiguredFor(actorWorld))
		{
			ImGui::TextDisabled("Current ActorWorldにはWorld Partition設定がありません。");
			return;
		}

		LevelWorldPartitionSettings settings = partition->GetSettings();
		bool settingsChanged = false;
		settingsChanged |= ImGui::Checkbox("Enabled##WorldPartition", &settings.enabled);
		settingsChanged |= ImGui::InputFloat("Cell Size", &settings.cellSize, 1.0f, 16.0f, "%.1f");
		settingsChanged |= ImGui::InputInt("Load Radius Cells", &settings.loadRadiusCells);
		settingsChanged |= ImGui::InputInt("Unload Radius Cells", &settings.unloadRadiusCells);
		if (settingsChanged)
		{
			partition->ApplyEditorSettings(settings);
			EditorContext::GetInstance()->MarkLevelDirty(); // Level保存時にRuntimeと同じWorld Partition設定をCaptureさせる。
			settings = partition->GetSettings();
		}

		const Vector3& sourcePosition = partition->GetStreamingSourcePosition();
		const WorldPartitionCell sourceCell = partition->GetStreamingSourceCell();
		ImGui::Text("Streaming Source: (%.1f, %.1f, %.1f)", sourcePosition.x, sourcePosition.y, sourcePosition.z);
		ImGui::Text("Source Cell: [%d, %d] | Loaded: %zu / %zu",
			sourceCell.x,
			sourceCell.z,
			partition->GetLoadedSubLevelCount(),
			partition->GetSubLevels().size());
		ImGui::TextDisabled("Load <= %d cells | Retain <= %d cells | Unload > %d cells",
			settings.loadRadiusCells,
			settings.unloadRadiusCells,
			settings.unloadRadiusCells);

		ImGui::SeparatorText("SubLevel Cells");
		if (partition->GetSubLevels().empty())
		{
			ImGui::TextDisabled("SubLevelが定義されていません。");
			return;
		}

		SubLevelManager* subLevels = SubLevelManager::GetInstance();
		for (const LevelSubLevelReference& storedReference : partition->GetSubLevels())
		{
			LevelSubLevelReference reference = storedReference;
			ImGui::PushID(reference.id.c_str());

			const SubLevelState state = subLevels->GetState(reference.id);
			const WorldPartitionCell targetCell{ reference.cellX, reference.cellZ };
			const int distance = WorldPartitionGrid::ChebyshevDistance(sourceCell, targetCell);
			const WorldPartitionStreamingDecision decision = WorldPartitionGrid::Evaluate(
				sourceCell,
				targetCell,
				reference.alwaysLoaded,
				settings.loadRadiusCells,
				settings.unloadRadiusCells);
			const std::string title = reference.id + "  [" + StateLabel(state) + "]##CellEntry";

			if (ImGui::TreeNode(title.c_str()))
			{
				ImGui::TextWrapped("Path: %s", reference.path.c_str());
				ImGui::Text("Cell [%d, %d] | Distance %d | Policy %s",
					reference.cellX,
					reference.cellZ,
					distance,
					settings.enabled ? DecisionLabel(decision) : "Streaming Disabled");

				bool metadataChanged = false;
				metadataChanged |= ImGui::InputInt("Cell X", &reference.cellX);
				metadataChanged |= ImGui::InputInt("Cell Z", &reference.cellZ);
				metadataChanged |= ImGui::InputInt("Priority (0-3)", &reference.priority);
				metadataChanged |= ImGui::Checkbox("Always Loaded", &reference.alwaysLoaded);
				if (metadataChanged)
				{
					reference.priority = (std::clamp)(reference.priority, 0, 3);
					if (partition->UpdateSubLevelEditorMetadata(
						reference.id,
						reference.cellX,
						reference.cellZ,
						reference.priority,
						reference.alwaysLoaded))
					{
						EditorContext::GetInstance()->MarkLevelDirty();
					}
				}

				if (state == SubLevelState::Failed)
				{
					if (ImGui::Button("Retry Load")) subLevels->Retry(reference.id);
					const std::string& error = subLevels->GetLastError(reference.id);
					if (!error.empty()) ImGui::TextWrapped("Error: %s", error.c_str());
				}
				else
				{
					if (state == SubLevelState::Unloaded)
					{
						if (ImGui::Button("Load Now")) subLevels->RequestLoad(reference.id);
					}
					else if (!reference.alwaysLoaded)
					{
						if (ImGui::Button("Unload Now")) subLevels->RequestUnload(reference.id);
					}
				}
				ImGui::TextDisabled("Manual Load/Unloadは次の自動Streaming評価で上書きされる場合があります。");
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
} // namespace Ken4lowEngine

#endif // USE_IMGUI
