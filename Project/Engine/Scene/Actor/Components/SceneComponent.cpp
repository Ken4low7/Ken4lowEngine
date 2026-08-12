#include "SceneComponent.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		Vector3 ReadVector3(const nlohmann::json& json, const char* key, const Vector3& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 3)
			{
				return defaultValue;
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>()
			};
		}
	}

	void SceneComponent::Initialize()
	{
		MarkTransformDirty();
		RefreshWorldTransformHierarchy();
	}

	void SceneComponent::Update([[maybe_unused]] float deltaTime)
	{
		RefreshWorldTransformHierarchy();
	}

	void SceneComponent::UpdateEditor([[maybe_unused]] float deltaTime)
	{
		RefreshWorldTransformHierarchy(); // Clean階層は即時returnし、Editor停止中の無変更Transformを再計算しない。
	}

	std::size_t SceneComponent::RefreshWorldTransformHierarchy()
	{
		return UpdateWorldTransform();
	}

	void SceneComponent::MarkTransformDirty()
	{
		MarkWorldTransformDirtyRecursive();
		MarkSubtreeDirtyUpward(); // 子だけが変わった場合もRoot側のclean fast-pathを解除する。
	}

	void SceneComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("Scene Component");
		bool transformChanged = false;
		transformChanged |= ImGui::DragFloat3("Local Position", &localPosition_.x, 0.1f);
		transformChanged |= ImGui::DragFloat3("Local Rotation", &localRotation_.x, 0.1f);
		transformChanged |= ImGui::DragFloat3("Local Scale", &localScale_.x, 0.1f);
		if (transformChanged) MarkTransformDirty();

		ImGui::SeparatorText("World Transform");
		ImGui::Text("World Position : %.2f, %.2f, %.2f", worldPosition_.x, worldPosition_.y, worldPosition_.z);
		ImGui::Text("World Rotation : %.2f, %.2f, %.2f", worldRotation_.x, worldRotation_.y, worldRotation_.z);
		ImGui::Text("World Scale    : %.2f, %.2f, %.2f", worldScale_.x, worldScale_.y, worldScale_.z);
		ImGui::Text("Transform Dirty: %s", worldTransformDirty_ ? "Yes" : "No");
		ImGui::Text("Transform Revision: %llu", static_cast<unsigned long long>(worldTransformRevision_));
		ImGui::Text("Children Count : %zu", children_.size());
#endif // USE_IMGUI
	}

	void SceneComponent::DrawComponentHierarchyImGui(Actor*& selectedActor, ActorComponent*& selectedComponent)
	{
#ifdef USE_IMGUI
		const std::string label = GetName().empty() ? "Scene Component" : GetName();
		const std::string treeLabel = label + "##SceneComponentTree";
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (selectedComponent == this)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		const bool opened = ImGui::TreeNodeEx(treeLabel.c_str(), flags);
		if (ImGui::IsItemClicked())
		{
			selectedActor = nullptr;
			selectedComponent = this;
		}

		if (opened)
		{
			for (SceneComponent* child : children_)
			{
				if (!child)
				{
					continue;
				}
				ImGui::PushID(child);
				child->DrawComponentHierarchyImGui(selectedActor, selectedComponent);
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
#endif // USE_IMGUI
	}

	void SceneComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["Type"] = "SceneComponent";
		outJson["LocalPosition"] = { GetLocalPosition().x, GetLocalPosition().y, GetLocalPosition().z };
		outJson["LocalRotation"] = { GetLocalRotation().x, GetLocalRotation().y, GetLocalRotation().z };
		outJson["LocalScale"] = { GetLocalScale().x, GetLocalScale().y, GetLocalScale().z };
		const SceneComponent* parent = GetParent();
		const bool isInternalParent = parent && parent->GetOwner() == GetOwner();
		outJson["Parent"] = isInternalParent ? parent->GetName() : ""; // Actorを跨ぐ親子関係はPrefabではなくLevel側のParentIdへ保存する。
	}

	void SceneComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		SetLocalPosition(ReadVector3(inJson, "LocalPosition", GetLocalPosition()));
		SetLocalRotation(ReadVector3(inJson, "LocalRotation", GetLocalRotation()));
		SetLocalScale(ReadVector3(inJson, "LocalScale", GetLocalScale()));
		RefreshWorldTransform();
	}

	bool SceneComponent::IsActiveInHierarchy() const
	{
		if (!IsActive())
		{
			return false;
		}
		return parent_ ? parent_->IsActiveInHierarchy() : true;
	}

	void SceneComponent::AttachTo(SceneComponent* parent)
	{
		if (parent_ == parent || parent == this)
		{
			return;
		}

		for (SceneComponent* ancestor = parent; ancestor; ancestor = ancestor->parent_)
		{
			if (ancestor == this) return; // 自分の子孫へAttachしてTransform階層を循環させない。
		}

		Detach();
		parent_ = parent;
		if (parent_)
		{
			parent_->children_.push_back(this);
		}
		MarkTransformDirty();
		RefreshWorldTransformHierarchy();
	}

	void SceneComponent::Detach()
	{
		if (!parent_)
		{
			return;
		}
		parent_->RemoveChild(this);
		parent_ = nullptr;
		MarkTransformDirty();
		RefreshWorldTransformHierarchy();
	}

	std::size_t SceneComponent::UpdateWorldTransform()
	{
		const std::uint64_t parentRevision = parent_ ? parent_->worldTransformRevision_ : 0;
		const bool parentChanged = parent_ && lastParentWorldTransformRevision_ != parentRevision;
		if (!subtreeTransformDirty_ && !worldTransformDirty_ && !parentChanged)
		{
			return 0;
		}

		std::size_t recomputedCount = 0;
		const bool recomputeSelf = worldTransformDirty_ || parentChanged;
		if (recomputeSelf)
		{
			if (parent_)
			{
				worldPosition_ = parent_->worldPosition_ + localPosition_;
				worldRotation_ = parent_->worldRotation_ + localRotation_;
				worldScale_ = {
					parent_->worldScale_.x * localScale_.x,
					parent_->worldScale_.y * localScale_.y,
					parent_->worldScale_.z * localScale_.z
				};
			}
			else
			{
				worldPosition_ = localPosition_;
				worldRotation_ = localRotation_;
				worldScale_ = localScale_;
			}

			lastParentWorldTransformRevision_ = parentRevision;
			worldTransformDirty_ = false;
			++worldTransformRevision_;
			++recomputedCount;
		}

		for (SceneComponent* child : children_)
		{
			if (child)
			{
				recomputedCount += child->UpdateWorldTransform();
			}
		}

		subtreeTransformDirty_ = false;
		return recomputedCount;
	}

	void SceneComponent::MarkWorldTransformDirtyRecursive()
	{
		worldTransformDirty_ = true;
		subtreeTransformDirty_ = true;
		for (SceneComponent* child : children_)
		{
			if (child) child->MarkWorldTransformDirtyRecursive();
		}
	}

	void SceneComponent::MarkSubtreeDirtyUpward()
	{
		for (SceneComponent* component = this; component; component = component->parent_)
		{
			component->subtreeTransformDirty_ = true;
		}
	}

	void SceneComponent::RemoveChild(SceneComponent* child)
	{
		std::erase(children_, child);
	}
} // namespace Ken4lowEngine
