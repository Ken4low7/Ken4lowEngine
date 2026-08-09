#include "HumanoidVisualComponent.h"

#include "ComponentProperty.h"
#include "MaterialRepository.h"

#include <algorithm>
#include <exception>
#include <unordered_map>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		template<class Func>
		void ForEachVisualPart(HumanoidVisualComponent::BodyPart& body, std::vector<HumanoidVisualComponent::BodyPart>& parts, Func&& func)
		{
			if (!body.id.empty()) func(body);
			for (auto& part : parts) func(part);
		}

		template<class Func>
		void ForEachVisualPart(const HumanoidVisualComponent::BodyPart& body, const std::vector<HumanoidVisualComponent::BodyPart>& parts, Func&& func)
		{
			if (!body.id.empty()) func(body);
			for (const auto& part : parts) func(part);
		}
	}

	void HumanoidVisualComponent::Initialize()
	{
		SceneComponent::Initialize();

		std::string loadError;
		if (definition_.GetParts().empty())
		{
			HumanoidDefinition loadedDefinition;
			if (!definitionPath_.empty() && loadedDefinition.LoadFromFile(definitionPath_, &loadError)) definition_ = std::move(loadedDefinition);
			else definition_ = HumanoidDefinition::CreateDefault();
		}

		if (!BuildBodyHierarchy(&loadError)) statusMessage_ = loadError;
	}

	void HumanoidVisualComponent::Update(float deltaTime)
	{
		ProcessDeferredDefinitionRequests();
		SceneComponent::Update(deltaTime);
		RefreshSharedMaterialBinding();
		UpdateHierarchy();
	}

	void HumanoidVisualComponent::UpdateEditor(float deltaTime)
	{
		ProcessDeferredDefinitionRequests();
		SceneComponent::UpdateEditor(deltaTime);
		RefreshSharedMaterialBinding();
		UpdateHierarchy();
	}

	void HumanoidVisualComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		RefreshWorldTransform();
		RefreshSharedMaterialBinding();
		UpdateHierarchy();
	}

	void HumanoidVisualComponent::Draw()
	{
		ForEachVisualPart(body_, parts_, [](const BodyPart& part)
			{
				if (part.visible && part.active && part.object) part.object->Draw();
			});
	}

	void HumanoidVisualComponent::DrawShadow()
	{
		if (!IsCastShadowEnabled()) return;
		ForEachVisualPart(body_, parts_, [](const BodyPart& part)
			{
				if (part.visible && part.active && part.object) part.object->DrawShadow();
			});
	}

	void HumanoidVisualComponent::DrawEditorObjectId(uint32_t objectId)
	{
		if (objectId == 0) return;
		ForEachVisualPart(body_, parts_, [objectId](const BodyPart& part)
			{
				if (part.visible && part.active && part.object) part.object->DrawEditorObjectId(objectId);
			});
	}

	void HumanoidVisualComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::SeparatorText("人型表示");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
		if (ImGui::Button("人型定義を再読み込み"))
		{
			requestDefinitionReload_ = true;
			statusMessage_ = "人型定義の再読み込みを予約しました。";
		}
		ImGui::SameLine();
		if (ImGui::Button("人型定義を保存"))
		{
			requestDefinitionSave_ = true;
			statusMessage_ = "人型定義の保存を予約しました。";
		}

		DrawMaterialBindingImGui();
		ImGui::SeparatorText("部位表示");
		ForEachVisualPart(body_, parts_, [this](BodyPart& part)
			{
				bool visible = part.visible;
				if (ImGui::Checkbox(part.id.c_str(), &visible)) SetPartVisible(part.id, visible);
			});
		ImGui::TextWrapped("状態: %s", statusMessage_.c_str());
#endif
	}

	void HumanoidVisualComponent::Finalize()
	{
		requestDefinitionReload_ = false;
		requestDefinitionSave_ = false;
		body_ = {};
		parts_.clear(); // 胴体と子部位のGPU描画オブジェクトをComponent所有権からまとめて解放する。
	}

	void HumanoidVisualComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		ComponentPropertyUtility::ToJson(const_cast<HumanoidVisualComponent*>(this)->CreateProperties(), outJson);
		outJson["Definition"] = definition_.ToJson();
		nlohmann::json partVisibility = nlohmann::json::object();
		for (const HumanoidPartDefinition& part : definition_.GetParts()) partVisibility[part.id] = part.visible;
		outJson["PartVisibility"] = std::move(partVisibility);
		if (materialBinding_.HasBinding()) outJson["Material"] = materialBinding_.ToJson();
	}

	void HumanoidVisualComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);

		HumanoidDefinition inlineDefinition;
		std::string definitionError;
		const auto definitionIt = inJson.find("Definition");
		const bool hasInlineDefinition = definitionIt != inJson.end() && inlineDefinition.FromJson(*definitionIt, &definitionError);

		HumanoidDefinition externalDefinition;
		const bool hasExternalDefinition = !definitionPath_.empty() && externalDefinition.LoadFromFile(definitionPath_, &definitionError);
		if (hasExternalDefinition) definition_ = std::move(externalDefinition);
		else if (hasInlineDefinition) definition_ = std::move(inlineDefinition);
		else if (definition_.GetParts().empty()) definition_ = HumanoidDefinition::CreateDefault();

		const auto visibilityIt = inJson.find("PartVisibility");
		if (visibilityIt != inJson.end() && visibilityIt->is_object())
		{
			for (auto item = visibilityIt->begin(); item != visibilityIt->end(); ++item)
			{
				HumanoidPartDefinition* part = definition_.FindPart(item.key());
				if (part && item.value().is_boolean()) part->visible = item.value().get<bool>();
			}
		}

		const auto materialIt = inJson.find("Material");
		if (materialIt != inJson.end() && materialIt->is_object()) materialBinding_.FromJson(*materialIt);
		else materialBinding_ = MaterialBinding{};

		if (IsInitialized() && !BuildBodyHierarchy(&definitionError)) statusMessage_ = definitionError;
	}

	bool HumanoidVisualComponent::SetDefinition(const HumanoidDefinition& definition, std::string* outError)
	{
		if (!definition.Validate(outError)) return false;
		definition_ = definition;
		if (!IsInitialized())
		{
			if (outError) outError->clear();
			return true;
		}
		return BuildBodyHierarchy(outError);
	}

	bool HumanoidVisualComponent::LoadDefinitionFromFile(std::string_view definitionPath, std::string* outError)
	{
		if (definitionPath.empty())
		{
			if (outError) *outError = "人型定義ファイルパスが空です。";
			return false;
		}

		HumanoidDefinition loadedDefinition;
		if (!loadedDefinition.LoadFromFile(definitionPath, outError)) return false;
		definitionPath_ = std::string(definitionPath);
		definition_ = std::move(loadedDefinition);
		if (IsInitialized() && !BuildBodyHierarchy(outError)) return false;
		statusMessage_ = "人型定義を読み込みました: " + definitionPath_;
		return true;
	}

	bool HumanoidVisualComponent::SaveDefinitionToFile(std::string_view definitionPath, std::string* outError) const
	{
		if (definitionPath.empty())
		{
			if (outError) *outError = "人型定義ファイルパスが空です。";
			return false;
		}
		return definition_.SaveToFile(definitionPath, outError);
	}

	HumanoidVisualComponent::BodyPart* HumanoidVisualComponent::FindPart(std::string_view partId)
	{
		if (body_.id == partId) return &body_;
		const auto partIt = std::find_if(parts_.begin(), parts_.end(), [partId](const BodyPart& part) { return part.id == partId; });
		return partIt != parts_.end() ? &(*partIt) : nullptr;
	}

	const HumanoidVisualComponent::BodyPart* HumanoidVisualComponent::FindPart(std::string_view partId) const
	{
		if (body_.id == partId) return &body_;
		const auto partIt = std::find_if(parts_.begin(), parts_.end(), [partId](const BodyPart& part) { return part.id == partId; });
		return partIt != parts_.end() ? &(*partIt) : nullptr;
	}

	bool HumanoidVisualComponent::SetPartVisible(std::string_view partId, bool visible)
	{
		BodyPart* part = FindPart(partId);
		HumanoidPartDefinition* partDefinition = definition_.FindPart(partId);
		if (!part || !partDefinition) return false;
		part->visible = visible;
		part->active = visible;
		partDefinition->visible = visible;
		return true;
	}

	void HumanoidVisualComponent::SetAllPartsVisible(bool visible)
	{
		ForEachVisualPart(body_, parts_, [this, visible](BodyPart& part)
			{
				part.visible = visible;
				part.active = visible;
				if (HumanoidPartDefinition* definition = definition_.FindPart(part.id)) definition->visible = visible;
			});
	}

	void HumanoidVisualComponent::SetSkinTexturePath(std::string_view texturePath)
	{
		skinTexturePath_ = std::string(texturePath);
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::ApplySkinToAllParts()
	{
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::ApplySkinToAllParts(std::string_view texturePath)
	{
		SetSkinTexturePath(texturePath);
	}

	void HumanoidVisualComponent::SetMaterialAssetId(std::string_view assetId)
	{
		materialBinding_.SetAssetId(assetId);
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::SetMaterialOverrideEnabled(bool enabled)
	{
		materialBinding_.SetUseOverride(enabled);
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::ApplyMaterialToAllParts()
	{
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::UpdateShadowMatrices(const Matrix4x4& lightViewProjection)
	{
		ForEachVisualPart(body_, parts_, [&lightViewProjection](const BodyPart& part)
			{
				if (part.active && part.object) part.object->UpdateShadowMatrix(lightViewProjection);
			});
	}

	bool HumanoidVisualComponent::BuildBodyHierarchy(std::string* outError)
	{
		if (!definition_.Validate(outError)) return false;
		const HumanoidPartDefinition* bodyDefinition = definition_.FindPart("Body");
		if (!bodyDefinition)
		{
			if (outError) *outError = "HumanoidDefinition requires a Body part.";
			return false;
		}

		body_ = {};
		parts_.clear();
		parts_.reserve(definition_.GetParts().size() > 0 ? definition_.GetParts().size() - 1 : 0);

		auto initializePart = [](BodyPart& part, const HumanoidPartDefinition& definition)
			{
				part.id = definition.id;
				part.parentId = definition.parentId;
				part.transform.translate_ = definition.localPosition;
				part.transform.rotate_ = definition.localRotation;
				part.transform.scale_ = definition.localScale;
				part.visible = definition.visible;
				part.active = definition.visible;
				try
				{
					part.object = std::make_unique<Object3D>();
					part.object->Initialize(definition.modelPath);
				}
				catch (const std::exception&)
				{
					part.object.reset();
				}
			};

		initializePart(body_, *bodyDefinition);
		body_.transform.parent_ = &visualRootTransform_;

		std::unordered_map<std::string, BodyPart*> partsById;
		partsById.emplace(body_.id, &body_);
		std::vector<bool> built(definition_.GetParts().size(), false);
		for (size_t index = 0; index < definition_.GetParts().size(); ++index)
		{
			if (definition_.GetParts()[index].id == body_.id) built[index] = true;
		}

		while (parts_.size() + 1 < definition_.GetParts().size())
		{
			bool builtAnyPart = false;
			for (size_t definitionIndex = 0; definitionIndex < definition_.GetParts().size(); ++definitionIndex)
			{
				if (built[definitionIndex]) continue;
				const HumanoidPartDefinition& partDefinition = definition_.GetParts()[definitionIndex];
				BodyPart* parentPart = nullptr;
				if (!partDefinition.parentId.empty())
				{
					const auto parentIt = partsById.find(partDefinition.parentId);
					if (parentIt == partsById.end()) continue;
					parentPart = parentIt->second;
				}

				parts_.push_back({});
				BodyPart& part = parts_.back();
				initializePart(part, partDefinition);
				part.transform.parent_ = parentPart ? &parentPart->transform : &visualRootTransform_;
				partsById.emplace(part.id, &part);
				built[definitionIndex] = true;
				builtAnyPart = true;
			}

			if (!builtAnyPart)
			{
				if (outError) *outError = "人型部位の親子順序を解決できませんでした。";
				body_ = {};
				parts_.clear();
				return false;
			}
		}

		ApplyAppearanceToAllParts();
		UpdateHierarchy();
		statusMessage_ = "人型部位を構築しました: " + std::to_string(GetTotalPartCount()) + " parts";
		if (outError) outError->clear();
		return true;
	}

	void HumanoidVisualComponent::ProcessDeferredDefinitionRequests()
	{
		if (requestDefinitionReload_)
		{
			requestDefinitionReload_ = false;
			std::string error;
			if (!LoadDefinitionFromFile(definitionPath_, &error)) statusMessage_ = error;
		}
		if (requestDefinitionSave_)
		{
			requestDefinitionSave_ = false;
			std::string error;
			if (!SaveDefinitionToFile(definitionPath_, &error)) statusMessage_ = error;
			else statusMessage_ = "人型定義を保存しました: " + definitionPath_;
		}
	}

	void HumanoidVisualComponent::UpdateHierarchy()
	{
		visualRootTransform_.translate_ = GetWorldPosition();
		visualRootTransform_.rotate_ = GetWorldRotation();
		visualRootTransform_.scale_ = GetWorldScale();
		visualRootTransform_.parent_ = nullptr;
		visualRootTransform_.Update();

		if (!body_.id.empty())
		{
			body_.transform.parent_ = &visualRootTransform_;
			body_.transform.Update();
			if (body_.object) body_.object->UpdateWithWorldMatrix(body_.transform.worldMatrix_);
		}

		for (BodyPart& part : parts_)
		{
			part.transform.Update();
			if (part.object) part.object->UpdateWithWorldMatrix(part.transform.worldMatrix_);
		}
	}

	void HumanoidVisualComponent::ApplyAppearanceToAllParts()
	{
		materialRepositoryRevision_ = MaterialRepository::GetInstance()->GetRevision();
		MaterialDesc resolvedMaterial{};
		const bool hasBinding = materialBinding_.HasBinding();
		const bool resolved = hasBinding && materialBinding_.Resolve(resolvedMaterial);

		ForEachVisualPart(body_, parts_, [this, resolved, &resolvedMaterial](BodyPart& part)
			{
				if (!part.object) return;
				if (resolved) part.object->ApplyMaterialDesc(resolvedMaterial);
				else part.object->ResetMaterialBinding();
				if (!skinTexturePath_.empty()) part.object->SetTextureForAll(skinTexturePath_);
			});

		if (!hasBinding) materialBindingStatus_ = "モデル既定Materialを使用中";
		else if (!resolved) materialBindingStatus_ = "MaterialAssetが見つからないためモデル既定へフォールバック";
		else materialBindingStatus_ = materialBinding_.IsUsingOverride()
			? "Component固有Material Overrideを使用中"
			: "共有MaterialAssetを使用中: " + materialBinding_.GetAssetId();
	}

	void HumanoidVisualComponent::RefreshSharedMaterialBinding()
	{
		if (materialBinding_.GetAssetId().empty() || materialBinding_.IsUsingOverride()) return;
		const uint64_t currentRevision = MaterialRepository::GetInstance()->GetRevision();
		if (currentRevision != materialRepositoryRevision_) ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::DrawMaterialBindingImGui()
	{
#ifdef USE_IMGUI
		if (Ken4lowEngine::DrawMaterialBindingImGui(materialBinding_, "HumanoidVisualComponent")) ApplyAppearanceToAllParts();
		ImGui::TextDisabled("状態: %s", materialBindingStatus_.c_str());
#endif
	}

	std::vector<ComponentProperty> HumanoidVisualComponent::CreateProperties()
	{
		return {
			{ "DefinitionPath", "人型定義パス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return definitionPath_; }, [this](const ComponentPropertyValue& value) { if (const std::string* path = std::get_if<std::string>(&value)) definitionPath_ = *path; } },
			{ "SkinTexturePath", "スキンテクスチャ", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return skinTexturePath_; }, [this](const ComponentPropertyValue& value) { if (const std::string* path = std::get_if<std::string>(&value)) SetSkinTexturePath(*path); } }
		};
	}
} // namespace Ken4lowEngine
