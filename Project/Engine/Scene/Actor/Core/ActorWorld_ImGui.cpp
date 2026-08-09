#include "ActorWorld.h"

#include "ActorJsonSerializer.h"
#include "ComponentFactory.h"
#include <Editor/EditorCommandHistory.h>
#include <Editor/EditorContext.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		std::string MakeEditorUndoActorPath(const char* prefix, const Actor* actor)
		{
			static std::atomic_uint64_t serial = 0;
			const auto value = ++serial;
			return "../Generated/Intermediate/EditorUndo/" + std::string(prefix) + "_" +
				std::to_string(reinterpret_cast<uintptr_t>(actor)) + "_" + std::to_string(value) + ".json";
		}
	}

	void ActorWorld::DrawImGui()
	{
#ifdef USE_IMGUI
		if (legacyEditorWindowsEnabled_)
		{
			if (ImGui::Begin("Actor World"))
			{
				ImGui::Text("アクタ数: %zu", actors_.size());
				DrawActorPrefabSpawnImGui();
				ImGui::Separator();
				for (size_t actorIndex = 0; actorIndex < actors_.size(); ++actorIndex)
				{
					Actor* actor = actors_[actorIndex].get();
					if (!actor) continue;
					Actor* beforeSelectedActor = selectedActor_;
					ActorComponent* beforeSelectedComponent = selectedComponent_;
					ImGui::PushID(static_cast<int>(actorIndex));
					actor->DrawHierarchyImGui(selectedActor_, selectedComponent_);
					ImGui::PopID();
					if (beforeSelectedActor != selectedActor_ || beforeSelectedComponent != selectedComponent_) requestFocusActorDetails_ = true;
				}
			}
			ImGui::End();
			DrawDetailsImGui();
		}
		SyncLightComponentsToLightManager();
#endif
	}

	void ActorWorld::DrawDetailsImGui()
	{
#ifdef USE_IMGUI
		if (requestFocusActorDetails_)
		{
			ImGui::SetNextWindowFocus();
			requestFocusActorDetails_ = false;
		}
		if (ImGui::Begin("Actor Details")) DrawSelectedInspectorContent();
		ImGui::End();
#endif
	}

	void ActorWorld::DrawSelectedInspectorContent()
	{
#ifdef USE_IMGUI
		if (ImGui::CollapsingHeader("構成済みActor生成##ActorArchetypeCreation"))
		{
			DrawActorPrefabSpawnImGui(); // 現行DetailsからPrefab相当の構成済み定義を通常生成経路として利用する。
			ImGui::Separator();
		}

		Actor* saveTargetActor = selectedActor_;
		if (!saveTargetActor && selectedComponent_) saveTargetActor = selectedComponent_->GetOwner();

		if (saveTargetActor)
		{
			if (ImGui::Button("選択アクタをJSON保存"))
			{
				const std::string actorName = saveTargetActor->GetName().empty() ? saveTargetActor->GetClassTypeName() : saveTargetActor->GetName();
				const std::string filePath = "Resources/ActorPrefabs/" + actorName + ".json";
				const bool succeeded = ActorJsonSerializer::SaveActorToFile(*saveTargetActor, filePath);
				lastActorJsonSaveMessage_ = succeeded ? "保存しました: " + filePath : "保存に失敗しました: " + filePath;
			}

			ImGui::SameLine();
			if (ImGui::Button("JSONから再読込"))
			{
				const std::string actorName = saveTargetActor->GetName().empty() ? saveTargetActor->GetClassTypeName() : saveTargetActor->GetName();
				const std::string filePath = "Resources/ActorPrefabs/" + actorName + ".json";
				selectedComponent_ = nullptr;
				pendingReloadActor_ = saveTargetActor;
				pendingReloadFilePath_ = filePath;
				hasPendingReloadActor_ = true;
				lastActorJsonSaveMessage_ = "再読込を予約しました: " + filePath;
			}

			ImGui::SameLine();
			if (ImGui::Button("アクタを複製"))
			{
				const std::string snapshotPath = MakeEditorUndoActorPath("DuplicateActor", saveTargetActor);
				if (ActorJsonSerializer::SaveActorToFile(*saveTargetActor, snapshotPath))
				{
					auto duplicatedActor = std::make_shared<Actor*>(nullptr);
					EditorCommandHistory::GetInstance()->Clear();
					EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorLambdaCommand>(
						"アクタ複製",
						[this, duplicatedActor, snapshotPath]()
						{
							*duplicatedActor = SpawnActorFromJson(snapshotPath);
							if (*duplicatedActor)
							{
								selectedActor_ = *duplicatedActor;
								selectedComponent_ = nullptr;
								EditorContext::GetInstance()->GetSelection().Clear();
								EditorContext::GetInstance()->MarkLevelDirty();
							}
						},
						[this, duplicatedActor]()
						{
							if (!*duplicatedActor) return;
							(*duplicatedActor)->Destroy();
							*duplicatedActor = nullptr;
							selectedActor_ = nullptr;
							selectedComponent_ = nullptr;
							EditorContext::GetInstance()->GetSelection().Clear();
							EditorContext::GetInstance()->MarkLevelDirty();
						}));
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("アクタを削除")) ImGui::OpenPopup("アクタを削除しますか？");

			if (ImGui::BeginPopupModal("アクタを削除しますか？", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("%s を削除します。", saveTargetActor->GetName().c_str());
				ImGui::TextDisabled("Ctrl+Zで削除前の状態へ戻せます。");
				ImGui::Separator();
				if (ImGui::Button("削除", ImVec2(120.0f, 0.0f)))
				{
					const std::string snapshotPath = MakeEditorUndoActorPath("DeleteActor", saveTargetActor);
					if (ActorJsonSerializer::SaveActorToFile(*saveTargetActor, snapshotPath))
					{
						auto actorState = std::make_shared<Actor*>(saveTargetActor);
						EditorCommandHistory::GetInstance()->Clear();
						EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorLambdaCommand>(
							"アクタ削除",
							[this, actorState]()
							{
								if (!*actorState) return;
								(*actorState)->Destroy();
								*actorState = nullptr;
								selectedActor_ = nullptr;
								selectedComponent_ = nullptr;
								EditorContext::GetInstance()->GetSelection().Clear();
								EditorContext::GetInstance()->MarkLevelDirty();
							},
							[this, actorState, snapshotPath]()
							{
								*actorState = SpawnActorFromJson(snapshotPath);
								selectedActor_ = *actorState;
								selectedComponent_ = nullptr;
								EditorContext::GetInstance()->GetSelection().Clear();
								EditorContext::GetInstance()->MarkLevelDirty();
							}));
					}
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}

			if (!lastActorJsonSaveMessage_.empty()) ImGui::TextWrapped("%s", lastActorJsonSaveMessage_.c_str());
			ImGui::Separator();
		}

		if (selectedComponent_)
		{
			const std::string componentName = selectedComponent_->GetName().empty() ? "名前なしコンポーネント" : selectedComponent_->GetName();
			ImGui::Text("選択中のコンポーネント: %s", componentName.c_str());
			Actor* owner = selectedComponent_->GetOwner();
			const bool isRootComponent = owner && selectedComponent_ == owner->GetRootComponent();
			if (isRootComponent)
			{
				ImGui::TextDisabled("ルートコンポーネントは複製・削除できません。");
			}
			else
			{
				if (ImGui::Button("コンポーネントを複製")) DuplicateSelectedComponent();
				ImGui::SameLine();
				if (ImGui::Button("コンポーネントを削除")) ImGui::OpenPopup("コンポーネントを削除しますか？");
			}

			if (ImGui::BeginPopupModal("コンポーネントを削除しますか？", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("%s を削除します。", componentName.c_str());
				ImGui::Separator();
				if (ImGui::Button("削除", ImVec2(120.0f, 0.0f)))
				{
					DeleteSelectedComponent();
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}

			if (selectedComponent_)
			{
				ImGui::Separator();
				selectedComponent_->DrawInspectorImGui();
			}
		}
		else if (selectedActor_)
		{
			const std::string actorName = selectedActor_->GetName().empty() ? "名前なしアクタ" : selectedActor_->GetName();
			ImGui::Text("選択中のアクタ: %s", actorName.c_str());
			ImGui::Separator();
			selectedActor_->DrawInspectorImGui();
		}
		else
		{
			ImGui::TextDisabled("アクタまたはコンポーネントが選択されていません。");
		}

		DrawAddComponentImGui();
#endif
	}

	void ActorWorld::DrawAddComponentImGui()
	{
#ifdef USE_IMGUI
		Actor* targetActor = selectedActor_;
		if (!targetActor && selectedComponent_) targetActor = selectedComponent_->GetOwner();
		ImGui::SeparatorText("コンポーネント追加");
		if (!targetActor)
		{
			ImGui::TextDisabled("アクタが選択されていません。");
			return;
		}

		const auto& componentTypes = ComponentFactory::GetRegisteredComponentTypes();
		if (componentTypes.empty())
		{
			ImGui::TextDisabled("登録済みのコンポーネントがありません。");
			return;
		}

		if (selectedAddComponentTypeIndex_ < 0 || selectedAddComponentTypeIndex_ >= static_cast<int>(componentTypes.size())) selectedAddComponentTypeIndex_ = 0;
		const ComponentFactory::ComponentTypeInfo& selectedType = componentTypes[selectedAddComponentTypeIndex_];
		if (ImGui::BeginCombo("種類", selectedType.displayName.c_str()))
		{
			std::string currentCategory;
			for (int index = 0; index < static_cast<int>(componentTypes.size()); ++index)
			{
				const ComponentFactory::ComponentTypeInfo& typeInfo = componentTypes[index];
				if (currentCategory != typeInfo.category)
				{
					currentCategory = typeInfo.category;
					ImGui::TextDisabled("%s", currentCategory.c_str());
				}
				const bool alreadyExists = targetActor->HasComponentClass(typeInfo.className);
				const bool disabled = !typeInfo.allowMultiple && alreadyExists;
				if (disabled) ImGui::BeginDisabled();
				const bool isSelected = selectedAddComponentTypeIndex_ == index;
				const std::string selectableLabel = typeInfo.displayName + "##" + typeInfo.className;
				if (ImGui::Selectable(selectableLabel.c_str(), isSelected)) selectedAddComponentTypeIndex_ = index;
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", typeInfo.description.c_str());
				if (isSelected) ImGui::SetItemDefaultFocus();
				if (disabled) ImGui::EndDisabled();
			}
			ImGui::EndCombo();
		}

		const bool alreadyExists = targetActor->HasComponentClass(selectedType.className);
		const bool canAdd = selectedType.allowMultiple || !alreadyExists;
		ImGui::Text("カテゴリ: %s", selectedType.category.c_str());
		ImGui::TextWrapped("%s", selectedType.description.c_str());
		if (!canAdd)
		{
			ImGui::TextDisabled("このコンポーネントは1つだけ追加できます。");
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("追加##AddComponent")) AddComponentToSelectedActor(selectedType.className);
		if (!canAdd) ImGui::EndDisabled();
#endif
	}
} // namespace Ken4lowEngine
