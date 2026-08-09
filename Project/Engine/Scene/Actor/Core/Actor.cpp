#include "Actor.h"

#include <algorithm>
#include <array>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#include <typeinfo>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void Actor::Initialize()
	{
		InitializeComponents();
	}

	void Actor::InitializeForWorld()
	{
		Initialize();
		InitializeComponents(); // 派生側が基底呼び出しを忘れてもComponentの初期化を保証する。
	}

	void Actor::InitializeComponents()
	{
		for (auto& component : components_)
		{
			if (component) component->InitializeForWorld(); // JSON再読込後もComponentを二重初期化しない。
		}
	}

	void Actor::Update(float deltaTime)
	{
		if (!isActive_)
		{
			return; // 無効なActorはUpdate対象から外す
		}

		std::vector<ActorComponent*> updateComponents;

		for (auto& component : components_)
		{
			if (!component || !component->IsActiveInHierarchy())
			{
				continue; // 無効化されたComponentはUpdate対象から外す
			}

			updateComponents.push_back(component.get());
		}

		std::stable_sort(updateComponents.begin(), updateComponents.end(),
			[](const ActorComponent* a, const ActorComponent* b)
			{
				return a->GetUpdateOrder() < b->GetUpdateOrder(); // UpdateOrderの昇順でソートする
			});

		for (ActorComponent* component : updateComponents)
		{
			if (!component)
			{
				continue; // 無効化されたComponentはUpdate対象から外す
			}

			// Actorは更新順だけ管理し、処理内容はComponent側に任せる
			component->Update(deltaTime);
		}
	}

	void Actor::UpdateEditor(float deltaTime)
	{
		if (!isActive_) return;

		std::vector<ActorComponent*> updateComponents;
		for (auto& component : components_)
		{
			if (component && component->IsActiveInHierarchy()) updateComponents.push_back(component.get());
		}

		std::stable_sort(updateComponents.begin(), updateComponents.end(),
			[](const ActorComponent* lhs, const ActorComponent* rhs)
			{
				return lhs->GetUpdateOrder() < rhs->GetUpdateOrder();
			});

		for (ActorComponent* component : updateComponents) component->UpdateEditor(deltaTime);
	}

	void Actor::PostPhysicsUpdate(float deltaTime)
	{
		if (!isActive_)
		{
			return; // 無効なActorはPostPhysicsUpdate対象から外す
		}

		std::vector<ActorComponent*> updateComponents;

		for (auto& component : components_)
		{
			if (!component || !component->IsActiveInHierarchy())
			{
				continue; // 無効化されたComponentはPostPhysicsUpdate対象から外す
			}

			// 物理更新後のTransform反映処理をComponent側へ流す。
			updateComponents.push_back(component.get());
		}

		std::stable_sort(updateComponents.begin(), updateComponents.end(),
			[](const ActorComponent* a, const ActorComponent* b)
			{
				return a->GetUpdateOrder() < b->GetUpdateOrder(); // UpdateOrderの昇順でソートする
			});

		for (ActorComponent* component : updateComponents)
		{
			if (!component)
			{
				continue; // 無効化されたComponentはPostPhysicsUpdate対象から外す
			}

			component->PostPhysicsUpdate(deltaTime);
		}
	}

	void Actor::Draw()
	{
		if (!isActive_)
		{
			return; // 無効なActorはDraw対象から外す
		}

		std::vector<ActorComponent*> drawComponents;

		for (auto& component : components_)
		{
			if (!component || !component->IsActiveInHierarchy())
			{
				continue; // 無効化されたComponentはDraw対象から外す
			}

			// 描画を持つComponentだけがDrawを実装する
			drawComponents.push_back(component.get());
		}

		std::stable_sort(drawComponents.begin(), drawComponents.end(),
			[](const ActorComponent* a, const ActorComponent* b)
			{
				return a->GetDrawOrder() < b->GetDrawOrder(); // DrawOrderの昇順でソートする
			});

		for (ActorComponent* component : drawComponents)
		{
			if (!component)
			{
				continue; // 無効化されたComponentはDraw対象から外す
			}

			component->Draw();
		}
	}

	void Actor::DrawShadow()
	{
		if (!isActive_)
		{
			return; // 無効なActorはShadow描画対象から外す
		}

		std::vector<ActorComponent*> drawComponents;

		for (auto& component : components_)
		{
			if (!component || !component->IsActiveInHierarchy())
			{
				continue; // 無効化されたComponentはShadow描画対象から外す
			}

			drawComponents.push_back(component.get());
		}

		std::stable_sort(drawComponents.begin(), drawComponents.end(),
			[](const ActorComponent* a, const ActorComponent* b)
			{
				return a->GetDrawOrder() < b->GetDrawOrder(); // DrawOrderの昇順でソートする
			});

		for (ActorComponent* component : drawComponents)
		{
			if (!component)
			{
				continue; // 無効化されたComponentはShadow描画対象から外す
			}

			// 影を落とすComponentだけがShadow描画を実装する
			component->DrawShadow();
		}
	}

	void Actor::DrawImGui()
	{
#ifdef USE_IMGUI
		DrawInspectorImGui(); // 既存のDrawImGui呼び出しはDetails表示として扱う
#endif // USE_IMGUI
	}

	void Actor::DrawHierarchyImGui(Actor*& selectedActor, ActorComponent*& selectedComponent)
	{
#ifdef USE_IMGUI
		const std::string actorLabel =
			GetName().empty()
			? std::string(typeid(*this).name())
			: GetName();

		const std::string treeLabel = actorLabel + "##ActorHierarchy";

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_OpenOnDoubleClick |
			ImGuiTreeNodeFlags_SpanAvailWidth;

		if (selectedActor == this && selectedComponent == nullptr)
		{
			flags |= ImGuiTreeNodeFlags_Selected; // 選択中のActorはハイライト表示する
		}

		const bool opened = ImGui::TreeNodeEx(treeLabel.c_str(), flags);

		if (ImGui::IsItemClicked())
		{
			selectedComponent = nullptr; // Actor自体が選択された場合はComponent選択を解除する
			selectedActor = this;		 // 閉じたTreeNodeをクリックした場合もActorを選択する
		}

		if (opened)
		{
			DrawComponentHierarchyImGui(selectedActor, selectedComponent);
			ImGui::TreePop();
		}
#endif // USE_IMGUI
	}

	void Actor::DrawComponentHierarchyImGui(Actor*& selectedActor, ActorComponent*& selectedComponent)
	{
#ifdef USE_IMGUI
		SceneComponent* root = GetRootComponent();

		if (root)
		{
			ImGui::PushID(root); // RootComponentのポインタを使ってImGui IDの衝突を防ぐ
			root->DrawComponentHierarchyImGui(selectedActor, selectedComponent);
			ImGui::PopID();
		}

		for (auto& component : components_)
		{
			if (!component)
			{
				continue; // nullptrのComponentは無視する
			}

			SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
			if (sceneComponent)
			{
				continue; // SceneComponentはRootComponentの階層描画に含まれるため、ここでは描画しない
			}

			ImGui::PushID(component.get()); // Componentのポインタを使ってImGui IDの衝突を防ぐ

			const std::string label = component->GetName().empty()
				? std::string(typeid(*component).name())
				: component->GetName();

			const std::string treeLabel = label + "##ActorComponentHierarchy";

			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth;

			if (selectedComponent == component.get())
			{
				flags |= ImGuiTreeNodeFlags_Selected; // 選択中のComponentはハイライト表示する
			}

			ImGui::TreeNodeEx(treeLabel.c_str(), flags);

			if (ImGui::IsItemClicked())
			{
				selectedActor = nullptr;			 // 選択されたComponentの所有者をDetails表示対象として選択する
				selectedComponent = component.get(); // ActorComponentをDetails表示対象として選択する
			}

			ImGui::PopID();
		}
#endif // USE_IMGUI
	}

	void Actor::DrawInspectorImGui()
	{
#ifdef USE_IMGUI
		static Actor* editingTagActor = nullptr;
		static std::array<char, 128> newTagBuffer{};
		if (editingTagActor != this)
		{
			editingTagActor = this;
			newTagBuffer.fill('\0');
		}

		ImGui::SeparatorText("Actor");
		ImGui::Text("Component Count : %zu", components_.size());
		ImGui::Text("Root Component : %s", rootComponent_ ? rootComponent_->GetName().c_str() : "None");

		bool active = IsActive();
		if (ImGui::Checkbox("有効", &active))
		{
			SetActive(active); // Actor単位の有効状態を更新する
		}

		std::array<char, 128> layerBuffer{};
		std::snprintf(layerBuffer.data(), layerBuffer.size(), "%s", layer_.c_str());
		if (ImGui::InputText("レイヤー", layerBuffer.data(), layerBuffer.size()))
		{
			SetLayer(layerBuffer.data());
		}

		ImGui::SeparatorText("タグ");
		if (tags_.empty())
		{
			ImGui::Text("タグなし");
		}

		for (size_t index = 0; index < tags_.size(); ++index)
		{
			ImGui::PushID(static_cast<int>(index));
			ImGui::Text("%s", tags_[index].c_str());
			ImGui::SameLine();
			if (ImGui::Button("削除"))
			{
				RemoveTag(tags_[index]);
				ImGui::PopID();
				break; // 削除で配列が変わるため次フレームに描画を続ける
			}
			ImGui::PopID();
		}

		ImGui::InputText("追加タグ", newTagBuffer.data(), newTagBuffer.size());
		ImGui::SameLine();
		if (ImGui::Button("追加"))
		{
			AddTag(newTagBuffer.data());
			newTagBuffer.fill('\0');
		}
#endif // USE_IMGUI

	}

	void Actor::Finalize()
	{
		// Actorを跨ぐSceneComponent参照も含め、所有Componentを破棄する前に階層を解除する。
		for (auto& component : components_)
		{
			SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
			if (!sceneComponent) continue;

			const std::vector<SceneComponent*> children = sceneComponent->GetChildren();
			for (SceneComponent* child : children)
			{
				if (!child) continue;
				const bool belongsToAnotherActor = child->GetOwner() != this;
				const Vector3 worldPosition = child->GetWorldPosition();
				const Vector3 worldRotation = child->GetWorldRotation();
				const Vector3 worldScale = child->GetWorldScale();
				child->Detach();
				if (belongsToAnotherActor)
				{
					child->SetLocalPosition(worldPosition);
					child->SetLocalRotation(worldRotation);
					child->SetLocalScale(worldScale);
					child->RefreshWorldTransform(); // 親Actor削除後も子Actorの見た目位置を維持する。
				}
			}
			sceneComponent->Detach();
		}

		for (auto& component : components_)
		{
			if (!component) continue;
			component->FinalizeForWorld();
			component->SetOwner(nullptr); // Component破棄時に所有Actorへの参照を残さない。
		}

		components_.clear(); // ActorがComponentの寿命を管理するため、ここで破棄する
		rootComponent_ = nullptr;
		isPhysicsRegistered_ = false;
	}

	void Actor::FinalizeForWorld()
	{
		Finalize();
		if (!components_.empty()) Actor::Finalize(); // 派生側が基底呼び出しを忘れても所有参照を残さない。
	}

	void Actor::ToJson(nlohmann::json& outJson) const
	{
		outJson["Name"] = GetName();		// Actor名を保存する
		outJson["Class"] = GetClassTypeName();  // Actorの種類を保存する
		outJson["Active"] = IsActive(); // Actorの有効状態を保存する
		outJson["Layer"] = GetLayer(); // Actorの管理Layerを保存する
		outJson["Tags"] = tags_; // Actor検索用Tag一覧を保存する

		outJson["Components"] = nlohmann::json::array(); // Component一覧を保存するための配列を作成する

		for (const auto& component : components_)
		{
			if (!component)
			{
				continue; // nullptrのComponentは無視する
			}

			nlohmann::json componentJson;
			component->ToJson(componentJson);				// Componentの情報をJSONへ保存する
			outJson["Components"].push_back(componentJson); // Component情報を配列へ追加する
		}
	}

	void Actor::FromJson(const nlohmann::json& inJson)
	{
		isActive_ = true;
		tags_.clear();
		layer_ = "Default";

		if (inJson.contains("Name") && inJson["Name"].is_string())
		{
			SetName(inJson["Name"].get<std::string>()); // JSONに保存されたActor名を復元する。
		}
		if (inJson.contains("Active") && inJson["Active"].is_boolean())
		{
			SetActive(inJson["Active"].get<bool>()); // Actorの有効状態を復元する
		}
		if (inJson.contains("Layer") && inJson["Layer"].is_string())
		{
			SetLayer(inJson["Layer"].get<std::string>()); // Actorの管理Layerを復元する
		}
		if (inJson.contains("Tags") && inJson["Tags"].is_array())
		{
			for (const auto& tagJson : inJson["Tags"])
			{
				if (tagJson.is_string())
				{
					AddTag(tagJson.get<std::string>()); // 不正なTag要素は無視して復元する
				}
			}
		}
	}

	void Actor::ClearComponents()
	{
		Actor::Finalize(); // JSON再読込では派生Actor全体を終了せず、Componentの参照解除だけを共通経路で行う。
	}

	bool Actor::RemoveComponent(ActorComponent* component)
	{
		if (!component)
		{
			return false; // 無効なComponentは削除できない
		}

		if (component == rootComponent_)
		{
			return false; // RootComponent削除は今回は禁止する
		}

		// SceneComponentの場合は、親子関係を整理してから削除する
		if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component))
		{
			SceneComponent* parent = sceneComponent->GetParent();

			// 子Component一覧はAttachTo中に変化するのでコピーしてから処理する
			std::vector<SceneComponent*> children = sceneComponent->GetChildren();

			for (SceneComponent* child : children)
			{
				if (!child)
				{
					continue; // nullptrの子Componentは無視する
				}

				if (parent)
				{
					child->AttachTo(parent); // 親がいる場合は、子Componentを親に接続し直す
				}
				else if (rootComponent_ && rootComponent_ != child)
				{
					child->AttachTo(rootComponent_); // 親がいない場合は、子ComponentをRootComponentに接続する
				}
				else
				{
					child->Detach(); // 親もRootComponentもいない場合は、子Componentを切り離す
				}
			}

			sceneComponent->Detach(); // 削除するSceneComponentを親から切り離す
		}

		const auto removeIt = std::find_if(components_.begin(), components_.end(),
			[component](const std::unique_ptr<ActorComponent>& ownedComponent)
			{
				return ownedComponent.get() == component; // 指定されたComponentと一致する所有要素を探す
			});

		if (removeIt == components_.end())
		{
			return false; // 指定されたComponentが見つからなかった場合は終了処理を行わない
		}

		// Finalize直後にunique_ptrを破棄し、Component内部の所有リソースはRAIIで解放する。
		(*removeIt)->FinalizeForWorld();
		(*removeIt)->SetOwner(nullptr);
		components_.erase(removeIt);
		return true;
	}

	bool Actor::HasComponentClass(const std::string& className) const
	{
		for (const auto& component : components_)
		{
			if (!component)
			{
				continue; // nullptrのComponentは無視する
			}

			if (component->GetClassTypeName() == className)
			{
				return true; // 指定されたclass名のComponentが見つかった場合はtrueを返す
			}
		}

		return false;
	}

	void Actor::AddTag(const std::string& tag)
	{
		if (tag.empty() || HasTag(tag))
		{
			return; // 空Tagや重複Tagは追加しない
		}

		tags_.push_back(tag);
	}

	void Actor::RemoveTag(const std::string& tag)
	{
		std::erase(tags_, tag); // 存在しないTag指定なら何もしない
	}

	bool Actor::HasTag(const std::string& tag) const
	{
		return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
	}

	void Actor::SetLayer(const std::string& layer)
	{
		layer_ = layer.empty() ? "Default" : layer; // 空LayerはDefaultへ戻す
	}

	ActorComponent* Actor::FindComponentByName(std::string_view name)
	{
		for (auto& component : components_)
		{
			// Component名が一致した最初のComponentを返す
			if (component->GetName() == name)
			{
				return component.get();
			}
		}

		return nullptr; // 一致するComponentが見つからなかった場合はnullptrを返す
	}

	const ActorComponent* Actor::FindComponentByName(std::string_view name) const
	{
		for (auto& component : components_)
		{
			// Component名が一致した最初のComponentを返す
			if (component->GetName() == name)
			{
				return component.get();
			}
		}

		return nullptr; // 一致するComponentが見つからなかった場合はnullptrを返す
	}

} // namespace Ken4lowEngine
