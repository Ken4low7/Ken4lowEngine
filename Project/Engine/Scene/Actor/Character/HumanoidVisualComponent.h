#pragma once

#include "ComponentProperty.h"
#include "HumanoidDefinition.h"
#include "MaterialBinding.h"
#include "Object3D.h"
#include "SceneComponent.h"
#include "WorldTransformEx.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// 人型の部位生成、Transform階層、描画、Material、JSONをActorから分離するComponent。
	class HumanoidVisualComponent : public SceneComponent
	{
	public:
		/// 生成済みの1部位と描画に必要な実行時状態を保持する。
		struct BodyPart
		{
			std::string id;
			std::string parentId;
			std::unique_ptr<Object3D> object;
			WorldTransformEx transform;
			bool visible = true;
			bool active = true;
		};

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void UpdateEditor(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void Draw() override;
		void DrawShadow() override;
		void DrawEditorObjectId(uint32_t objectId) override;
		void DrawImGui() override;
		void Finalize() override;

		bool SupportsShadowCasting() const override { return true; }
		bool SupportsEditorObjectId() const override { return true; }
		std::string GetClassTypeName() const override { return "HumanoidVisualComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		bool SetDefinition(const HumanoidDefinition& definition, std::string* outError = nullptr);
		const HumanoidDefinition& GetDefinition() const { return definition_; }
		void SetDefinitionPath(std::string_view definitionPath) { definitionPath_ = std::string(definitionPath); }
		const std::string& GetDefinitionPath() const { return definitionPath_; }
		bool LoadDefinitionFromFile(std::string_view definitionPath, std::string* outError = nullptr);
		bool SaveDefinitionToFile(std::string_view definitionPath, std::string* outError = nullptr) const;

		BodyPart* FindPart(std::string_view partId);
		const BodyPart* FindPart(std::string_view partId) const;

		/// 胴体はComponent内で安定したアドレスを持ち、旧Gameplay参照も同じ実体を利用する。
		BodyPart& GetBodyPart() { return body_; }
		const BodyPart& GetBodyPart() const { return body_; }

		/// 子部位だけを構築順で返す。部位実体の所有権はこのComponentだけが持つ。
		std::vector<BodyPart>& GetParts() { return parts_; }
		const std::vector<BodyPart>& GetParts() const { return parts_; }
		size_t GetTotalPartCount() const { return (body_.id.empty() ? 0u : 1u) + parts_.size(); }

		void UpdateShadowMatrices(const Matrix4x4& lightViewProjection);
		bool SetPartVisible(std::string_view partId, bool visible);
		void SetAllPartsVisible(bool visible);
		void SetSkinTexturePath(std::string_view texturePath);
		const std::string& GetSkinTexturePath() const { return skinTexturePath_; }
		void ApplySkinToAllParts();
		void ApplySkinToAllParts(std::string_view texturePath);
		void SetMaterialAssetId(std::string_view assetId);
		void SetMaterialOverrideEnabled(bool enabled);
		const MaterialBinding& GetMaterialBinding() const { return materialBinding_; }
		void ApplyMaterialToAllParts();
		const std::string& GetStatusMessage() const { return statusMessage_; }

	private:
		bool BuildBodyHierarchy(std::string* outError = nullptr);
		void ProcessDeferredDefinitionRequests();
		void UpdateHierarchy();
		void ApplyAppearanceToAllParts();
		void RefreshSharedMaterialBinding();
		void DrawMaterialBindingImGui();
		std::vector<ComponentProperty> CreateProperties();

	private:
		HumanoidDefinition definition_;
		std::string definitionPath_ = "Resources/JSON/Characters/DefaultHumanoid.json";
		BodyPart body_{}; // 胴体を安定アドレスで所有し、旧Gameplayと新Component経路で同じ実体を共有する。
		std::vector<BodyPart> parts_; // Head/Arm/Legなど胴体以外の子部位だけを所有する。
		WorldTransformEx visualRootTransform_;
		std::string skinTexturePath_;
		MaterialBinding materialBinding_{};
		std::string materialBindingStatus_ = "モデル既定Materialを使用中";
		std::string statusMessage_ = "人型定義は未初期化です。";
		uint64_t materialRepositoryRevision_ = 0;
		bool requestDefinitionReload_ = false;
		bool requestDefinitionSave_ = false;
	};
} // namespace Ken4lowEngine
