#pragma once
#include "SceneComponent.h"
#include "Object3D.h"
#include "ComponentProperty.h"
#include "MaterialBinding.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	class Camera;
	class ForwardRenderQueue;

	/// -------------------------------------------------------------
	/// Actorに3Dモデル描画機能を追加するComponentクラス。
	/// -------------------------------------------------------------
	class ModelComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void UpdateEditor(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void Draw() override;
		void DrawShadow() override;
		void DrawReflectionCapture();
		BoundingSphere GetReflectionCaptureBounds() const
		{
			return object3D_ ? object3D_->GetWorldBoundsForCulling() : BoundingSphere{};
		}
		bool HasReflectionCaptureBounds() const
		{
			return object3D_ && object3D_->HasWorldBoundsForCulling(); // Planar Captureの簡易Clip判定は既存Culling Boundsを再利用する。
		}
		bool SupportsShadowCasting() const override { return true; }
		bool SupportsEditorObjectId() const override { return true; }
		void DrawEditorObjectId(uint32_t objectId) override
		{
			if (visible_ && IsActiveInHierarchy() && object3D_)
			{
				object3D_->DrawEditorObjectId(objectId);
			}
		}
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override { return "ModelComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SetModelPath(std::string_view modelPath);
		void SetVisible(bool visible) { visible_ = visible; }
		void SetCamera(Camera* camera);
		void SetMaterialAssetId(std::string_view assetId);
		void SetMaterialOverrideEnabled(bool enabled);
		const MaterialBinding& GetMaterialBinding() const { return materialBinding_; }
		std::vector<ComponentProperty> CreateProperties(bool includeModelPath = true);
		bool SubmitForwardOpaque(ForwardRenderQueue& queue);
		bool SubmitForwardMasked(ForwardRenderQueue& queue);
		bool SubmitForwardTransparent(ForwardRenderQueue& queue);
		bool SubmitForwardAdditive(ForwardRenderQueue& queue);

	private:
		bool SubmitForwardBucket(ForwardRenderQueue& queue, MaterialBlendMode expectedBlendMode);
		void PrepareForCurrentRenderView();
		void DrawWithReflectionBinding();
		void SyncTransformToObject3D();
		void ApplyMaterialBinding();
		void RefreshSharedMaterialBinding();
		void DrawMaterialBindingImGui();

		std::unique_ptr<Object3D> object3D_;
		std::string modelPath_;
		Camera* camera_ = nullptr;
		bool visible_ = true;
		MaterialBinding materialBinding_{};
		std::string materialBindingStatus_ = "モデル既定Materialを使用中";
		uint64_t materialRepositoryRevision_ = 0;
		uint64_t lastForwardQueueSerial_ = 0; // 同じActorWorld描画内でQueue実行後に通常Drawが二重描画しないための印。
	};
} // namespace Ken4lowEngine
