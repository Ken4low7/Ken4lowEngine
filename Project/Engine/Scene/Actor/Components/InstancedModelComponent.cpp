#define NOMINMAX
#include "InstancedModelComponent.h"
#include "AssetPathSelector.h"
#include "MaterialRepository.h"

#include <algorithm>
#include <cmath>
#include <exception>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		bool IsDifferentVector3(const Vector3& lhs, const Vector3& rhs)
		{
			constexpr float epsilon = 0.0001f;
			return std::abs(lhs.x - rhs.x) > epsilon ||
				std::abs(lhs.y - rhs.y) > epsilon ||
				std::abs(lhs.z - rhs.z) > epsilon;
		}

		Vector3 ReadVector3FromJson(const nlohmann::json& json, const char* key, const Vector3& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 3) return defaultValue;
			return { json[key][0].get<float>(), json[key][1].get<float>(), json[key][2].get<float>() };
		}

		Vector4 ReadVector4FromJson(const nlohmann::json& json, const char* key, const Vector4& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 4) return defaultValue;
			return { json[key][0].get<float>(), json[key][1].get<float>(), json[key][2].get<float>(), json[key][3].get<float>() };
		}

		float DivideScale(float value, float parentValue)
		{
			return std::abs(parentValue) > 0.0001f ? value / parentValue : value;
		}
	}

	void InstancedModelComponent::Initialize()
	{
		SceneComponent::Initialize();
		hasInitialized_ = true;
		EnsureInstanceLayout();
		RebuildRenderer();
	}

	void InstancedModelComponent::Update(float deltaTime)
	{
		UpdateInstanceRenderData(deltaTime, false);
	}

	void InstancedModelComponent::UpdateEditor(float deltaTime)
	{
		UpdateInstanceRenderData(deltaTime, true); // Edit/Pause中もGizmoで変更したInstanceだけGPU Bufferへ反映する。
	}

	void InstancedModelComponent::UpdateInstanceRenderData(float deltaTime, bool editorOnly)
	{
		if (editorOnly) SceneComponent::UpdateEditor(deltaTime);
		else SceneComponent::Update(deltaTime);

		RefreshSharedMaterialBinding();
		const Vector3 currentWorldPosition = GetWorldPosition();
		const Vector3 currentWorldRotation = GetWorldRotation();
		const Vector3 currentWorldScale = GetWorldScale();
		if (!hasLastWorldTransform_ ||
			IsDifferentVector3(currentWorldPosition, lastWorldPosition_) ||
			IsDifferentVector3(currentWorldRotation, lastWorldRotation_) ||
			IsDifferentVector3(currentWorldScale, lastWorldScale_))
		{
			RequestRebuild();
			lastWorldPosition_ = currentWorldPosition;
			lastWorldRotation_ = currentWorldRotation;
			lastWorldScale_ = currentWorldScale;
			hasLastWorldTransform_ = true;
		}
		if (isRebuildRequested_) RebuildInstances();
	}

	void InstancedModelComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		RefreshSharedMaterialBinding();
		const Vector3 currentWorldPosition = GetWorldPosition();
		const Vector3 currentWorldRotation = GetWorldRotation();
		const Vector3 currentWorldScale = GetWorldScale();
		if (!hasLastWorldTransform_ ||
			IsDifferentVector3(currentWorldPosition, lastWorldPosition_) ||
			IsDifferentVector3(currentWorldRotation, lastWorldRotation_) ||
			IsDifferentVector3(currentWorldScale, lastWorldScale_))
		{
			RequestRebuild();
			lastWorldPosition_ = currentWorldPosition;
			lastWorldRotation_ = currentWorldRotation;
			lastWorldScale_ = currentWorldScale;
			hasLastWorldTransform_ = true;
		}
		if (isRebuildRequested_) RebuildInstances();
	}

	void InstancedModelComponent::Draw()
	{
		if (visible_ && renderer_) renderer_->Draw();
	}

	void InstancedModelComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::SeparatorText("Instanced Model Component");
		ImGui::Text("現在のモデル: %s", modelPath_.empty() ? "未選択" : modelPath_.c_str());
		std::string selectedModelPath = modelPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##InstancedModelComponentModelPath", selectedModelPath, AssetType::Model))
		{
			SetModelPath(selectedModelPath);
		}
		ComponentPropertyUtility::DrawImGui(CreateProperties(false));
		DrawMaterialBindingImGui();
		ImGui::Text("Renderer: %s", renderer_ ? "Created" : "None");
		ImGui::Text("Model: %s", rendererStatus_.c_str());
		ImGui::Text("Editable Instances: %zu", instanceTransforms_.size());
		if (renderer_)
		{
			ImGui::Text("Visible: %zu / Total: %zu / Capacity: %zu",
				renderer_->GetVisibleInstanceCount(), renderer_->GetInstanceCount(), renderer_->GetMaxInstanceCount());
			if (renderer_->WasDrawSkippedByBudget()) ImGui::Text("Draw skipped by index budget.");
		}
#endif // USE_IMGUI
	}

	void InstancedModelComponent::Finalize()
	{
		if (renderer_)
		{
			renderer_->Finalize();
			renderer_.reset();
		}
		isInitializedRenderer_ = false;
		isRebuildRequested_ = true;
		isLayoutRebuildRequested_ = true;
		rendererStatus_ = modelPath_.empty() ? "Empty" : "Finalized";
		hasInitialized_ = false;
	}

	void InstancedModelComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		outJson["Class"] = GetClassTypeName();
		ComponentPropertyUtility::ToJson(const_cast<InstancedModelComponent*>(this)->CreateProperties(), outJson);
		if (materialBinding_.HasBinding()) outJson["Material"] = materialBinding_.ToJson();

		nlohmann::json instances = nlohmann::json::array();
		for (const InstanceTransform& transform : instanceTransforms_)
		{
			instances.push_back({
				{ "Position", { transform.position.x, transform.position.y, transform.position.z } },
				{ "Rotation", { transform.rotation.x, transform.rotation.y, transform.rotation.z } },
				{ "Scale", { transform.scale.x, transform.scale.y, transform.scale.z } },
				{ "Color", { transform.color.x, transform.color.y, transform.color.z, transform.color.w } }
				});
		}
		outJson["Instances"] = std::move(instances); // 個別Gizmo編集したLocal Transformを保存する。
	}

	void InstancedModelComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
		const auto materialIt = inJson.find("Material");
		if (materialIt != inJson.end() && materialIt->is_object()) materialBinding_.FromJson(*materialIt);
		else materialBinding_ = MaterialBinding{};

		instanceTransforms_.clear();
		const auto instancesIt = inJson.find("Instances");
		if (instancesIt != inJson.end() && instancesIt->is_array())
		{
			const size_t count = std::min<size_t>(instancesIt->size(), 30000u);
			instanceTransforms_.reserve(count);
			for (size_t i = 0; i < count; ++i)
			{
				const nlohmann::json& instanceJson = (*instancesIt)[i];
				if (!instanceJson.is_object()) continue;
				InstanceTransform transform{};
				transform.position = ReadVector3FromJson(instanceJson, "Position", {});
				transform.rotation = ReadVector3FromJson(instanceJson, "Rotation", {});
				transform.scale = ReadVector3FromJson(instanceJson, "Scale", { 1.0f, 1.0f, 1.0f });
				transform.color = ReadVector4FromJson(instanceJson, "Color", { 1.0f, 1.0f, 1.0f, 1.0f });
				instanceTransforms_.push_back(transform);
			}
			if (!instanceTransforms_.empty())
			{
				instanceCount_ = static_cast<int>(instanceTransforms_.size());
				isLayoutRebuildRequested_ = false;
			}
		}
		if (instanceTransforms_.empty()) isLayoutRebuildRequested_ = true;
		ApplyMaterialBinding();
		RequestRebuild();
	}

	void InstancedModelComponent::SetModelPath(std::string_view modelPath)
	{
		const std::string newModelPath(modelPath);
		if (modelPath_ == newModelPath)
		{
			if (!renderer_ && hasInitialized_)
			{
				RebuildRenderer();
				RequestRebuild();
			}
			return;
		}
		modelPath_ = newModelPath;
		if (renderer_)
		{
			renderer_->Finalize();
			renderer_.reset();
		}
		isInitializedRenderer_ = false;
		RebuildRenderer();
		RequestRebuild();
	}

	void InstancedModelComponent::SetInstanceCount(int instanceCount)
	{
		const int clampedCount = std::clamp(instanceCount, 1, 30000);
		if (instanceCount_ == clampedCount) return;
		instanceCount_ = clampedCount;
		RequestLayoutRebuild();
		if (renderer_ && static_cast<size_t>(instanceCount_) > renderer_->GetMaxInstanceCount()) RebuildRenderer();
	}

	void InstancedModelComponent::SetSpacing(float spacing)
	{
		spacing_ = std::max(spacing, 0.1f);
		RequestLayoutRebuild();
	}

	void InstancedModelComponent::SetInstanceScale(const Vector3& scale)
	{
		instanceScale_ = { std::max(scale.x, 0.01f), std::max(scale.y, 0.01f), std::max(scale.z, 0.01f) };
		RequestLayoutRebuild();
	}

	void InstancedModelComponent::SetMaterialAssetId(std::string_view assetId)
	{
		materialBinding_.SetAssetId(assetId);
		ApplyMaterialBinding();
	}

	void InstancedModelComponent::SetMaterialOverrideEnabled(bool enabled)
	{
		materialBinding_.SetUseOverride(enabled);
		ApplyMaterialBinding();
	}

	bool InstancedModelComponent::GetInstanceLocalTransform(size_t instanceIndex, InstanceTransform& outTransform) const
	{
		if (instanceIndex >= instanceTransforms_.size()) return false;
		outTransform = instanceTransforms_[instanceIndex];
		return true;
	}

	bool InstancedModelComponent::GetInstanceWorldTransform(size_t instanceIndex, InstanceTransform& outTransform) const
	{
		if (!GetInstanceLocalTransform(instanceIndex, outTransform)) return false;
		const Vector3 baseScale = GetWorldScale();
		outTransform.position += GetWorldPosition();
		outTransform.rotation += GetWorldRotation();
		outTransform.scale = {
			outTransform.scale.x * baseScale.x,
			outTransform.scale.y * baseScale.y,
			outTransform.scale.z * baseScale.z
		};
		return true;
	}

	bool InstancedModelComponent::SetInstanceLocalTransform(size_t instanceIndex, const InstanceTransform& transform)
	{
		if (instanceIndex >= instanceTransforms_.size()) return false;
		instanceTransforms_[instanceIndex] = transform;
		instanceTransforms_[instanceIndex].scale = {
			std::max(std::abs(transform.scale.x), 0.001f),
			std::max(std::abs(transform.scale.y), 0.001f),
			std::max(std::abs(transform.scale.z), 0.001f)
		};
		RequestRebuild();
		return true;
	}

	bool InstancedModelComponent::SetInstanceWorldTransform(size_t instanceIndex, const InstanceTransform& transform)
	{
		InstanceTransform local = transform;
		local.position -= GetWorldPosition();
		local.rotation -= GetWorldRotation();
		const Vector3 baseScale = GetWorldScale();
		local.scale = {
			DivideScale(transform.scale.x, baseScale.x),
			DivideScale(transform.scale.y, baseScale.y),
			DivideScale(transform.scale.z, baseScale.z)
		};
		return SetInstanceLocalTransform(instanceIndex, local);
	}

	void InstancedModelComponent::ApplyMaterialBinding()
	{
		if (!renderer_ || !isInitializedRenderer_) return;
		materialRepositoryRevision_ = MaterialRepository::GetInstance()->GetRevision();
		if (!materialBinding_.HasBinding())
		{
			renderer_->ResetMaterialBinding();
			materialBindingStatus_ = "モデル既定Materialを使用中";
			return;
		}
		MaterialDesc resolvedDesc{};
		if (!materialBinding_.Resolve(resolvedDesc))
		{
			renderer_->ResetMaterialBinding();
			materialBindingStatus_ = "MaterialAssetが見つからないためモデル既定へフォールバック";
			return;
		}
		renderer_->ApplyMaterialDesc(resolvedDesc);
		materialBindingStatus_ = materialBinding_.IsUsingOverride()
			? "Component固有Material Overrideを使用中"
			: "共有MaterialAssetを使用中: " + materialBinding_.GetAssetId();
	}

	void InstancedModelComponent::RefreshSharedMaterialBinding()
	{
		if (!renderer_ || materialBinding_.GetAssetId().empty() || materialBinding_.IsUsingOverride()) return;
		const uint64_t currentRevision = MaterialRepository::GetInstance()->GetRevision();
		if (currentRevision != materialRepositoryRevision_) ApplyMaterialBinding();
	}

	void InstancedModelComponent::DrawMaterialBindingImGui()
	{
#ifdef USE_IMGUI
		if (Ken4lowEngine::DrawMaterialBindingImGui(materialBinding_, "InstancedModelComponent")) ApplyMaterialBinding();
		ImGui::TextDisabled("状態: %s", materialBindingStatus_.c_str());
#endif // USE_IMGUI
	}

	void InstancedModelComponent::EnsureInstanceLayout()
	{
		const int count = std::clamp(instanceCount_, 1, 30000);
		if (!isLayoutRebuildRequested_ && instanceTransforms_.size() == static_cast<size_t>(count)) return;
		instanceTransforms_.clear();
		instanceTransforms_.reserve(static_cast<size_t>(count));
		const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
		const float centerOffset = static_cast<float>(columns - 1) * 0.5f;
		for (int i = 0; i < count; ++i)
		{
			const int x = i % columns;
			const int z = i / columns;
			InstanceTransform transform{};
			transform.position = {
				(static_cast<float>(x) - centerOffset) * spacing_,
				0.0f,
				(static_cast<float>(z) - centerOffset) * spacing_
			};
			transform.scale = instanceScale_;
			instanceTransforms_.push_back(transform);
		}
		isLayoutRebuildRequested_ = false;
	}

	void InstancedModelComponent::RebuildInstances()
	{
		if (!renderer_ || !isInitializedRenderer_) return;
		EnsureInstanceLayout();
		std::vector<InstanceTransform> worldTransforms;
		worldTransforms.reserve(instanceTransforms_.size());
		const Vector3 basePosition = GetWorldPosition();
		const Vector3 baseRotation = GetWorldRotation();
		const Vector3 baseScale = GetWorldScale();
		for (const InstanceTransform& local : instanceTransforms_)
		{
			InstanceTransform world = local;
			world.position += basePosition;
			world.rotation += baseRotation;
			world.scale = {
				local.scale.x * baseScale.x,
				local.scale.y * baseScale.y,
				local.scale.z * baseScale.z
			};
			worldTransforms.push_back(world);
		}
		renderer_->SetTransforms(worldTransforms);
		isRebuildRequested_ = false;
	}

	bool InstancedModelComponent::RebuildRenderer()
	{
		if (renderer_)
		{
			renderer_->Finalize();
			renderer_.reset();
		}
		isInitializedRenderer_ = false;
		if (modelPath_.empty())
		{
			rendererStatus_ = "Empty";
			return false;
		}
		if (!hasInitialized_)
		{
			rendererStatus_ = "Waiting Initialize";
			return false;
		}
		try
		{
			renderer_ = std::make_unique<InstancedObject3DRenderer>();
			renderer_->Initialize(modelPath_, static_cast<size_t>(std::max(instanceCount_, 1)));
			renderer_->SetDebugIndexBudget(50'000'000ull);
			isInitializedRenderer_ = true;
			rendererStatus_ = "Loaded";
			ApplyMaterialBinding();
			RebuildInstances();
			return true;
		}
		catch (const std::exception& exception)
		{
			renderer_.reset();
			rendererStatus_ = std::string("Failed: ") + exception.what();
		}
		catch (...)
		{
			renderer_.reset();
			rendererStatus_ = "Failed";
		}
		isInitializedRenderer_ = false;
		return false;
	}

	void InstancedModelComponent::RequestRebuild()
	{
		isRebuildRequested_ = true;
	}

	void InstancedModelComponent::RequestLayoutRebuild()
	{
		isLayoutRebuildRequested_ = true;
		RequestRebuild();
	}

	std::vector<ComponentProperty> InstancedModelComponent::CreateProperties(bool includeModelPath)
	{
		std::vector<ComponentProperty> properties = {
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) SetVisible(*typedValue); } },
			{ "InstanceCount", "インスタンス数", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return instanceCount_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<int>(&value)) SetInstanceCount(*typedValue); }, 1.0f, 30000.0f, 1.0f, true },
			{ "Spacing", "間隔", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return spacing_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) SetSpacing(*typedValue); }, 0.1f, 50.0f, 0.05f, true },
			{ "InstanceScale", "インスタンススケール", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return instanceScale_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector3>(&value)) SetInstanceScale(*typedValue); }, 0.01f, 100.0f, 0.05f, true }
		};
		if (includeModelPath)
		{
			properties.insert(properties.begin(),
				{ "ModelPath", "モデルパス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return modelPath_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) SetModelPath(*typedValue); } });
		}
		return properties;
	}
} // namespace Ken4lowEngine
