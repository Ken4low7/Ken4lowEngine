#pragma once
#include "SceneComponent.h"
#include "ComponentProperty.h"
#include "MaterialBinding.h"
#include <InstancedObject3DRenderer.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   Actorにインスタンシング描画機能を追加するComponentクラス
	/// -------------------------------------------------------------
	class InstancedModelComponent : public SceneComponent
	{
	public:
		using InstanceTransform = InstancedObject3DRenderer::InstanceTransform;

		void Initialize() override;
		void Update(float deltaTime) override;
		void UpdateEditor(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void Draw() override;
		void DrawShadow() override
		{
			if (visible_ && IsCastShadowEnabled() && renderer_) renderer_->DrawShadow();
		}
		bool SupportsShadowCasting() const override { return true; }
		bool SupportsEditorObjectId() const override { return true; }
		void DrawEditorObjectId(uint32_t baseObjectId) override
		{
			if (visible_ && IsActiveInHierarchy() && renderer_)
			{
				renderer_->DrawEditorObjectId(baseObjectId); // baseId + SV_InstanceIDで各Instanceを個別選択する。
			}
		}
		void DrawEditorInstanceObjectId(size_t instanceIndex, uint32_t objectId)
		{
			if (visible_ && IsActiveInHierarchy() && renderer_)
			{
				renderer_->DrawEditorInstanceObjectId(instanceIndex, objectId);
			}
		}
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override { return "InstancedModelComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SetModelPath(std::string_view modelPath);
		void SetInstanceCount(int instanceCount);
		void SetSpacing(float spacing);
		void SetInstanceScale(const Vector3& scale);
		void SetVisible(bool visible) { visible_ = visible; }
		void SetMaterialAssetId(std::string_view assetId);
		void SetMaterialOverrideEnabled(bool enabled);
		const MaterialBinding& GetMaterialBinding() const { return materialBinding_; }
		std::vector<ComponentProperty> CreateProperties(bool includeModelPath = true);

		size_t GetEditableInstanceCount() const { return instanceTransforms_.size(); }
		bool GetInstanceLocalTransform(size_t instanceIndex, InstanceTransform& outTransform) const;
		bool GetInstanceWorldTransform(size_t instanceIndex, InstanceTransform& outTransform) const;
		bool SetInstanceLocalTransform(size_t instanceIndex, const InstanceTransform& transform);
		bool SetInstanceWorldTransform(size_t instanceIndex, const InstanceTransform& transform);

	private:
		void RebuildInstances();
		bool RebuildRenderer();
		void RequestRebuild();
		void RequestLayoutRebuild();
		void EnsureInstanceLayout();
		void UpdateInstanceRenderData(float deltaTime, bool editorOnly);
		void ApplyMaterialBinding();
		void RefreshSharedMaterialBinding();
		void DrawMaterialBindingImGui();

		std::unique_ptr<InstancedObject3DRenderer> renderer_;
		std::string modelPath_;
		std::string rendererStatus_ = "Empty";
		MaterialBinding materialBinding_{};
		std::string materialBindingStatus_ = "モデル既定Materialを使用中";
		uint64_t materialRepositoryRevision_ = 0;

		int instanceCount_ = 100;
		float spacing_ = 2.0f;
		Vector3 instanceScale_{ 1.0f, 1.0f, 1.0f };
		bool visible_ = true;
		std::vector<InstanceTransform> instanceTransforms_{}; // Component基準のLocal TransformをInstanceごとに保持する。

		bool isRebuildRequested_ = true;
		bool isLayoutRebuildRequested_ = true;
		bool isInitializedRenderer_ = false;
		bool hasInitialized_ = false;
		Vector3 lastWorldPosition_{};
		Vector3 lastWorldRotation_{};
		Vector3 lastWorldScale_{};
		bool hasLastWorldTransform_ = false;
	};
} // namespace Ken4lowEngine
