#include "ActorWorld.h"

#include "ActorFactory.h"
#include "ActorJsonSerializer.h"
#include "ComponentFactory.h"
#include "SceneComponent.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef USE_IMGUI
#include <Editor/EditorContext.h>
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		/// Editorの通常生成に使用する軽量な構成済みActor定義。
		struct ActorArchetypeDefinition
		{
			std::string id;
			std::string displayName;
			std::string description;
			std::string type;
			std::vector<std::string> components;
		};

		/// Archetype一覧のEditor選択状態を保持する。
		struct ActorArchetypeBrowserState
		{
			std::string directory = "Resources/ActorArchetypes";
			std::vector<std::string> files;
			int selectedIndex = 0;
			bool initialized = false;
		};

		ActorArchetypeBrowserState& GetActorArchetypeBrowserState()
		{
			static ActorArchetypeBrowserState state;
			return state;
		}

		/// ArchetypeフォルダからJSON定義だけを列挙する。
		void RefreshActorArchetypeFiles(ActorArchetypeBrowserState& state)
		{
			state.files.clear();
			const std::filesystem::path directoryPath{ state.directory };
			if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath))
			{
				state.selectedIndex = 0;
				state.initialized = true;
				return;
			}

			for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
			{
				if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
				state.files.push_back(entry.path().generic_string());
			}
			std::sort(state.files.begin(), state.files.end());
			if (state.selectedIndex < 0 || state.selectedIndex >= static_cast<int>(state.files.size())) state.selectedIndex = 0;
			state.initialized = true;
		}

		/// TypeとComponent一覧を検証し、Archetype JSONを実行時定義へ変換する。
		bool LoadActorArchetypeDefinition(const std::string& filePath, ActorArchetypeDefinition& outDefinition, std::string& outError)
		{
			try
			{
				std::ifstream file{ std::filesystem::path(filePath) };
				if (!file.is_open())
				{
					outError = "Archetypeファイルを開けません: " + filePath;
					return false;
				}

				nlohmann::json json;
				file >> json;
				if (!json.is_object() || !json.contains("Type") || !json["Type"].is_string() ||
					!json.contains("Components") || !json["Components"].is_array())
				{
					outError = "TypeまたはComponentsが不正です: " + filePath;
					return false;
				}

				ActorArchetypeDefinition definition{};
				definition.id = json.value("Id", std::filesystem::path(filePath).stem().string());
				definition.displayName = json.value("DisplayName", definition.id);
				definition.description = json.value("Description", std::string{});
				definition.type = json["Type"].get<std::string>();
				if (!ActorFactory::IsRegistered(definition.type))
				{
					outError = "未登録のActor Typeです: " + definition.type;
					return false;
				}

				std::unordered_set<std::string> uniqueComponents;
				for (const auto& componentJson : json["Components"])
				{
					if (!componentJson.is_string())
					{
						outError = "ComponentsにはComponentクラス名の文字列だけを指定してください。";
						return false;
					}
					const std::string componentClass = componentJson.get<std::string>();
					if (componentClass.empty() || !uniqueComponents.insert(componentClass).second)
					{
						outError = "空または重複したComponent指定があります: " + componentClass;
						return false;
					}
					definition.components.push_back(componentClass);
				}

				outDefinition = std::move(definition);
				outError.clear();
				return true;
			}
			catch (const std::exception& exception)
			{
				outError = std::string("Archetype読込例外: ") + exception.what();
				return false;
			}
			catch (...)
			{
				outError = "Archetype読込中に不明な例外が発生しました。";
				return false;
			}
		}
	}

	void ActorWorld::DrawActorPrefabSpawnImGui()
	{
#ifdef USE_IMGUI
		ActorArchetypeBrowserState& archetypeState = GetActorArchetypeBrowserState();
		if (!archetypeState.initialized) RefreshActorArchetypeFiles(archetypeState);

		ImGui::SeparatorText("構成済みキャラクター生成");
		if (ImGui::Button("構成定義を再読込")) RefreshActorArchetypeFiles(archetypeState);

		if (archetypeState.files.empty())
		{
			ImGui::TextDisabled("Resources/ActorArchetypes に構成定義がありません。");
		}
		else
		{
			if (archetypeState.selectedIndex < 0 || archetypeState.selectedIndex >= static_cast<int>(archetypeState.files.size())) archetypeState.selectedIndex = 0;
			ActorArchetypeDefinition selectedDefinition{};
			std::string definitionError;
			const bool definitionValid = LoadActorArchetypeDefinition(archetypeState.files[archetypeState.selectedIndex], selectedDefinition, definitionError);
			const std::string previewLabel = definitionValid ? selectedDefinition.displayName : std::filesystem::path(archetypeState.files[archetypeState.selectedIndex]).stem().string();

			if (ImGui::BeginCombo("生成する構成", previewLabel.c_str()))
			{
				for (int index = 0; index < static_cast<int>(archetypeState.files.size()); ++index)
				{
					ActorArchetypeDefinition definition{};
					std::string error;
					const bool valid = LoadActorArchetypeDefinition(archetypeState.files[index], definition, error);
					const std::string label = valid ? definition.displayName : std::filesystem::path(archetypeState.files[index]).stem().string();
					const bool selected = archetypeState.selectedIndex == index;
					if (ImGui::Selectable((label + "##" + std::to_string(index)).c_str(), selected)) archetypeState.selectedIndex = index;
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (definitionValid)
			{
				ImGui::Text("Actor Type: %s", selectedDefinition.type.c_str());
				if (!selectedDefinition.description.empty()) ImGui::TextWrapped("%s", selectedDefinition.description.c_str());
				if (ImGui::TreeNode("生成されるComponent"))
				{
					for (const std::string& componentClass : selectedDefinition.components) ImGui::BulletText("%s", componentClass.c_str());
					ImGui::TreePop();
				}

				if (ImGui::Button("この構成で生成"))
				{
					std::unique_ptr<Actor> actor = ActorFactory::CreateActor(selectedDefinition.type);
					if (!actor)
					{
						lastActorJsonSaveMessage_ = "Archetype生成失敗: Actor Typeを生成できません。";
					}
					else
					{
						actor->SetName(MakeUniqueActorName(selectedDefinition.displayName.empty() ? selectedDefinition.type : selectedDefinition.displayName));
						actor->InitializeForWorld(); // Actor固有の必須構成を先に生成し、Archetype側は不足Componentだけ補う。

						bool compositionSucceeded = true;
						std::string failedComponent;
						for (const std::string& componentClass : selectedDefinition.components)
						{
							if (actor->HasComponentClass(componentClass)) continue;
							ActorComponent* component = ComponentFactory::CreateComponent(actor.get(), componentClass);
							if (!component)
							{
								compositionSucceeded = false;
								failedComponent = componentClass;
								break;
							}
							if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component))
							{
								SceneComponent* root = actor->GetRootComponent();
								if (root && sceneComponent != root && !sceneComponent->GetParent()) sceneComponent->AttachTo(root); // 追加した表示・Camera系を既定でActor Rootへ接続する。
							}
						}

						if (!compositionSucceeded)
						{
							actor->FinalizeForWorld();
							lastActorJsonSaveMessage_ = "Archetype生成失敗: 未登録Component " + failedComponent;
						}
						else
						{
							actor->InitializeComponents();
							if (SceneComponent* root = actor->GetRootComponent())
							{
								root->SetLocalPosition(Vector3::Add(root->GetLocalPosition(), actorPrefabSpawnOffset_));
								root->RefreshWorldTransform();
							}
							Actor* spawnedActor = AddActorToWorld(std::move(actor), false);
							selectedActor_ = spawnedActor;
							selectedComponent_ = nullptr;
							EditorContext::GetInstance()->GetSelection().Clear();
							EditorContext::GetInstance()->MarkLevelDirty();
							lastActorJsonSaveMessage_ = spawnedActor
								? "構成済みActorを生成しました: " + selectedDefinition.displayName
								: "Archetype生成失敗: ActorWorldへ追加できませんでした。";
						}
					}
				}
			}
			else
			{
				ImGui::TextWrapped("定義エラー: %s", definitionError.c_str());
			}
		}

		if (ImGui::CollapsingHeader("詳細: 完成済みActor JSONを直接生成"))
		{
			constexpr size_t kPathBufferSize = 256;
			std::array<char, kPathBufferSize> buffer{};
			std::snprintf(buffer.data(), buffer.size(), "%s", actorPrefabPath_.c_str());
			if (ImGui::InputText("Prefab Path", buffer.data(), buffer.size())) actorPrefabPath_ = buffer.data();

			if (ImGui::Button("Spawn Actor From JSON"))
			{
				ActorSpawnOptions options;
				options.applySpawnOffset = true;
				options.spawnOffset = actorPrefabSpawnOffset_;
				options.disableAutoRegisterMainCamera = true;
				pendingSpawnFilePath_ = actorPrefabPath_;
				pendingSpawnOptions_ = options;
				hasPendingSpawnActor_ = true;
				lastActorJsonSaveMessage_ = "Spawn requested : " + pendingSpawnFilePath_;
			}

			ImGui::SameLine();
			if (ImGui::Button("Use TestActor")) actorPrefabPath_ = "Resources/ActorPrefabs/TestActor.json";
			ImGui::SameLine();
			if (ImGui::Button("Use TestGroundActor")) actorPrefabPath_ = "Resources/ActorPrefabs/TestGroundActor.json";

			DrawActorPrefabBrowserImGui();
			DrawActorPrefabSaveImGui();
		}

		if (!lastActorJsonSaveMessage_.empty()) ImGui::TextWrapped("%s", lastActorJsonSaveMessage_.c_str());
#endif // USE_IMGUI
	}

	void ActorWorld::RefreshActorPrefabFileList()
	{
		actorPrefabFiles_.clear();
		const std::filesystem::path prefabDirectoryPath{ actorPrefabDirectory_ };
		if (!std::filesystem::exists(prefabDirectoryPath))
		{
			lastActorJsonSaveMessage_ = "Prefab directory does not exist: " + actorPrefabDirectory_;
			return;
		}
		if (!std::filesystem::is_directory(prefabDirectoryPath))
		{
			lastActorJsonSaveMessage_ = "Prefab path is not a directory: " + actorPrefabDirectory_;
			return;
		}

		for (const auto& entry : std::filesystem::directory_iterator(prefabDirectoryPath))
		{
			if (!entry.is_regular_file()) continue;
			const std::filesystem::path filePath = entry.path();
			if (filePath.extension() != ".json") continue;
			actorPrefabFiles_.push_back(filePath.string());
		}
		std::sort(actorPrefabFiles_.begin(), actorPrefabFiles_.end());
		lastActorJsonSaveMessage_ = "Prefab list refreshed : " + std::to_string(actorPrefabFiles_.size()) + " files";
	}

	void ActorWorld::DrawActorPrefabBrowserImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::Button("Refresh Prefab List")) RefreshActorPrefabFileList();
		if (actorPrefabFiles_.empty())
		{
			ImGui::Text("No prefab json files.");
			return;
		}

		ImGui::SeparatorText("Prefab List");
		for (const std::string& prefabFilePath : actorPrefabFiles_)
		{
			const std::filesystem::path path{ prefabFilePath };
			const std::string fileName = path.filename().generic_string();
			const bool isSelected = actorPrefabPath_ == prefabFilePath;
			if (ImGui::Selectable(fileName.c_str(), isSelected))
			{
				actorPrefabPath_ = prefabFilePath;
				lastActorJsonSaveMessage_ = "Selected prefab : " + actorPrefabPath_;
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Delete Selected Prefab JSON")) ImGui::OpenPopup("Delete Prefab JSON?");
		if (ImGui::BeginPopupModal("Delete Prefab JSON?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Delete this prefab file?");
			ImGui::Text("%s", actorPrefabPath_.c_str());
			ImGui::Separator();
			if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
			{
				DeleteSelectedActorPrefabFile();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
#endif // USE_IMGUI
	}

	void ActorWorld::DrawActorPrefabSaveImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("Save Actor Prefab");
		constexpr size_t kPathBufferSize = 256;
		std::array<char, kPathBufferSize> buffer{};
		std::snprintf(buffer.data(), buffer.size(), "%s", actorPrefabSavePath_.c_str());
		if (ImGui::InputText("Save Prefab Path", buffer.data(), buffer.size())) actorPrefabSavePath_ = buffer.data();

		Actor* saveTargetActor = selectedActor_;
		if (!saveTargetActor && selectedComponent_) saveTargetActor = selectedComponent_->GetOwner();
		if (!saveTargetActor)
		{
			ImGui::Text("No selected Actor.");
			return;
		}

		ImGui::Text("Save Target : %s", saveTargetActor->GetName().c_str());
		if (ImGui::Button("Save Selected As Prefab"))
		{
			const bool result = ActorJsonSerializer::SaveActorToFile(*saveTargetActor, actorPrefabSavePath_);
			if (result)
			{
				lastActorJsonSaveMessage_ = "Saved prefab : " + actorPrefabSavePath_;
				RefreshActorPrefabFileList();
			}
			else
			{
				lastActorJsonSaveMessage_ = "Save Prefab failed : " + actorPrefabSavePath_;
			}
		}
#endif // USE_IMGUI
	}

	void ActorWorld::DeleteSelectedActorPrefabFile()
	{
		if (!IsValidActorPrefabJsonPath(actorPrefabPath_))
		{
			lastActorJsonSaveMessage_ = "Delete prefab failed : invalid path";
			return;
		}

		const std::filesystem::path deletePath{ actorPrefabPath_ };
		if (!std::filesystem::exists(deletePath))
		{
			lastActorJsonSaveMessage_ = "Delete prefab failed : file not found";
			RefreshActorPrefabFileList();
			return;
		}

		std::error_code ec;
		const bool removed = std::filesystem::remove(deletePath, ec);
		if (!removed || ec)
		{
			lastActorJsonSaveMessage_ = "Delete prefab failed : " + actorPrefabPath_;
			return;
		}

		lastActorJsonSaveMessage_ = "Deleted prefab : " + actorPrefabPath_;
		RefreshActorPrefabFileList();
		if (!actorPrefabFiles_.empty()) actorPrefabSavePath_ = actorPrefabFiles_.front();
		else actorPrefabPath_.clear();
	}

	bool ActorWorld::IsValidActorPrefabJsonPath(const std::string& filePath) const
	{
		if (filePath.empty()) return false;
		const std::filesystem::path prefabDirectoryPath = std::filesystem::weakly_canonical(std::filesystem::path(actorPrefabDirectory_));
		const std::filesystem::path targetPath = std::filesystem::weakly_canonical(std::filesystem::path(filePath));
		if (targetPath.extension() != ".json") return false;
		const std::string prefabDirectoryString = prefabDirectoryPath.generic_string();
		const std::string targetPathString = targetPath.generic_string();
		return targetPathString.find(prefabDirectoryString) == 0;
	}
} // namespace Ken4lowEngine
