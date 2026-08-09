#pragma once

#include "EditorCommandHistory.h"
#include "EditorContext.h"
#include "EditorPanelIds.h"
#include "EditorWindowManager.h"

#include <BaseScene.h>
#include <LightManager.h>
#include <SceneManager.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>World Outliner V2とDetailsを同じEditorSelectionへ接続します。</summary>
	class EditorHierarchyPanel
	{
	public:
		static EditorHierarchyPanel* GetInstance()
		{
			static EditorHierarchyPanel instance;
			return &instance;
		}

		void Draw()
		{
#ifdef USE_IMGUI
			CollectCurrentObjects();
			RefreshSelection();
			DrawWorldOutliner();
			DrawDetails();
#endif
		}

	private:
		EditorHierarchyPanel() = default;
		~EditorHierarchyPanel() = default;
		EditorHierarchyPanel(const EditorHierarchyPanel&) = delete;
		EditorHierarchyPanel& operator=(const EditorHierarchyPanel&) = delete;

#ifdef USE_IMGUI
		static constexpr const char* kActorDragPayload = "K4E_OUTLINER_ACTOR";

		static char ToLowerAscii(unsigned char character)
		{
			return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : static_cast<char>(character);
		}

		static bool ContainsCaseInsensitive(std::string_view text, std::string_view filter)
		{
			if (filter.empty()) return true;
			std::string loweredText(text);
			std::string loweredFilter(filter);
			std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char c) { return ToLowerAscii(c); });
			std::transform(loweredFilter.begin(), loweredFilter.end(), loweredFilter.begin(), [](unsigned char c) { return ToLowerAscii(c); });
			return loweredText.find(loweredFilter) != std::string::npos;
		}

		void CollectCurrentObjects()
		{
			objects_.clear();
			SceneManager* sceneManager = EditorWindowManager::GetInstance()->GetSceneManager();
			BaseScene* scene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
			if (scene) scene->CollectEditorObjects(objects_);
			std::stable_sort(objects_.begin(), objects_.end(), [](const EditorObjectInfo& lhs, const EditorObjectInfo& rhs)
				{
					if (lhs.parentId != rhs.parentId) return lhs.parentId < rhs.parentId;
					if (lhs.objectKind != rhs.objectKind) return lhs.objectKind == EditorObjectKind::Folder;
					if (lhs.sortOrder != rhs.sortOrder) return lhs.sortOrder < rhs.sortOrder;
					return lhs.displayName < rhs.displayName;
				});
		}

		EditorObjectInfo* FindObject(uint64_t objectId)
		{
			const auto found = std::find_if(objects_.begin(), objects_.end(), [objectId](const EditorObjectInfo& object)
				{
					return object.id == objectId;
				});
			return found != objects_.end() ? &*found : nullptr;
		}

		const EditorObjectInfo* FindObject(uint64_t objectId) const
		{
			const auto found = std::find_if(objects_.begin(), objects_.end(), [objectId](const EditorObjectInfo& object)
				{
					return object.id == objectId;
				});
			return found != objects_.end() ? &*found : nullptr;
		}

		void RefreshSelection()
		{
			EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
			if (!selection.HasSelection()) return;
			const EditorObjectInfo& selected = selection.GetSelected();
			const auto current = std::find_if(objects_.begin(), objects_.end(), [&selected](const EditorObjectInfo& object)
				{
					return object.id == selected.id && object.sceneName == selected.sceneName;
				});
			if (current != objects_.end()) selection.RefreshSelected(*current);
			else
			{
				CancelInspectorTransaction();
				selection.Clear();
			}
		}

		bool MatchesFilter(const EditorObjectInfo& object) const
		{
			const std::string_view filter(outlinerSearch_.data());
			return ContainsCaseInsensitive(object.displayName, filter) ||
				ContainsCaseInsensitive(object.typeName, filter) ||
				ContainsCaseInsensitive(object.sceneName, filter) ||
				ContainsCaseInsensitive(object.folderPath, filter);
		}

		bool HasMatchingDescendant(uint64_t objectId) const
		{
			for (const EditorObjectInfo& child : objects_)
			{
				if (child.parentId == objectId && (MatchesFilter(child) || HasMatchingDescendant(child.id))) return true;
			}
			return false;
		}

		bool HasChildren(uint64_t objectId) const
		{
			return std::any_of(objects_.begin(), objects_.end(), [objectId](const EditorObjectInfo& object)
				{
					return object.parentId == objectId;
				});
		}

		bool IsSelected(const EditorObjectInfo& object) const
		{
			const EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
			return selection.HasSelection() && selection.GetSelected().id == object.id && selection.GetSelected().sceneName == object.sceneName;
		}

		void BeginActorDragSource(const EditorObjectInfo& object)
		{
			if (object.objectKind != EditorObjectKind::Actor || !object.canReparent) return;
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				const uint64_t actorId = object.id;
				ImGui::SetDragDropPayload(kActorDragPayload, &actorId, sizeof(actorId));
				ImGui::Text("%s  %s", object.icon.c_str(), object.displayName.c_str());
				ImGui::TextDisabled("アクタまたはフォルダーへドロップして親子・分類を変更");
				ImGui::EndDragDropSource();
			}
		}

		void ApplyActorDrop(const EditorObjectInfo& target)
		{
			if (!ImGui::BeginDragDropTarget()) return;
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kActorDragPayload))
			{
				if (payload->DataSize == sizeof(uint64_t))
				{
					const uint64_t draggedId = *static_cast<const uint64_t*>(payload->Data);
					EditorObjectInfo* dragged = FindObject(draggedId);
					if (dragged && dragged->objectKind == EditorObjectKind::Actor && dragged->id != target.id)
					{
						if (target.objectKind == EditorObjectKind::Actor)
						{
							dragged->RequestReparent(target.id);
						}
						else if (target.objectKind == EditorObjectKind::Folder)
						{
							dragged->RequestReparent(0);
							dragged->SetFolder(target.folderPath);
						}
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		void DrawVisibilityButton(const EditorObjectInfo& object)
		{
			bool visible = true;
			if (!object.ReadVisible(visible)) return;
			const char* label = visible ? "V" : "H";
			if (ImGui::SmallButton(label)) object.WriteVisible(!visible);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(visible ? "Editor Viewportで非表示にする" : "Editor Viewportで表示する");
			ImGui::SameLine();
		}

		void DrawLockButton(const EditorObjectInfo& object)
		{
			bool locked = false;
			if (!object.ReadLocked(locked)) return;
			const char* label = locked ? "L" : "U";
			if (ImGui::SmallButton(label)) object.WriteLocked(!locked);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(locked ? "ロックを解除する" : "Viewport選択とTransform編集をロックする");
			ImGui::SameLine();
		}

		void DrawObjectNode(const EditorObjectInfo& object)
		{
			const std::string_view filter(outlinerSearch_.data());
			const bool filterActive = !filter.empty();
			if (filterActive && !MatchesFilter(object) && !HasMatchingDescendant(object.id)) return;

			ImGui::PushID(static_cast<int>(object.id & 0x7fffffff));
			DrawVisibilityButton(object);
			DrawLockButton(object);

			bool active = true;
			if (object.ReadActive(active))
			{
				if (ImGui::Checkbox("##OutlinerActive", &active)) object.WriteActive(active);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip(active ? "有効" : "無効");
				ImGui::SameLine();
			}

			const bool hasChildren = HasChildren(object.id);
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
			if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (IsSelected(object)) flags |= ImGuiTreeNodeFlags_Selected;
			if (object.objectKind == EditorObjectKind::Actor || object.objectKind == EditorObjectKind::Folder) flags |= ImGuiTreeNodeFlags_DefaultOpen;
			if (filterActive && hasChildren) ImGui::SetNextItemOpen(true, ImGuiCond_Always);

			const std::string label = object.icon + "  " + object.displayName + "##OutlinerNode";
			const bool opened = ImGui::TreeNodeEx(label.c_str(), flags);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				CommitInspectorTransaction();
				EditorContext::GetInstance()->GetSelection().Select(object);
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) object.RequestFocus();
			}

			BeginActorDragSource(object);
			if (object.objectKind == EditorObjectKind::Actor || object.objectKind == EditorObjectKind::Folder) ApplyActorDrop(object);

			if (ImGui::BeginPopupContextItem("##OutlinerContext"))
			{
				ImGui::TextDisabled("%s", object.typeName.c_str());
				if (object.canFocus && ImGui::MenuItem("選択へフォーカス", "F")) object.RequestFocus();
				if (object.canRename && ImGui::MenuItem("名前を変更"))
				{
					renameTargetId_ = object.id;
					std::snprintf(renameBuffer_.data(), renameBuffer_.size(), "%s", object.displayName.c_str());
					openRenamePopup_ = true;
				}
				if (object.canSetFolder && ImGui::MenuItem("フォルダーを設定..."))
				{
					folderTargetId_ = object.id;
					std::snprintf(folderBuffer_.data(), folderBuffer_.size(), "%s", object.folderPath.c_str());
					openFolderPopup_ = true;
				}
				if (object.canReparent && object.parentId != 0 && ImGui::MenuItem("ルートへ移動")) object.RequestReparent(0);
				ImGui::Separator();
				if (object.canDuplicate && ImGui::MenuItem("複製")) object.RequestDuplicate();
				if (object.canDelete && ImGui::MenuItem("削除")) object.RequestDelete();
				ImGui::EndPopup();
			}

			if (hasChildren && opened)
			{
				for (const EditorObjectInfo& child : objects_)
				{
					if (child.parentId == object.id) DrawObjectNode(child);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		void DrawRenamePopup()
		{
			if (openRenamePopup_)
			{
				ImGui::OpenPopup("名前を変更##OutlinerRename");
				openRenamePopup_ = false;
			}
			if (ImGui::BeginPopupModal("名前を変更##OutlinerRename", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::SetNextItemWidth(320.0f);
				ImGui::InputText("新しい名前", renameBuffer_.data(), renameBuffer_.size());
				ImGui::Separator();
				if (ImGui::Button("変更", ImVec2(120.0f, 0.0f)))
				{
					if (EditorObjectInfo* target = FindObject(renameTargetId_)) target->Rename(renameBuffer_.data());
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
		}

		void DrawFolderPopup()
		{
			if (openFolderPopup_)
			{
				ImGui::OpenPopup("フォルダーを設定##OutlinerFolder");
				openFolderPopup_ = false;
			}
			if (ImGui::BeginPopupModal("フォルダーを設定##OutlinerFolder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextDisabled("例: Environment/Buildings");
				ImGui::SetNextItemWidth(360.0f);
				ImGui::InputText("フォルダーパス", folderBuffer_.data(), folderBuffer_.size());
				ImGui::Separator();
				if (ImGui::Button("設定", ImVec2(120.0f, 0.0f)))
				{
					if (EditorObjectInfo* target = FindObject(folderTargetId_)) target->SetFolder(folderBuffer_.data());
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("ルート", ImVec2(120.0f, 0.0f)))
				{
					if (EditorObjectInfo* target = FindObject(folderTargetId_)) target->SetFolder("");
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
		}

		void DrawSceneRootDropTarget()
		{
			ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 8.0f));
			if (!ImGui::BeginDragDropTarget()) return;
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kActorDragPayload))
			{
				if (payload->DataSize == sizeof(uint64_t))
				{
					const uint64_t draggedId = *static_cast<const uint64_t*>(payload->Data);
					if (EditorObjectInfo* dragged = FindObject(draggedId))
					{
						dragged->RequestReparent(0);
						dragged->SetFolder("");
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		void DrawWorldOutliner()
		{
			auto& windowState = EditorWindowManager::GetInstance()->GetWindowState();
			if (!windowState.showWorldOutliner) return;
			const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (ImGui::Begin(EditorPanelIds::WorldOutliner, &windowState.showWorldOutliner, windowFlags))
			{
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##OutlinerSearch", "アクタ、コンポーネント、フォルダーを検索...", outlinerSearch_.data(), outlinerSearch_.size());
				const size_t actorCount = static_cast<size_t>(std::count_if(objects_.begin(), objects_.end(), [](const EditorObjectInfo& object)
					{
						return object.objectKind == EditorObjectKind::Actor;
					}));
				ImGui::TextDisabled("Actors: %zu  Objects: %zu", actorCount, objects_.size());
				ImGui::Separator();
				if (ImGui::BeginChild("##WorldOutlinerBody", ImVec2(0.0f, 0.0f), false))
				{
					const char* sceneLabel = objects_.empty() ? "現在のシーン" : objects_.front().sceneName.c_str();
					if (ImGui::TreeNodeEx(sceneLabel, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
					{
						if (objects_.empty()) ImGui::TextDisabled("表示できるEditorオブジェクトがありません。");
						for (const EditorObjectInfo& object : objects_)
						{
							if (object.parentId == 0) DrawObjectNode(object);
						}
						DrawSceneRootDropTarget();
						ImGui::TreePop();
					}
				}
				ImGui::EndChild();
				DrawRenamePopup();
				DrawFolderPopup();
			}
			ImGui::End();
		}

		void DrawFallbackTransformInspector(const EditorObjectInfo& selected)
		{
			EditorTransform transform{};
			if (!selected.TryReadTransform(transform))
			{
				ImGui::TextDisabled("%s", selected.transformUnavailableReason.c_str());
				return;
			}
			bool changed = false;
			changed |= ImGui::DragFloat3("位置", &transform.position.x, 0.1f);
			changed |= ImGui::DragFloat3("回転", &transform.rotation.x, 0.01f);
			changed |= ImGui::DragFloat3("スケール", &transform.scale.x, 0.1f, 0.001f, 1000.0f);
			if (changed)
			{
				selected.WriteTransform(transform);
				EditorContext::GetInstance()->MarkLevelDirty();
			}
		}

		void CommitInspectorTransaction()
		{
			if (!inspectorTransactionActive_) return;
			if (!inspectorBeforeState_.empty() && inspectorBeforeState_ != inspectorAfterState_)
			{
				const EditorObjectInfo target = inspectorTarget_;
				EditorCommandHistory::GetInstance()->PushExecuted(std::make_unique<EditorStateCommand>(
					"プロパティ変更", inspectorBeforeState_, inspectorAfterState_,
					[target](std::string_view state)
					{
						target.RestoreState(state);
						EditorContext::GetInstance()->MarkLevelDirty();
					}));
			}
			CancelInspectorTransaction();
		}

		void CancelInspectorTransaction()
		{
			inspectorTransactionActive_ = false;
			inspectorTarget_ = {};
			inspectorBeforeState_.clear();
			inspectorAfterState_.clear();
		}

		void DrawInspectorWithHistory(const EditorObjectInfo& selected, const std::function<void()>& drawInspector)
		{
			if (!selected.canCaptureState)
			{
				drawInspector();
				return;
			}

			const std::string beforeFrame = selected.CaptureState();
			drawInspector();
			const EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
			if (!selection.HasSelection() || selection.GetSelected().id != selected.id || selection.GetSelected().sceneName != selected.sceneName)
			{
				CancelInspectorTransaction();
				return; // 削除や再構築で対象が変わったフレームは古いCallbackを呼ばない。
			}

			const std::string afterFrame = selected.CaptureState();
			const bool changed = !beforeFrame.empty() && beforeFrame != afterFrame;
			if (changed)
			{
				if (!inspectorTransactionActive_ || inspectorTarget_.id != selected.id)
				{
					CommitInspectorTransaction();
					inspectorTransactionActive_ = true;
					inspectorTarget_ = selected;
					inspectorBeforeState_ = beforeFrame;
				}
				inspectorAfterState_ = afterFrame;
				EditorContext::GetInstance()->MarkLevelDirty();
			}
			if (inspectorTransactionActive_ && inspectorTarget_.id == selected.id && !ImGui::IsAnyItemActive()) CommitInspectorTransaction();
		}

		void DrawDetails()
		{
			auto& windowState = EditorWindowManager::GetInstance()->GetWindowState();
			if (!windowState.showDetails) return;
			const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (ImGui::Begin(EditorPanelIds::Details, &windowState.showDetails, windowFlags))
			{
				if (ImGui::BeginChild("##DetailsBody", ImVec2(0.0f, 0.0f), false))
				{
					EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
					if (!selection.HasSelection())
					{
						CommitInspectorTransaction();
						ImGui::TextDisabled("オブジェクトが選択されていません。");
					}
					else
					{
						const EditorObjectInfo selected = selection.GetSelected();
						ImGui::PushID(static_cast<int>(selected.id & 0x7fffffff));
						ImGui::Text("%s  %s", selected.icon.c_str(), selected.displayName.c_str());
						ImGui::TextDisabled("%s", selected.typeName.c_str());

						if (selected.canFocus && ImGui::Button("選択へフォーカス (F)")) selected.RequestFocus();

						if (selected.canRename)
						{
							std::array<char, 256> nameBuffer{};
							std::snprintf(nameBuffer.data(), nameBuffer.size(), "%s", selected.displayName.c_str());
							ImGui::SetNextItemWidth(-1.0f);
							if (ImGui::InputText("名前##SelectedObjectName", nameBuffer.data(), nameBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) selected.Rename(nameBuffer.data());
						}

						bool active = true;
						if (selected.ReadActive(active) && ImGui::Checkbox("有効##SelectedObjectActive", &active)) selected.WriteActive(active);
						bool visible = true;
						if (selected.ReadVisible(visible) && ImGui::Checkbox("Editorで表示##SelectedObjectVisible", &visible)) selected.WriteVisible(visible);
						bool locked = false;
						if (selected.ReadLocked(locked) && ImGui::Checkbox("Editorでロック##SelectedObjectLocked", &locked)) selected.WriteLocked(locked);

						if (selected.canSetFolder)
						{
							std::array<char, 256> folderBuffer{};
							std::snprintf(folderBuffer.data(), folderBuffer.size(), "%s", selected.folderPath.c_str());
							ImGui::SetNextItemWidth(-1.0f);
							if (ImGui::InputText("フォルダー##SelectedObjectFolder", folderBuffer.data(), folderBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) selected.SetFolder(folderBuffer.data());
						}

						if (ImGui::CollapsingHeader("基本情報##SelectedObjectInfo", ImGuiTreeNodeFlags_DefaultOpen))
						{
							ImGui::Text("種類: %s", selected.typeName.c_str());
							ImGui::Text("シーン: %s", selected.sceneName.c_str());
							if (!selected.folderPath.empty()) ImGui::Text("フォルダー: %s", selected.folderPath.c_str());
							ImGui::TextDisabled("Editor ID: %llu", static_cast<unsigned long long>(selected.id));
							if (!selected.inspectorHint.empty()) ImGui::TextDisabled("%s", selected.inspectorHint.c_str());
						}

						ImGui::Separator();
						DrawInspectorWithHistory(selected, [&selected]()
							{
								if (selected.drawInspector)
								{
									ImGui::PushID("SelectedObjectInspector");
									selected.drawInspector();
									ImGui::PopID();
									return;
								}
								switch (selected.inspectorType)
								{
								case EditorInspectorType::Transform: EditorHierarchyPanel::GetInstance()->DrawFallbackTransformInspector(selected); break;
								case EditorInspectorType::PunctualLights: LightManager::GetInstance()->DrawPunctualLightsInspector(); break;
								default: ImGui::TextDisabled("このオブジェクトの詳細表示は未実装です。"); break;
								}
							});
						ImGui::PopID();
					}
				}
				ImGui::EndChild();
			}
			ImGui::End();
		}
#endif

		std::vector<EditorObjectInfo> objects_;
		std::array<char, 128> outlinerSearch_{};
		std::array<char, 256> renameBuffer_{};
		std::array<char, 256> folderBuffer_{};
		uint64_t renameTargetId_ = 0;
		uint64_t folderTargetId_ = 0;
		bool openRenamePopup_ = false;
		bool openFolderPopup_ = false;
		bool inspectorTransactionActive_ = false;
		EditorObjectInfo inspectorTarget_{};
		std::string inspectorBeforeState_;
		std::string inspectorAfterState_;
	};
} // namespace Ken4lowEngine
